#include "zoom.h"

#include <godot_cpp/core/math.hpp>

namespace godot {

void ViewZoom::scroll(double p_value) {
	base_ratio -= p_value * sensitivity;
	base_ratio = CLAMP(base_ratio, 0.0, 1.0);
}

void ViewZoom::add_base_ratio(double p_delta) {
	base_ratio += p_delta;
	base_ratio = CLAMP(base_ratio, 0.0, 1.0);
}

void ViewZoom::set_toggle_active(bool p_active) {
	if (p_active) {
		toggle -= toggle_last;
		toggle_last = toggle;
	} else {
		toggle = 0.0;
	}
}

void ViewZoom::update(
		double p_delta,
		double p_focus_point_speed,
		bool p_focus_is_tracking,
		int p_quality_level
) {
	// Gamepad toggle drift.
	if (toggle != 0.0) {
		base_ratio += toggle * 0.01;
		base_ratio = CLAMP(base_ratio, 0.0, 1.0);
	}

	// Speed-reactive pull-out (high quality only, and only while following).
	const double zoom_speed_ratio = Math::smoothstep(speed_edge_min, speed_edge_max, p_focus_point_speed);
	ratio = base_ratio;
	if (p_focus_is_tracking && p_quality_level == 0) {
		ratio += speed_amplitude * zoom_speed_ratio;
	}

	// Ease toward target (folio uses the UNSCALED delta here).
	smoothed_ratio = Math::lerp(smoothed_ratio, ratio, p_delta * 10.0);
}

} // namespace godot
