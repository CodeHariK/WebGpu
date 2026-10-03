#include "jump_metrics.h"

#include "../player/celeste_controller.h"
#include "spring_character.h"

#include <godot_cpp/core/math.hpp>

#include <cmath>

namespace godot {

static float _safe(float p_v) { return p_v > 0.001f ? p_v : 0.001f; }

float JumpMetrics::launch_velocity() const { return 2.0f * height / _safe(time_to_peak); }
float JumpMetrics::rise_gravity() const { return 2.0f * height / (_safe(time_to_peak) * _safe(time_to_peak)); }
float JumpMetrics::fall_gravity() const { return 2.0f * height / (_safe(time_to_descent) * _safe(time_to_descent)); }

float JumpMetrics::_fall_time(float p_drop) const {
	const float g = fall_gravity();
	if (terminal_velocity <= 0.0f) {
		return std::sqrt(2.0f * p_drop / g);
	}
	// Accelerate to terminal speed, then fall at constant speed.
	const float t_cap = terminal_velocity / g;
	const float d_cap = 0.5f * g * t_cap * t_cap;
	if (p_drop <= d_cap) {
		return std::sqrt(2.0f * p_drop / g);
	}
	return t_cap + (p_drop - d_cap) / terminal_velocity;
}

float JumpMetrics::airtime(float p_dh) const {
	if (p_dh > height) {
		return -1.0f; // above the apex
	}
	return time_to_peak + _fall_time(height - p_dh);
}

float JumpMetrics::reach(
		float p_dh,
		bool p_sprint
) const {
	const float t = airtime(p_dh);
	return t < 0.0f ? 0.0f : t * (p_sprint ? sprint_speed : run_speed);
}

float JumpMetrics::double_apex() const {
	if (air_jumps <= 0) {
		return height;
	}
	return height * (1.0f + air_jump_mult * air_jump_mult); // apex + second jump height (v^2 scales)
}

float JumpMetrics::double_airtime(float p_dh) const {
	if (air_jumps <= 0) {
		return airtime(p_dh);
	}
	static const int SAMPLES = 32;
	const float second_rise = time_to_peak * air_jump_mult; // same up-gravity, scaled speed
	const float second_height = height * air_jump_mult * air_jump_mult;
	// The press must happen before the first arc lands (on the target, or on the take-off
	// level when the target is higher up).
	const float window = airtime(p_dh < 0.0f ? p_dh : 0.0f);
	float best = airtime(p_dh);
	for (int i = 1; i <= SAMPLES; ++i) {
		const float t1 = window * i / SAMPLES;
		const float drop = height_at(t1) + second_height - p_dh;
		if (drop >= 0.0f) {
			best = MAX(best, t1 + second_rise + _fall_time(drop));
		}
	}
	return best;
}

float JumpMetrics::double_reach(
		float p_dh,
		bool p_sprint
) const {
	const float t = double_airtime(p_dh);
	return t < 0.0f ? 0.0f : t * (p_sprint ? sprint_speed : run_speed);
}

float JumpMetrics::_fall_dist(float p_t) const {
	const float g = fall_gravity();
	if (terminal_velocity <= 0.0f || g * p_t <= terminal_velocity) {
		return 0.5f * g * p_t * p_t;
	}
	const float t_cap = terminal_velocity / g;
	return 0.5f * g * t_cap * t_cap + terminal_velocity * (p_t - t_cap);
}

float JumpMetrics::_arc_y(
		float p_v,
		float p_t
) const {
	const float gu = rise_gravity();
	const float t_rise = p_v / gu;
	if (p_t <= t_rise) {
		return p_v * p_t - 0.5f * gu * p_t * p_t;
	}
	return p_v * p_v / (2.0f * gu) - _fall_dist(p_t - t_rise);
}

float JumpMetrics::_arc_land(
		float p_v,
		float p_y0,
		float p_dh
) const {
	const float gu = rise_gravity();
	const float drop = p_y0 + p_v * p_v / (2.0f * gu) - p_dh;
	return drop < 0.0f ? -1.0f : p_v / gu + _fall_time(drop);
}

float JumpMetrics::full_reach(
		float p_dh,
		bool p_sprint
) const {
	const float run = p_sprint ? sprint_speed : run_speed;
	float best = double_reach(p_dh, p_sprint);
	if (!air_dash || dash_time <= 0.0f) {
		return best;
	}
	static const int N = 16;
	// Dash distance, plus the carry-over while air control brakes back down to run speed
	// (constant deceleration: (dash - run)^2 / 2a).
	float dash_d = dash_speed * dash_time;
	if (air_accel > 0.0f && dash_speed > run) {
		dash_d += (dash_speed - run) * (dash_speed - run) / (2.0f * air_accel);
	}
	const float v1 = launch_velocity();
	const float v2 = air_jumps > 0 ? v1 * air_jump_mult : -1.0f; // < 0: no air jump
	// Distance for a chain lasting `total` seconds, of which dash_time is spent dashing.
	auto dist = [&](float total) { return run * (total - dash_time) + dash_d; };

	// Press windows end when the current arc would reach the landing height (or the
	// take-off level when the target is higher: the floor would catch you first).
	const float floor_h = p_dh < 0.0f ? p_dh : 0.0f;
	const float w1 = _arc_land(v1, 0.0f, floor_h);
	for (int i = 1; i <= N; ++i) {
		const float t1 = w1 * i / N;
		const float y1 = _arc_y(v1, t1);

		// Jump -> dash -> fall (no double jump).
		if (y1 >= p_dh) {
			best = MAX(best, dist(t1 + dash_time + _fall_time(y1 - p_dh)));
		}
		if (v2 < 0.0f) {
			continue;
		}
		// Jump -> double (t1) -> dash (t2 later) -> fall.
		const float w2 = _arc_land(v2, y1, floor_h);
		for (int j = 1; j <= N && w2 > 0.0f; ++j) {
			const float t2 = w2 * j / N;
			const float y2 = y1 + _arc_y(v2, t2);
			if (y2 >= p_dh) {
				best = MAX(best, dist(t1 + t2 + dash_time + _fall_time(y2 - p_dh)));
			}
		}
		// Jump -> dash (t1) -> fall -> double (t2 later).
		const float w3 = _fall_time(MAX(y1 - floor_h, 0.0f));
		for (int j = 0; j <= N; ++j) {
			const float t2 = w3 * j / N;
			const float land = _arc_land(v2, y1 - _fall_dist(t2), p_dh);
			if (land > 0.0f) {
				best = MAX(best, dist(t1 + dash_time + t2 + land));
			}
		}
	}
	return best;
}

float JumpMetrics::height_at(float p_t) const {
	if (p_t <= time_to_peak) {
		return launch_velocity() * p_t - 0.5f * rise_gravity() * p_t * p_t;
	}
	const float tf = p_t - time_to_peak;
	const float g = fall_gravity();
	if (terminal_velocity <= 0.0f || g * tf <= terminal_velocity) {
		return height - 0.5f * g * tf * tf;
	}
	const float t_cap = terminal_velocity / g;
	return height - 0.5f * g * t_cap * t_cap - terminal_velocity * (tf - t_cap);
}

JumpMetrics::Band JumpMetrics::classify(
		float p_dx,
		float p_dh
) const {
	const float run = reach(p_dh);
	if (run > 0.0f) {
		const float ratio = p_dx / run;
		if (ratio <= COMFORT) {
			return BAND_COMFORT;
		}
		if (ratio <= PRECISE) {
			return BAND_PRECISE;
		}
		if (ratio <= 1.0f) {
			return BAND_LIMIT;
		}
		if (p_dx <= reach(p_dh, true)) {
			return BAND_SPRINT;
		}
	}
	if (air_jumps > 0 && p_dh <= double_apex() && p_dx <= double_reach(p_dh, true)) {
		return BAND_DOUBLE;
	}
	if (air_dash && p_dh <= double_apex() && p_dx <= full_reach(p_dh, true)) {
		return BAND_DASH;
	}
	return BAND_OUT;
}

bool JumpMetrics::from_node(
		Node *p_node,
		JumpMetrics &r_metrics
) {
	if (CelesteController *c = Object::cast_to<CelesteController>(p_node)) {
		r_metrics = c->get_jump_metrics();
		return true;
	}
	if (SpringCharacter *s = Object::cast_to<SpringCharacter>(p_node)) {
		r_metrics = s->get_jump_metrics();
		return true;
	}
	return false;
}

} // namespace godot
