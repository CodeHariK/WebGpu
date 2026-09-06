/**
 * @file race_track_progress.cpp
 * @brief RaceTrack: per-body progress, laps, wrong-way detection, falling off and respawn.
 */
#include "race_track.h"
#include <godot_cpp/classes/rigid_body3d.hpp>
#include <godot_cpp/classes/time.hpp>

namespace godot {

// ---------------------------------------------------------------------------------------------
// Checkpoints and laps
// ---------------------------------------------------------------------------------------------

/// Direction of travel at gate p_index (global, unit).
Vector3 RaceTrack::_track_direction(int p_index) const {
	if (_gates.empty()) {
		return Vector3(0, 0, 1);
	}
	const int n = (int)_gates.size();
	return _gates[((p_index % n) + n) % n].frame.basis.get_column(2).normalized();
}

/// A body crossed gate p_index: only the expected gate counts; gate 0 starts the race, then closes laps.
void RaceTrack::_advance(
		Node3D *p_body,
		BodyState &p_state,
		int p_index
) {
	if (p_state.finished || p_index != p_state.next) {
		return;
	}
	const uint64_t now = Time::get_singleton()->get_ticks_msec();
	const int n = (int)_gates.size();

	if (p_index == 0) {
		if (!p_state.started) {
			p_state.started = true;
			p_state.race_start_msec = now;
			p_state.lap_start_msec = now;
		} else {
			p_state.lap += 1;
			emit_signal("lap_completed", p_body, p_state.lap, (int64_t)(now - p_state.lap_start_msec));
			p_state.lap_start_msec = now;
			if (laps > 0 && p_state.lap >= laps) {
				p_state.finished = true;
				emit_signal("race_finished", p_body, (int64_t)(now - p_state.race_start_msec));
			}
		}
	}
	p_state.next = (p_index + 1) % n;
	emit_signal("checkpoint_passed", p_body, p_index);
}

// ---------------------------------------------------------------------------------------------
// Per-frame watch: wrong way, falling off
// ---------------------------------------------------------------------------------------------

void RaceTrack::_watch_body(
		Node3D *p_body,
		BodyState &p_state,
		float p_delta
) {
	if (_gates.empty()) {
		return;
	}
	const int n = (int)_gates.size();
	const int prev = (p_state.next - 1 + n) % n;
	const Vector3 pos = p_body->get_global_position();

	// Fell off: below the last checkpoint by kill_depth (also after the finish line).
	if (kill_depth > 0.0f && pos.y < _gates[prev].frame.origin.y - kill_depth) {
		emit_signal("fell_off", p_body);
		if (auto_respawn) {
			respawn(p_body);
		}
		return;
	}
	if (p_state.finished) {
		return;
	}

	// Wrong way: moving against the segment prev → next for wrong_way_time.
	Vector3 velocity;
	if (RigidBody3D *rb = Object::cast_to<RigidBody3D>(p_body)) {
		velocity = rb->get_linear_velocity();
	}
	velocity.y = 0.0f;
	const float speed = velocity.length();
	bool wrong_now = false;
	if (speed >= wrong_way_min_speed && wrong_way_min_speed > 0.0f) {
		Vector3 segment = _gates[p_state.next].frame.origin - _gates[prev].frame.origin;
		segment.y = 0.0f;
		if (segment.length_squared() < 1e-6f) {
			segment = _track_direction(prev);
			segment.y = 0.0f;
		}
		const float cos_angle = velocity.normalized().dot(segment.normalized());
		wrong_now = cos_angle < Math::cos(Math::deg_to_rad(wrong_way_angle));
	}
	if (wrong_now) {
		p_state.wrong_timer += p_delta;
		if (!p_state.is_wrong && p_state.wrong_timer >= wrong_way_time) {
			p_state.is_wrong = true;
			emit_signal("wrong_way", p_body, true);
		}
	} else {
		p_state.wrong_timer = 0.0f;
		if (p_state.is_wrong) {
			p_state.is_wrong = false;
			emit_signal("wrong_way", p_body, false);
		}
	}
}

// ---------------------------------------------------------------------------------------------
// Respawn
// ---------------------------------------------------------------------------------------------

/// Upright frame respawn_back metres before the last passed gate, respawn_height above the deck, facing along the
/// track.
Transform3D RaceTrack::_respawn_frame(const BodyState &p_state) const {
	if (_gates.empty()) {
		return get_global_transform();
	}
	const int n = (int)_gates.size();
	const int prev = p_state.started ? (p_state.next - 1 + n) % n : 0;
	const Transform3D &g = _gates[prev].frame;
	Vector3 forward = g.basis.get_column(2);
	forward.y = 0.0f;
	forward = forward.length_squared() > 1e-8f ? forward.normalized() : Vector3(0, 0, 1);
	const Vector3 up(0, 1, 0);
	const Vector3 lateral = up.cross(forward).normalized();
	Transform3D t;
	t.basis = Basis(lateral, up, forward);
	t.origin = g.origin - forward * respawn_back + g.basis.get_column(1) * respawn_height;
	return t;
}

void RaceTrack::respawn(Node3D *p_body) {
	if (!p_body) {
		return;
	}
	const Transform3D t = get_respawn_transform(p_body);
	if (RigidBody3D *rb = Object::cast_to<RigidBody3D>(p_body)) {
		rb->set_linear_velocity(Vector3());
		rb->set_angular_velocity(Vector3());
	}
	p_body->set_global_transform(t);
	emit_signal("respawned", p_body);
}

// ---------------------------------------------------------------------------------------------
// Queries
// ---------------------------------------------------------------------------------------------

void RaceTrack::track_body(Node3D *p_body) {
	if (p_body && !_bodies.has(p_body->get_instance_id())) {
		_bodies[p_body->get_instance_id()] = BodyState();
	}
}

void RaceTrack::untrack_body(Node3D *p_body) {
	if (p_body) {
		_bodies.erase(p_body->get_instance_id());
	}
}

int RaceTrack::get_next_checkpoint(Node3D *p_body) const {
	if (!p_body || !_bodies.has(p_body->get_instance_id())) {
		return -1;
	}
	return _bodies[p_body->get_instance_id()].next;
}

int RaceTrack::get_lap(Node3D *p_body) const {
	if (!p_body || !_bodies.has(p_body->get_instance_id())) {
		return 0;
	}
	return _bodies[p_body->get_instance_id()].lap;
}

float RaceTrack::get_progress(Node3D *p_body) const {
	if (!p_body || _gates.empty() || !_bodies.has(p_body->get_instance_id())) {
		return 0.0f;
	}
	const BodyState &st = _bodies[p_body->get_instance_id()];
	const int n = (int)_gates.size();
	const int prev = (st.next - 1 + n) % n;
	const Vector3 a = _gates[prev].frame.origin;
	const Vector3 b = _gates[st.next].frame.origin;
	const Vector3 ab = b - a;
	float frac = 0.0f;
	if (ab.length_squared() > 1e-6f) {
		frac = CLAMP((p_body->get_global_position() - a).dot(ab) / ab.length_squared(), 0.0f, 1.0f);
	}
	const float passed = st.started ? (float)prev : 0.0f;
	return CLAMP((passed + frac) / (float)n, 0.0f, 1.0f);
}

uint64_t RaceTrack::get_race_time_msec(Node3D *p_body) const {
	if (!p_body || !_bodies.has(p_body->get_instance_id())) {
		return 0;
	}
	const BodyState &st = _bodies[p_body->get_instance_id()];
	if (!st.started) {
		return 0;
	}
	return Time::get_singleton()->get_ticks_msec() - st.race_start_msec;
}

Transform3D RaceTrack::get_respawn_transform(Node3D *p_body) const {
	BodyState fallback;
	const BodyState *st = &fallback;
	if (p_body && _bodies.has(p_body->get_instance_id())) {
		st = &_bodies[p_body->get_instance_id()];
	}
	return _respawn_frame(*st);
}

} // namespace godot
