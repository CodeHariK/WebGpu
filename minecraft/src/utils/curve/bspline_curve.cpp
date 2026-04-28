#include "bspline_curve.h"

namespace godot {

Vector3 BSplineCurve::evaluate_cubic(const Vector3 &p0, const Vector3 &p1, const Vector3 &p2, const Vector3 &p3, float t) {
	float t2 = t * t;
	float t3 = t2 * t;

	// Basis functions for Uniform Cubic B-Spline
	float b0 = (-t3 + 3.0f * t2 - 3.0f * t + 1.0f);
	float b1 = (3.0f * t3 - 6.0f * t2 + 4.0f);
	float b2 = (-3.0f * t3 + 3.0f * t2 + 3.0f * t + 1.0f);
	float b3 = t3;

	return (p0 * b0 + p1 * b1 + p2 * b2 + p3 * b3) / 6.0f;
}

Vector3 BSplineCurve::evaluate_derivative(const Vector3 &p0, const Vector3 &p1, const Vector3 &p2, const Vector3 &p3, float t) {
	float t2 = t * t;

	// Derivatives of basis functions
	float d0 = (-3.0f * t2 + 6.0f * t - 3.0f);
	float d1 = (9.0f * t2 - 12.0f * t);
	float d2 = (-9.0f * t2 + 6.0f * t + 3.0f);
	float d3 = (3.0f * t2);

	return (p0 * d0 + p1 * d1 + p2 * d2 + p3 * d3) / 6.0f;
}

} // namespace godot
