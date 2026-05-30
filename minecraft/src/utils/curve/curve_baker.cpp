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

// In CurveBaker.cpp
std::vector<Transform3D> CurveBaker::bake_transforms_adaptive(
		const Ref<Curve3D> &curve, float start_dist, float end_dist,
		float max_step, float min_step, float angle_tol_degrees,
		const Transform3D &global_transform) {
	std::vector<Transform3D> result;
	if (curve.is_null() || start_dist >= end_dist)
		return result;

	float tolerance_dot = Math::cos(Math::deg_to_rad(angle_tol_degrees));

	// 1. Always push the start point
	Transform3D start_t = global_transform * curve->sample_baked_with_rotation(start_dist);
	result.push_back(start_t);

	float current_d = start_dist;

	// 2. Step forward along the curve
	while (current_d < end_dist) {
		float next_d = MIN(current_d + max_step, end_dist);
		Transform3D current_t = result.back();
		Transform3D next_t = global_transform * curve->sample_baked_with_rotation(next_d);

		// 3. ADAPTIVE SUBDIVISION LOOP
		// If the angle between Forward vectors is too sharp, cut the step size in half
		float step_size = next_d - current_d;

		while (step_size > min_step) {
			Vector3 fwd_current = -current_t.basis.get_column(2).normalized();
			Vector3 fwd_next = -next_t.basis.get_column(2).normalized();

			if (fwd_current.dot(fwd_next) >= tolerance_dot) {
				break; // The curve is flat enough, accept this step!
			}

			// Too sharp! Halve the step and re-evaluate
			step_size *= 0.5f;
			next_d = current_d + step_size;
			next_t = global_transform * curve->sample_baked_with_rotation(next_d);
		}

		// 4. Accept the point and move forward
		result.push_back(next_t);
		current_d = next_d;
	}

	return result;
}

} // namespace godot
