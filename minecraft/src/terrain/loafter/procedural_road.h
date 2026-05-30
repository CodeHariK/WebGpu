#ifndef PROCEDURAL_ROAD_H
#define PROCEDURAL_ROAD_H

#include "utils/spline3d/procedural_spline3d.h"
#include <godot_cpp/classes/array_mesh.hpp>
#include <godot_cpp/classes/curve2d.hpp>
#include <godot_cpp/classes/material.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>

namespace godot {

class ProceduralRoad : public SplineComponent {
	GDCLASS(ProceduralRoad, SplineComponent)

private:
	ProceduralSpline3D *parent_spline = nullptr;
	MeshInstance3D *mesh_instance = nullptr;

	Ref<Curve2D> cross_section; // The single road profile
	Ref<Material> material;
	float custom_padding = 20.0f;
	bool mesh_dirty = true;

	bool use_adaptive = false;
	float chunk_start_distance = 0.0f;
	float chunk_end_distance = 0.0f;
	float adaptive_max_step = 10.0f;
	float adaptive_min_step = 1.0f;
	float adaptive_angle_tol = 5.0f;
	float texture_length = 10.0f; // The repeating length of the road texture along the track.

	// Bakes the transforms along the path curve based on adaptive or standard chunk settings.
	std::vector<Transform3D> _bake_road_transforms(ProceduralSpline3D *p_spline, const Ref<Curve3D> &p_curve, float p_start, float p_end);

	// Computes U coordinates based on the 2D cross-section points.
	std::vector<float> _calculate_profile_u(const PackedVector2Array &p_profile, float &r_total_length);

	// Projects the 2D road profile onto 3D space using baked transforms.
	std::vector<PackedVector3Array> _project_road_rings(const std::vector<Transform3D> &p_transforms, const PackedVector2Array &p_profile, const Transform3D &p_inv_global);

	// Generates vertices, normals, UVs, and indices by stitching adjacent rings together.
	void _stitch_road_mesh(
			const std::vector<Transform3D> &p_transforms,
			const std::vector<PackedVector3Array> &p_rings,
			const std::vector<float> &p_profile_u,
			float p_start_v,
			float p_total_profile_length,
			bool p_is_closed,
			float p_texture_length,
			PackedVector3Array &r_vertices,
			PackedVector3Array &r_normals,
			PackedVector2Array &r_uvs,
			PackedInt32Array &r_indices);

	void _update_mesh();

protected:
	static void _bind_methods();

public:
	ProceduralRoad();
	~ProceduralRoad() = default;

	virtual void _ready() override;
	void queue_update();
	void _on_parent_spline_changed();

	void set_cross_section(const Ref<Curve2D> &p_profile);
	Ref<Curve2D> get_cross_section() const { return cross_section; }

	void set_material(const Ref<Material> &p_material);
	Ref<Material> get_material() const { return material; }

	void set_custom_padding(float p_pad);
	float get_custom_padding() const { return custom_padding; }
	virtual float get_spline_padding() const override { return custom_padding; }

	void set_use_adaptive(bool p_adaptive) {
		use_adaptive = p_adaptive;
		queue_update();
	}
	bool get_use_adaptive() const { return use_adaptive; }

	void set_chunk_start_distance(float p_dist) {
		chunk_start_distance = p_dist < 0.0f ? 0.0f : p_dist;
		queue_update();
	}
	float get_chunk_start_distance() const { return chunk_start_distance; }

	void set_chunk_end_distance(float p_dist) {
		chunk_end_distance = p_dist < 0.0f ? 0.0f : p_dist;
		queue_update();
	}
	float get_chunk_end_distance() const { return chunk_end_distance; }

	void set_adaptive_max_step(float p_step) {
		adaptive_max_step = p_step < 0.01f ? 0.01f : p_step;
		queue_update();
	}
	float get_adaptive_max_step() const { return adaptive_max_step; }

	void set_adaptive_min_step(float p_step) {
		adaptive_min_step = p_step < 0.001f ? 0.001f : p_step;
		queue_update();
	}
	float get_adaptive_min_step() const { return adaptive_min_step; }

	void set_adaptive_angle_tol(float p_tol) {
		adaptive_angle_tol = p_tol < 0.0f ? 0.0f : (p_tol > 180.0f ? 180.0f : p_tol);
		queue_update();
	}
	float get_adaptive_angle_tol() const { return adaptive_angle_tol; }

	void set_texture_length(float p_length) {
		texture_length = p_length < 0.01f ? 0.01f : p_length;
		queue_update();
	}
	float get_texture_length() const { return texture_length; }
};

} // namespace godot

#endif // PROCEDURAL_ROAD_H


