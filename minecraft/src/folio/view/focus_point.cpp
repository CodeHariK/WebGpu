#include "focus_point.h"

#include <godot_cpp/core/math.hpp>

namespace godot {

void FolioViewFocusPoint::init(const Vector3 &p_initial) {
	tracked_position = Vector3(p_initial.x, 0.0, p_initial.z);
	position = tracked_position;
	smoothed_position = tracked_position;
}

void FolioViewFocusPoint::set_tracked_position(const Vector3 &p_pos) {
	tracked_position.x = p_pos.x;
	tracked_position.z = p_pos.z;
}

void FolioViewFocusPoint::pan(
		double p_dx,
		double p_dz
) {
	is_tracking = false;
	position.x += p_dx;
	position.z += p_dz;
}

double FolioViewFocusPoint::update(double p_delta) {
	// Snap to the tracked point while following.
	if (is_tracking) {
		position.x = tracked_position.x;
		position.z = tracked_position.z;
	}

	// Magnet: ease toward the tracked point, stronger the farther away.
	if (magnet_active) {
		const double magnet_dx = tracked_position.x - position.x;
		const double magnet_dz = tracked_position.z - position.z;
		const double distance = Math::sqrt(magnet_dx * magnet_dx + magnet_dz * magnet_dz);
		const double strength = distance * magnet_multiplier;
		position.x += strength * magnet_dx * p_delta;
		position.z += strength * magnet_dz * p_delta;
	}

	// Low-pass smoothing. folio: easing = remap(easing, 0,1, 1, delta*10).
	const double ease = Math::remap(easing, 0.0, 1.0, 1.0, p_delta * 10.0);
	const Vector3 new_smoothed = smoothed_position.lerp(position, ease);

	// Travel speed of the smoothed point (units per second).
	const double sdx = new_smoothed.x - smoothed_position.x;
	const double sdz = new_smoothed.z - smoothed_position.z;
	const double speed = (p_delta > 0.0) ? (Math::sqrt(sdx * sdx + sdz * sdz) / p_delta) : 0.0;

	smoothed_position = new_smoothed;
	return speed;
}

} // namespace godot
