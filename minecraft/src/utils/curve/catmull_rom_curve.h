#ifndef CATMULL_ROM_CURVE_H
#define CATMULL_ROM_CURVE_H

#include <godot_cpp/variant/vector3.hpp>
#include <vector>

namespace godot {

class CatmullRomCurve {
public:
	// Cardinal Spline: A spline that passes through its control points with a scale parameter.
	// Scale 0.5f is standard Catmull-Rom. Scale 0.0f is linear. Scale 1.0f: High Curvature
	// Given 4 points (p0, p1, p2, p3), evaluates the segment between p1 and p2
	static Vector3 evaluate(const Vector3 &p0, const Vector3 &p1, const Vector3 &p2, const Vector3 &p3, float t, float p_scale = 0.5f);

	// First derivative for velocity/tangent
	static Vector3 evaluate_derivative(const Vector3 &p0, const Vector3 &p1, const Vector3 &p2, const Vector3 &p3, float t, float p_scale = 0.5f);
};

} // namespace godot

#endif // CATMULL_ROM_CURVE_H
