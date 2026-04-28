#ifndef NATURAL_CUBIC_SPLINE_H
#define NATURAL_CUBIC_SPLINE_H

#include <godot_cpp/variant/vector3.hpp>
#include <vector>

namespace godot {

class NaturalCubicSpline {
private:
    struct Segment {
        Vector3 a, b, c, d;
    };

    std::vector<Vector3> points;
    std::vector<Segment> segments;
    bool is_dirty = true;

    void solve();

public:
    NaturalCubicSpline() = default;
    ~NaturalCubicSpline() = default;

    void set_points(const std::vector<Vector3> &p_points);
    void add_point(const Vector3 &p_point);
    void clear();

    Vector3 evaluate(float p_t); // t is in range [0, points.size() - 1]
    Vector3 evaluate_derivative(float p_t);
    
    size_t get_point_count() const { return points.size(); }
};

} // namespace godot

#endif // NATURAL_CUBIC_SPLINE_H
