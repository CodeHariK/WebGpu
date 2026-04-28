#ifndef BEZIER_CURVE_H
#define BEZIER_CURVE_H

#include <godot_cpp/variant/vector3.hpp>
#include <vector>

namespace godot {

class BezierCurve {
public:
	// Cubic Bezier: B(t) = (1-t)^3*P0 + 3(1-t)^2*t*P1 + 3(1-t)*t^2*P2 + t^3*P3
	static Vector3 evaluate_cubic(const Vector3 &p0, const Vector3 &p1, const Vector3 &p2, const Vector3 &p3, float t);

	// First derivative: B'(t) = 3(1-t)^2(P1-P0) + 6(1-t)t(P2-P1) + 3t^2(P3-P2)
	static Vector3 evaluate_derivative(const Vector3 &p0, const Vector3 &p1, const Vector3 &p2, const Vector3 &p3, float t);
};

} // namespace godot

#endif // BEZIER_CURVE_H
