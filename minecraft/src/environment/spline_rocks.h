/**
 * @file spline_rocks.h
 * @brief SplineRocks: a wall / line of boulders that follows the parent ProceduralSpline3D.
 */
#ifndef SPLINE_ROCKS_H
#define SPLINE_ROCKS_H

#include "rock_mesh.h"
#include "utils/spline3d/procedural_spline3d.h"
#include <godot_cpp/classes/material.hpp>
#include <godot_cpp/classes/multi_mesh_instance3d.hpp>
#include <godot_cpp/classes/shader_material.hpp>
#include <godot_cpp/classes/static_body3d.hpp>
#include <vector>

namespace godot {

/**
 * @class SplineRocks
 * @brief SplineComponent that lines the parent spline with boulders: every `spacing` metres (± jitter),
 * in `rows` parallel rows `row_offset` apart, one instance of one of `variants` RockMesh variants (same
 * style, different seeds), scaled by `size` (± `size_variation`), yawed randomly or elongated along the
 * spline. Overlapping spacing makes a continuous rock wall for cliff feet and mountain flanks; sparse
 * spacing scatters boulders along a trail. `snap_to_ground` drops each rock onto the Terrain3D
 * heightmap (retrying while chunks stream in). One MultiMesh per variant, per-blob convex colliders,
 * matte stone material. Internal children only; never touches the terrain.
 */
class SplineRocks : public SplineComponent {
	GDCLASS(SplineRocks,
			SplineComponent)

private:
	// Placement
	float spacing = 2.4f;
	float spacing_jitter = 0.3f;
	float start_offset = 0.0f;
	float section_start = 0.0f;
	float section_end = 0.0f; // 0 = to the end
	int rows = 1;
	float row_offset = 1.6f; // Between rows, metres
	float lateral_offset = 0.0f; // Whole formation off the spline (+ = right of travel)
	float lateral_jitter = 0.4f;
	float vertical_offset = 0.0f;
	bool snap_to_ground = true;
	bool align_to_tangent = true; // Elongate rocks along the spline (else random yaw)
	int seed = 3;

	// Rock style (copied onto every variant)
	int variants = 4;
	Vector3 size = Vector3(1.6f, 1.3f, 1.4f);
	float size_variation = 0.35f;
	int detail = 2;
	RockMesh::BaseShape base_shape = RockMesh::BASE_CUBE;
	float roundness = 0.4f;
	float tilt = 8.0f;
	float noise_amplitude = 0.32f;
	bool ridged = true;
	float facet_snap = 0.6f;
	bool flat_shaded = true;
	float flatten_bottom = 0.3f;
	Color base_color = Color(0.40f, 0.37f, 0.34f);
	Color top_color = Color(0.62f, 0.60f, 0.56f);
	int strata = 0;
	float moss_amount = 0.2f;
	Ref<Material> material;
	bool collision_enabled = true;

	std::vector<Ref<RockMesh>> _variants;
	std::vector<MultiMeshInstance3D *> _mm;
	StaticBody3D *static_body = nullptr;
	Ref<ShaderMaterial> _stone;
	bool _rebuild_queued = false;
	ProceduralSpline3D *_watched_spline = nullptr;
	int _ground_retries = 0;
	double _retry_timer = 0.0;

	void _connect_spline();
	void _disconnect_spline();
	void _make_variants();
	bool
	_place(std::vector<std::vector<Transform3D>> &r_per_variant,
		   bool &r_missing_ground) const;
	bool _ground_height(
			const Vector3 &p_global,
			float &r_height
	) const;
	void _clear_children();

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	SplineRocks();
	~SplineRocks();

	float get_spline_padding() const override { return 0.0f; }

	void queue_rebuild();
	void rebuild();
	void _on_spline_changed();

	void set_material(const Ref<Material> &p_material);
	Ref<Material> get_material() const { return material; }

	// clang-format off
#define SR_PROP(m_type, m_name, m_clamp)      \
	void set_##m_name(m_type p_value) {       \
		m_name = m_clamp;                     \
		queue_rebuild();                      \
	}                                         \
	m_type get_##m_name() const { return m_name; }

	SR_PROP(float, spacing, MAX(0.1f, p_value))
	SR_PROP(float, spacing_jitter, CLAMP(p_value, 0.0f, 0.9f))
	SR_PROP(float, start_offset, MAX(0.0f, p_value))
	SR_PROP(float, section_start, MAX(0.0f, p_value))
	SR_PROP(float, section_end, MAX(0.0f, p_value))
	SR_PROP(int, rows, CLAMP(p_value, 1, 6))
	SR_PROP(float, row_offset, MAX(0.0f, p_value))
	SR_PROP(float, lateral_offset, p_value)
	SR_PROP(float, lateral_jitter, MAX(0.0f, p_value))
	SR_PROP(float, vertical_offset, p_value)
	SR_PROP(bool, snap_to_ground, p_value)
	SR_PROP(bool, align_to_tangent, p_value)
	SR_PROP(int, seed, p_value)
	SR_PROP(int, variants, CLAMP(p_value, 1, 12))
	SR_PROP(Vector3, size, Vector3(MAX(0.02f, p_value.x), MAX(0.02f, p_value.y), MAX(0.02f, p_value.z)))
	SR_PROP(float, size_variation, CLAMP(p_value, 0.0f, 0.9f))
	SR_PROP(int, detail, CLAMP(p_value, 0, 3))
	SR_PROP(RockMesh::BaseShape, base_shape, p_value)
	SR_PROP(float, roundness, CLAMP(p_value, 0.0f, 1.0f))
	SR_PROP(float, tilt, CLAMP(p_value, 0.0f, 60.0f))
	SR_PROP(float, noise_amplitude, CLAMP(p_value, 0.0f, 1.0f))
	SR_PROP(bool, ridged, p_value)
	SR_PROP(float, facet_snap, CLAMP(p_value, 0.0f, 1.0f))
	SR_PROP(bool, flat_shaded, p_value)
	SR_PROP(float, flatten_bottom, CLAMP(p_value, 0.0f, 0.9f))
	SR_PROP(Color, base_color, p_value)
	SR_PROP(Color, top_color, p_value)
	SR_PROP(int, strata, CLAMP(p_value, 0, 32))
	SR_PROP(float, moss_amount, CLAMP(p_value, 0.0f, 1.0f))
	SR_PROP(bool, collision_enabled, p_value)
#undef SR_PROP
	// clang-format on
};

} // namespace godot

#endif // SPLINE_ROCKS_H
