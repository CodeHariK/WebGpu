#include "curve_baker.h"

namespace godot {

PackedVector3Array CurveBaker::bake_blended_ring(const Ref<Curve3D> &p_slice_low, const Ref<Curve3D> &p_slice_high, float p_t, int p_resolution) {
	PackedVector3Array current_ring;
	current_ring.resize(p_resolution);

	for (int k = 0; k < p_resolution; ++k) {
		float ring_t = (float)k / (float)(p_resolution - 1);

		Vector3 pt_low = Vector3();
		Vector3 pt_high = Vector3();

		if (p_slice_low.is_valid()) {
			pt_low = p_slice_low->sample_baked(ring_t * p_slice_low->get_baked_length());
		}
		if (p_slice_high.is_valid()) {
			pt_high = p_slice_high->sample_baked(ring_t * p_slice_high->get_baked_length());
		}

		Vector3 blended_pt = pt_low.lerp(pt_high, p_t);
		current_ring.set(k, blended_pt);
	}

	return current_ring;
}

PackedVector3Array CurveBaker::bake_curve(const Ref<Curve3D> &p_curve, float p_interval, const Transform3D &p_transform) {
	if (p_curve.is_null()) {
		return PackedVector3Array();
	}

	PackedVector3Array pts;
	float total_len = p_curve->get_baked_length();
	for (float d = 0.0f; d < total_len; d += p_interval) {
		pts.push_back(p_transform.xform(p_curve->sample_baked(d)));
	}
	pts.push_back(p_transform.xform(p_curve->sample_baked(total_len)));
	return pts;
}

std::vector<Transform3D> CurveBaker::bake_transforms(const Ref<Curve3D> &p_curve, float p_interval, const Transform3D &p_transform) {
	if (p_curve.is_null()) {
		return std::vector<Transform3D>();
	}

	std::vector<Transform3D> transforms;
	float total_len = p_curve->get_baked_length();
	for (float d = 0.0f; d < total_len; d += p_interval) {
		transforms.push_back(p_transform * p_curve->sample_baked_with_rotation(d));
	}
	transforms.push_back(p_transform * p_curve->sample_baked_with_rotation(total_len));
	return transforms;
}

} // namespace godot
