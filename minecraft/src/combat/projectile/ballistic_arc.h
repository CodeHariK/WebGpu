#ifndef BALLISTIC_ARC_H
#define BALLISTIC_ARC_H

#include <godot_cpp/variant/vector3.hpp>

namespace godot {

/**
 * BallisticArc — the pure math for an unguided lob (no nodes, no engine state).
 * ---------------------------------------------------------------------------
 * Choose a flight time from the horizontal distance (horizontal speed is constant in a
 * ballistic arc), then solve the launch velocity that lands exactly on the spot:
 *     p(t) = from + v0 t + 1/2 g t^2,  p(T) = to   =>   v0 = (to - from) / T + (0, g T / 2, 0)
 * Apex height above the straight line is g T^2 / 8, so longer shots lob higher.
 */
namespace BallisticArc {

float flight_time(
		const Vector3 &p_from,
		const Vector3 &p_to,
		float p_horizontal_speed,
		float p_min_time
);

Vector3 launch_velocity(
		const Vector3 &p_from,
		const Vector3 &p_to,
		float p_time,
		float p_gravity
);

} // namespace BallisticArc

} // namespace godot

#endif // BALLISTIC_ARC_H
