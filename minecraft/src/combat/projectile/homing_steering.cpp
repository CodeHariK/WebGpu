#include "homing_steering.h"

#include <godot_cpp/core/math.hpp>

#include <cmath>

namespace godot {

namespace HomingSteering {

float intercept_time(
		const Vector3 &p_shooter,
		float p_speed,
		const Vector3 &p_target,
		const Vector3 &p_target_velocity
) {
	Vector3 d = p_target - p_shooter;
	float a = p_target_velocity.dot(p_target_velocity) - p_speed * p_speed;
	float b = 2.0f * d.dot(p_target_velocity);
	float c = d.dot(d);

	if (std::abs(a) < 1e-4f) { // same speed: the equation is linear
		return (b < 0.0f) ? -c / b : -1.0f;
	}
	float disc = b * b - 4.0f * a * c;
	if (disc < 0.0f) {
		return -1.0f;
	}
	float root = std::sqrt(disc);
	float t1 = (-b - root) / (2.0f * a);
	float t2 = (-b + root) / (2.0f * a);
	float t_min = MIN(t1, t2);
	float t_max = MAX(t1, t2);
	if (t_min > 0.0f) {
		return t_min;
	}
	return (t_max > 0.0f) ? t_max : -1.0f;
}

Vector3 aim_point(
		const Vector3 &p_shooter,
		float p_speed,
		const Vector3 &p_target,
		const Vector3 &p_target_velocity,
		float p_lead,
		float p_max_time
) {
	if (p_lead <= 0.0f || p_speed <= 0.0f) {
		return p_target;
	}
	float t = intercept_time(p_shooter, p_speed, p_target, p_target_velocity);
	if (t < 0.0f) { // can't catch it: aim at where it will be after the straight-line flight time
		t = p_shooter.distance_to(p_target) / p_speed;
	}
	t = MIN(t, p_max_time);
	return p_target + p_target_velocity * (t * p_lead);
}

Vector3 turn_toward(
		const Vector3 &p_dir,
		const Vector3 &p_desired,
		float p_max_angle
) {
	float angle = p_dir.angle_to(p_desired);
	if (angle <= p_max_angle) {
		return p_desired;
	}
	Vector3 axis = p_dir.cross(p_desired);
	if (axis.length_squared() < 1e-8f) { // exactly opposite: turn over the top
		axis = p_dir.cross(Vector3(0, 1, 0));
		if (axis.length_squared() < 1e-8f) {
			axis = Vector3(1, 0, 0);
		}
	}
	return p_dir.rotated(axis.normalized(), p_max_angle).normalized();
}

} // namespace HomingSteering

} // namespace godot
