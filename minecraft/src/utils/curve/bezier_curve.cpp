#include "bezier_curve.h"

namespace godot {

Vector3 BezierCurve::evaluate_cubic(const Vector3 &p0, const Vector3 &p1, const Vector3 &p2, const Vector3 &p3, float t) {
	float inv_t = 1.0f - t;
	float b0 = inv_t * inv_t * inv_t;
	float b1 = 3.0f * inv_t * inv_t * t;
	float b2 = 3.0f * inv_t * t * t;
	float b3 = t * t * t;

	return p0 * b0 + p1 * b1 + p2 * b2 + p3 * b3;
}

Vector3 BezierCurve::evaluate_derivative(const Vector3 &p0, const Vector3 &p1, const Vector3 &p2, const Vector3 &p3, float t) {
	float inv_t = 1.0f - t;
	float d0 = 3.0f * inv_t * inv_t;
	float d1 = 6.0f * inv_t * t;
	float d2 = 3.0f * t * t;

	return (p1 - p0) * d0 + (p2 - p1) * d1 + (p3 - p2) * d2;
}

} // namespace godot
