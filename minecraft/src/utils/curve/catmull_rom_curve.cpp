#include "catmull_rom_curve.h"

namespace godot {

Vector3 CatmullRomCurve::evaluate(const Vector3 &p0, const Vector3 &p1, const Vector3 &p2, const Vector3 &p3, float t, float s) {
	float t2 = t * t;
	float t3 = t2 * t;

	// Basis functions for Cardinal Spline
	float b0 = -s * t3 + 2.0f * s * t2 - s * t;
	float b1 = (2.0f - s) * t3 + (s - 3.0f) * t2 + 1.0f;
	float b2 = (s - 2.0f) * t3 + (3.0f - 2.0f * s) * t2 + s * t;
	float b3 = s * t3 - s * t2;

	return p0 * b0 + p1 * b1 + p2 * b2 + p3 * b3;
}

Vector3 CatmullRomCurve::evaluate_derivative(const Vector3 &p0, const Vector3 &p1, const Vector3 &p2, const Vector3 &p3, float t, float s) {
	float t2 = t * t;

	// Derivatives of basis functions
	float d0 = -3.0f * s * t2 + 4.0f * s * t - s;
	float d1 = 3.0f * (2.0f - s) * t2 + 2.0f * (s - 3.0f) * t;
	float d2 = 3.0f * (s - 2.0f) * t2 + 2.0f * (3.0f - 2.0f * s) * t + s;
	float d3 = 3.0f * s * t2 - 2.0f * s * t;

	return p0 * d0 + p1 * d1 + p2 * d2 + p3 * d3;
}

} // namespace godot
