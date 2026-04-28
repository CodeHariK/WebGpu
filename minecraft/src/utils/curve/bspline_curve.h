#ifndef BSPLINE_CURVE_H
#define BSPLINE_CURVE_H

#include <godot_cpp/variant/vector3.hpp>
#include <vector>

namespace godot {

class BSplineCurve {
public:
    // Uniform Cubic B-Spline: Smooth curve passing through the neighborhood of control points
    // Given 4 points (p0, p1, p2, p3), evaluates the segment between p1 and p2
    static Vector3 evaluate_cubic(const Vector3 &p0, const Vector3 &p1, const Vector3 &p2, const Vector3 &p3, float t);
    
    // First derivative for velocity/tangent
    static Vector3 evaluate_derivative(const Vector3 &p0, const Vector3 &p1, const Vector3 &p2, const Vector3 &p3, float t);
};

} // namespace godot

#endif // BSPLINE_CURVE_H
