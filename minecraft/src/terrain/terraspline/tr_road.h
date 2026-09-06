/**
 * @file tr_road.h
 * @brief TerrainSplineRoad: a drivable track mesh swept along its parent spline.
 */
#ifndef TR_ROAD_H
#define TR_ROAD_H

#include "utils/spline3d/procedural_spline3d.h"
#include <godot_cpp/classes/collision_shape3d.hpp>
#include <godot_cpp/classes/curve2d.hpp>
#include <godot_cpp/classes/material.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/static_body3d.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/packed_color_array.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>
#include <godot_cpp/variant/packed_vector2_array.hpp>
#include <godot_cpp/variant/packed_vector3_array.hpp>
#include <vector>

namespace godot {

/**
 * @class TerrainSplineRoad
 * @brief SplineComponent that sweeps a closed cross-section (a slab with rounded edges, a slab with
 * rails, a half-pipe, or a custom Curve2D) along its parent ProceduralSpline3D and builds one
 * flat-shaded, vertex-coloured mesh with an underside and end caps - a self-contained track that can
 * float in the air, loop and bank (the spline's tilt), with trimesh collision to drive on.
 *
 * `height_source`: SPLINE (default) - the spline's Y and tilt ARE the road surface; nothing else moves
 * it. TERRAIN (planned) - heights from the terrain profile a sibling deformer bakes.
 *
 * A long spline can be split into sections with `section_start` / `section_end` (arc length), each
 * with its own profile - or a gap between two sections for a jump. Nothing generated is saved: the
 * mesh and collision live in internal children and are rebuilt on load and on every change.
 *
 * Implementation:
 *  - tr_road.cpp          bindings, properties, rebuild scheduling, child nodes
 *  - tr_road_profile.cpp  cross-section presets -> closed 2-D polygon with per-edge regions
 *  - tr_road_mesh.cpp     stations along the spline, sweeping, stitching, caps
 */
class TerrainSplineRoad : public SplineComponent {
	GDCLASS(TerrainSplineRoad,
			SplineComponent)

public:
	enum Profile { PROFILE_SLAB = 0, PROFILE_SLAB_RAILS = 1, PROFILE_HALF_PIPE = 2, PROFILE_CUSTOM = 3 };
	enum Sampling { SAMPLING_FIXED = 0, SAMPLING_ADAPTIVE = 1 };
	enum HeightSource { HEIGHT_SPLINE = 0, HEIGHT_TERRAIN = 1 };
	/// Which part of the cross-section a polygon edge belongs to; picks its vertex colour.
	enum Region { REGION_DECK = 0, REGION_EDGE = 1, REGION_RAIL = 2, REGION_UNDERSIDE = 3 };

	/// One corner of the cross-section (x = lateral, + is right of travel; y = up). The edge that
	/// starts at this corner carries `region`; `hard` starts a new flat-shaded facet.
	struct ProfilePoint {
		Vector2 pos;
		int region = REGION_DECK;
	};

private:
	// ---- Cross-section ----
	Profile profile = PROFILE_SLAB;
	float width = 8.0f;
	float thickness = 0.6f;
	float edge_radius = 0.25f; // Chamfer on the deck's outer edges
	float rail_height = 0.8f;
	float rail_width = 0.4f;
	float pipe_depth = 2.5f; // HALF_PIPE: how far the middle sags below the rims
	int pipe_segments = 8;
	Ref<Curve2D> cross_section; // CUSTOM: baked points, closed if the last point meets the first

	// ---- Along the spline ----
	HeightSource height_source = HEIGHT_SPLINE;
	Sampling sampling = SAMPLING_FIXED;
	float segment_length = 3.0f;
	float adaptive_max_step = 8.0f;
	float adaptive_min_step = 1.0f;
	float adaptive_angle_tol = 4.0f; // Degrees between stations before subdividing
	float section_start = 0.0f; // Arc length; 0..0 = whole spline
	float section_end = 0.0f;
	bool cap_ends = true;

	// ---- Look ----
	float texture_length = 10.0f; // Metres of track per V repeat
	Color deck_color = Color(0.36f, 0.38f, 0.42f);
	Color edge_color = Color(0.95f, 0.92f, 0.85f);
	Color rail_color = Color(0.9f, 0.25f, 0.2f);
	Color underside_color = Color(0.28f, 0.3f, 0.34f);
	Ref<Material> material;
	bool collision_enabled = true;

	// ---- Internals ----
	MeshInstance3D *mesh_instance = nullptr;
	StaticBody3D *static_body = nullptr;
	CollisionShape3D *collision_shape = nullptr;
	Ref<Material> _fallback_material;
	bool _rebuild_queued = false;
	ProceduralSpline3D *_watched_spline = nullptr;

	// tr_road.cpp
	void _connect_spline();
	void _disconnect_spline();
	void _ensure_nodes();
	void _apply_material();
	void _update_collision(const Ref<ArrayMesh> &p_mesh);

	// tr_road_profile.cpp
	void _build_profile(
			std::vector<ProfilePoint> &r_points,
			bool &r_closed
	) const;
	Color _region_color(int p_region) const;

	// tr_road_mesh.cpp
	bool _build_stations(
			std::vector<Transform3D> &r_stations,
			std::vector<float> &r_distances,
			bool &r_loop
	) const;
	Ref<ArrayMesh> _build_mesh(
			const std::vector<Transform3D> &p_stations,
			const std::vector<float> &p_distances,
			bool p_loop,
			const std::vector<ProfilePoint> &p_profile,
			bool p_profile_closed
	) const;

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	TerrainSplineRoad();
	~TerrainSplineRoad();

	void rebuild();
	void queue_rebuild();
	void _on_spline_changed();
	void _on_resource_changed();

	float get_spline_padding() const override { return width * 0.5f + rail_width + 1.0f; }

	// clang-format off
#define TR_ROAD_PROP(m_type, m_name, m_clamp) \
	void set_##m_name(m_type p_value) {       \
		m_name = m_clamp;                     \
		queue_rebuild();                      \
	}                                         \
	m_type get_##m_name() const { return m_name; }

	TR_ROAD_PROP(Profile, profile, p_value)
	TR_ROAD_PROP(float, width, MAX(0.2f, p_value))
	TR_ROAD_PROP(float, thickness, MAX(0.02f, p_value))
	TR_ROAD_PROP(float, edge_radius, MAX(0.0f, p_value))
	TR_ROAD_PROP(float, rail_height, MAX(0.0f, p_value))
	TR_ROAD_PROP(float, rail_width, MAX(0.05f, p_value))
	TR_ROAD_PROP(float, pipe_depth, MAX(0.0f, p_value))
	TR_ROAD_PROP(int, pipe_segments, CLAMP(p_value, 2, 64))
	TR_ROAD_PROP(HeightSource, height_source, p_value)
	TR_ROAD_PROP(Sampling, sampling, p_value)
	TR_ROAD_PROP(float, segment_length, MAX(0.25f, p_value))
	TR_ROAD_PROP(float, adaptive_max_step, MAX(0.1f, p_value))
	TR_ROAD_PROP(float, adaptive_min_step, MAX(0.05f, p_value))
	TR_ROAD_PROP(float, adaptive_angle_tol, CLAMP(p_value, 0.1f, 90.0f))
	TR_ROAD_PROP(float, section_start, MAX(0.0f, p_value))
	TR_ROAD_PROP(float, section_end, MAX(0.0f, p_value))
	TR_ROAD_PROP(bool, cap_ends, p_value)
	TR_ROAD_PROP(float, texture_length, MAX(0.1f, p_value))
	TR_ROAD_PROP(Color, deck_color, p_value)
	TR_ROAD_PROP(Color, edge_color, p_value)
	TR_ROAD_PROP(Color, rail_color, p_value)
	TR_ROAD_PROP(Color, underside_color, p_value)
	TR_ROAD_PROP(bool, collision_enabled, p_value)
#undef TR_ROAD_PROP
	// clang-format on

	void set_cross_section(const Ref<Curve2D> &p_curve);
	Ref<Curve2D> get_cross_section() const { return cross_section; }
	void set_material(const Ref<Material> &p_material);
	Ref<Material> get_material() const { return material; }
};

} // namespace godot

VARIANT_ENUM_CAST(godot::TerrainSplineRoad::Profile);
VARIANT_ENUM_CAST(godot::TerrainSplineRoad::Sampling);
VARIANT_ENUM_CAST(godot::TerrainSplineRoad::HeightSource);

#endif // TR_ROAD_H
