#ifndef CURVE_BAKER_H
#define CURVE_BAKER_H

#include <godot_cpp/classes/curve3d.hpp>
#include <godot_cpp/variant/packed_vector3_array.hpp>
#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/variant/transform3d.hpp>

namespace godot {

class CurveBaker {
public:
	static PackedVector3Array bake_blended_ring(const Ref<Curve3D> &p_slice_low, const Ref<Curve3D> &p_slice_high, float p_t, int p_resolution);
	static PackedVector3Array bake_curve(const Ref<Curve3D> &p_curve, float p_interval, const Transform3D &p_transform = Transform3D());
	static std::vector<Transform3D> bake_transforms(const Ref<Curve3D> &p_curve, float p_interval, const Transform3D &p_transform = Transform3D());
};

} // namespace godot

#endif // CURVE_BAKER_H
