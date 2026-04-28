#include "natural_cubic_spline.h"
#include <godot_cpp/variant/utility_functions.hpp>
#include <cmath>

namespace godot {

void NaturalCubicSpline::set_points(const std::vector<Vector3> &p_points) {
    points = p_points;
    is_dirty = true;
}

void NaturalCubicSpline::add_point(const Vector3 &p_point) {
    points.push_back(p_point);
    is_dirty = true;
}

void NaturalCubicSpline::clear() {
    points.clear();
    segments.clear();
    is_dirty = true;
}

void NaturalCubicSpline::solve() {
    if (!is_dirty || points.size() < 2) {
        return;
    }

    int n = (int)points.size() - 1;
    segments.resize(n);

    // solving for second derivatives (D) using Thomas Algorithm for tridiagonal matrix
    // system: D[i-1] + 4D[i] + D[i+1] = 6 * (P[i+1] - 2P[i] + P[i-1])
    std::vector<float> alpha(n + 1);
    std::vector<Vector3> beta(n + 1);
    std::vector<Vector3> D(n + 1);

    // Natural boundary: D[0] = D[n] = 0
    alpha[0] = 0.0f;
    beta[0] = Vector3(0, 0, 0);

    for (int i = 1; i < n; i++) {
        float m = 1.0f / (4.0f - alpha[i - 1]);
        alpha[i] = m;
        Vector3 rhs = 6.0f * (points[i + 1] - 2.0f * points[i] + points[i - 1]);
        beta[i] = m * (rhs - beta[i - 1]);
    }

    D[n] = Vector3(0, 0, 0);
    for (int i = n - 1; i >= 0; i--) {
        D[i] = beta[i] - alpha[i] * D[i + 1];
    }

    // Convert to polynomial coefficients for each segment
    // S_i(t) = a*t^3 + b*t^2 + c*t + d, where t is local [0, 1]
    for (int i = 0; i < n; i++) {
        segments[i].a = (D[i + 1] - D[i]) / 6.0f;
        segments[i].b = D[i] / 2.0f;
        segments[i].c = (points[i + 1] - points[i]) - (D[i + 1] + 2.0f * D[i]) / 6.0f;
        segments[i].d = points[i];
    }

    is_dirty = false;
}

Vector3 NaturalCubicSpline::evaluate(float p_t) {
    if (points.empty()) return Vector3();
    if (points.size() == 1) return points[0];
    
    solve();

    int n = (int)segments.size();
    int idx = (int)std::floor(p_t);
    
    if (idx < 0) return points[0];
    if (idx >= n) return points.back();

    float t = p_t - (float)idx;
    const Segment &s = segments[idx];

    // S(t) = a*t^3 + b*t^2 + c*t + d
    return s.a * (t * t * t) + s.b * (t * t) + s.c * t + s.d;
}

Vector3 NaturalCubicSpline::evaluate_derivative(float p_t) {
    if (points.size() < 2) return Vector3();
    
    solve();

    int n = (int)segments.size();
    int idx = (int)std::floor(p_t);
    
    if (idx < 0) idx = 0;
    if (idx >= n) idx = n - 1;

    float t = p_t - (float)idx;
    if (p_t >= (float)n) t = 1.0f;

    const Segment &s = segments[idx];

    // S'(t) = 3a*t^2 + 2b*t + c
    return 3.0f * s.a * (t * t) + 2.0f * s.b * t + s.c;
}

} // namespace godot
