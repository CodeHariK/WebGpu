/**
 * @file tr_lake.h
 * @brief TerrainSplineLake: a flat water surface filling a closed spline.
 */
#ifndef TR_LAKE_H
#define TR_LAKE_H

#include "utils/spline3d/procedural_spline3d.h"
#include <godot_cpp/classes/area3d.hpp>
#include <godot_cpp/classes/collision_shape3d.hpp>
#include <godot_cpp/classes/material.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/shader_material.hpp>
#include <vector>

namespace godot {

/**
 * @class TerrainSplineLake
 * @brief SplineComponent that fills its closed parent spline with a flat water surface at `water_level`
 * (default: the spline's own height), triangulated with interior samples like the cliff cap so the
 * shader can bob it, with an Area3D volume `depth` deep (group "water"). The bed is a separate
 * TerrainSplineDeformer on the same spline (Replace, fill_interior, negative max_height); the lake
 * itself never touches the terrain. Uses the shared toon water material unless `material` is set.
 */
class TerrainSplineLake : public SplineComponent {
	GDCLASS(TerrainSplineLake,
			SplineComponent)

private:
	bool level_from_spline = true; // Water at the spline's Y (plus level_offset); else at water_level
	float water_level = 0.0f; // World Y when !level_from_spline
	float level_offset = -0.3f; // Below the rim when following the spline
	float depth = 2.0f; // Area3D volume below the surface
	float shore_offset = 0.0f; // Grow (+) or shrink (−) the polygon, metres
	float resolution = 6.0f; // Interior sample spacing
	float segment_length = 3.0f; // Rim sample spacing
	float texture_scale = 12.0f; // Metres per UV repeat
	Color water_color = Color(0.25f, 0.55f, 0.85f);
	Color foam_color = Color(0.95f, 0.98f, 1.0f);
	float water_speed = 0.15f;
	float water_alpha = 0.85f;
	Ref<Material> material;

	MeshInstance3D *mesh_instance = nullptr;
	Area3D *water_area = nullptr;
	CollisionShape3D *water_shape = nullptr;
	Ref<ShaderMaterial> _water_material;
	bool _rebuild_queued = false;
	ProceduralSpline3D *_watched_spline = nullptr;

	void _connect_spline();
	void _disconnect_spline();
	bool _rim_points(
			std::vector<Vector3> &r_rim,
			float &r_level
	) const;
	Ref<ArrayMesh> _build_mesh(
			const std::vector<Vector3> &p_rim,
			float p_level
	) const;
	void _apply_material();

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	TerrainSplineLake();
	~TerrainSplineLake();

	void rebuild();
	void queue_rebuild();
	void _on_spline_changed();

	float get_spline_padding() const override { return MAX(0.0f, shore_offset) + 1.0f; }
	Area3D *get_water_area() const { return water_area; }

	// clang-format off
#define TR_LAKE_PROP(m_type, m_name, m_clamp) \
	void set_##m_name(m_type p_value) {       \
		m_name = m_clamp;                     \
		queue_rebuild();                      \
	}                                         \
	m_type get_##m_name() const { return m_name; }

	TR_LAKE_PROP(bool, level_from_spline, p_value)
	TR_LAKE_PROP(float, water_level, p_value)
	TR_LAKE_PROP(float, level_offset, p_value)
	TR_LAKE_PROP(float, depth, MAX(0.1f, p_value))
	TR_LAKE_PROP(float, shore_offset, p_value)
	TR_LAKE_PROP(float, resolution, MAX(0.5f, p_value))
	TR_LAKE_PROP(float, segment_length, MAX(0.5f, p_value))
	TR_LAKE_PROP(float, texture_scale, MAX(0.1f, p_value))
	TR_LAKE_PROP(Color, water_color, p_value)
	TR_LAKE_PROP(Color, foam_color, p_value)
	TR_LAKE_PROP(float, water_speed, p_value)
	TR_LAKE_PROP(float, water_alpha, CLAMP(p_value, 0.0f, 1.0f))
#undef TR_LAKE_PROP
	// clang-format on

	void set_material(const Ref<Material> &p_material);
	Ref<Material> get_material() const { return material; }
};

} // namespace godot

#endif // TR_LAKE_H
