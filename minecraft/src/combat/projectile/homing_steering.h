#ifndef HOMING_STEERING_H
#define HOMING_STEERING_H

#include <godot_cpp/variant/vector3.hpp>

namespace godot {

/**
 * HomingSteering — the pure math behind homing (no nodes, no engine state).
 * ------------------------------------------------------------------------
 * intercept_time: when a shot from `m` at speed `s` can meet a target at `p` moving
 *   with constant velocity `v`. Solves |p + v t - m| = s t, a quadratic in t:
 *     (v.v - s^2) t^2 + 2 (d.v) t + d.d = 0,  d = p - m
 *   and returns the smallest positive root, or -1 when the target is uncatchable.
 * aim_point: where to point. lead = 0 is pure chase (aim at the target), lead = 1 is the
 *   full predicted intercept; in between is partial prediction (fair for bosses).
 * turn_toward: rotate a direction toward another by at most `max_angle` radians. This cap
 *   IS the dodge window: turn radius = speed / turn_rate.
 */
namespace HomingSteering {

float intercept_time(
		const Vector3 &p_shooter,
		float p_speed,
		const Vector3 &p_target,
		const Vector3 &p_target_velocity
);

Vector3 aim_point(
		const Vector3 &p_shooter,
		float p_speed,
		const Vector3 &p_target,
		const Vector3 &p_target_velocity,
		float p_lead,
		float p_max_time
);

Vector3 turn_toward(
		const Vector3 &p_dir,
		const Vector3 &p_desired,
		float p_max_angle
);

} // namespace HomingSteering

} // namespace godot

#endif // HOMING_STEERING_H
