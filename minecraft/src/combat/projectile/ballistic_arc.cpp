#include "ballistic_arc.h"

#include <godot_cpp/core/math.hpp>

namespace godot {

namespace BallisticArc {

float flight_time(
		const Vector3 &p_from,
		const Vector3 &p_to,
		float p_horizontal_speed,
		float p_min_time
) {
	Vector3 d = p_to - p_from;
	float horizontal = Vector3(d.x, 0.0f, d.z).length();
	return MAX(p_min_time, horizontal / MAX(0.1f, p_horizontal_speed));
}

Vector3 launch_velocity(
		const Vector3 &p_from,
		const Vector3 &p_to,
		float p_time,
		float p_gravity
) {
	float t = MAX(0.05f, p_time);
	return (p_to - p_from) / t + Vector3(0.0f, 0.5f * p_gravity * t, 0.0f);
}

} // namespace BallisticArc

} // namespace godot
