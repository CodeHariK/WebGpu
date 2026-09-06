/**
 * @file tr_cliff.h
 * @brief TerrainSplineCliff: a stylized cliff wall mesh hung from its parent spline's top edge.
 */
#ifndef TR_CLIFF_H
#define TR_CLIFF_H

#include "utils/spline3d/procedural_spline3d.h"
#include <godot_cpp/classes/collision_shape3d.hpp>
#include <godot_cpp/classes/curve.hpp>
#include <godot_cpp/classes/fast_noise_lite.hpp>
#include <godot_cpp/classes/gradient.hpp>
#include <godot_cpp/classes/material.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/static_body3d.hpp>
#include <godot_cpp/variant/packed_color_array.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>
#include <godot_cpp/variant/packed_vector3_array.hpp>
#include <vector>

namespace godot {

/**
 * @class TerrainSplineCliff
 * @brief SplineComponent that builds a low-poly, flat-shaded cliff face along its parent
 * ProceduralSpline3D. The spline (with its Y) is the TOP EDGE, pushed `base_offset` metres sideways so it
 * can sit exactly on the rim of a plateau made by a TerrainSplineDeformer (spline_width + falloff);
 * the wall drops `height` metres away from the plateau (closed loops detect their own winding; open
 * curves use the right-hand side of travel, or the left with `flip_side`) as a stack of `strata`
 * layers that step in and out, with optional walkable ledges, bevelled edges, a lip curling over the
 * top and a skirt sinking below the foot. Closed loops also get a flat top cap as a second surface
 * (`top_material`, e.g. grass or snow), so the cliff is a complete, self-contained mesa: no terrain
 * deformation is needed or expected. Colours come from a Gradient per stratum (vertex colours, no
 * textures). Deterministic from `seed`.
 *
 * It never modifies the terrain and needs no compositor: it owns an internal MeshInstance3D and,
 * when `collision_enabled`, a StaticBody3D with a trimesh shape (ledges are standable). Only the
 * parameters are saved with the scene. Rebuilds are coalesced to one per frame and triggered by the
 * parent's `spline_changed` and by property edits.
 *
 * Implementation:
 *  - tr_cliff.cpp          bindings, properties, rebuild scheduling, child nodes
 *  - tr_cliff_profile.cpp  stations along the spline and the per-station cross-section
 *  - tr_cliff_mesh.cpp     stitching stations into a flat-shaded, vertex-coloured mesh
 */
class TerrainSplineCliff : public SplineComponent {
	GDCLASS(TerrainSplineCliff,
			SplineComponent)

public:
	/// One sample along the spline: where the top edge is and which way "out" (down the face) points.
	struct Station {
		Vector3 top; // Local-space top edge position
		Vector3 out; // Unit horizontal direction away from the plateau
		float distance = 0.0f; // Arc length from the spline start
		float wall_height = 0.0f; // Top edge to foot at this station (before the skirt)
	};

	/*
	 * How far down the wall goes.
	 *  RELATIVE - `height` metres below the top edge everywhere (times height_curve).
	 *  ABSOLUTE - down to world level `bottom_y`, so a top edge that rises and falls still meets one floor.
	 *  GROUND   - raycast at build time from just outside the wall and stop at the hit (needs collision).
	 */
	enum BottomMode { BOTTOM_RELATIVE = 0, BOTTOM_ABSOLUTE = 1, BOTTOM_GROUND = 2 };
	/// One corner of the cross-section: horizontal offset from the top edge (+ = out) and depth below it.
	struct ProfilePoint {
		float offset = 0.0f;
		float depth = 0.0f;
		int stratum = 0; // Which layer's colour it takes
	};

private:
	// ---- Shape ----
	BottomMode bottom_mode = BOTTOM_RELATIVE;
	float height = 12.0f;
	float bottom_y = 0.0f; // ABSOLUTE: world Y of the foot
	float base_offset = 0.0f; // Extra horizontal distance from the spline to the top edge
	bool rim_from_deformer = true; // Add a sibling TerrainSplineDeformer's width + falloff so the wall wraps its slope
	Ref<Curve> height_curve; // Optional multiplier over normalized arc length
	bool flip_side = false;
	float segment_length = 3.0f;
	int strata = 4;
	float strata_variation = 0.4f; // Random layer thickness spread, 0..1
	float step_out = 0.8f; // Max random in/out step between layers, metres
	float ledge_chance = 0.3f; // 0..1, per layer, varies along the wall
	float ledge_depth = 1.5f; // Extra outward step of a ledge
	float bevel = 0.3f; // Chamfer on each layer's outer top edge
	float lip = 0.6f; // How far the top curls back over the plateau
	float skirt = 2.0f; // Continuation below the foot, hides the ground seam
	float noise_amplitude = 0.5f; // Along-wall wobble of each layer's offset
	float noise_frequency = 0.08f; // Per metre of arc
	float noise_quantize = 0.25f; // Snap wobble to this step for a blocky look (0 = smooth)
	float column_width = 0.0f; // Sample the wobble per `column_width` metres of wall: vertical panels (0 = per station)
	bool cap_ends = true;
	bool cap_top = true; // Closed loops: build the plateau surface as a second material slot (grass/snow)
	float cap_resolution = 4.0f; // Interior sample spacing of the top surface, metres
	float cap_dome = 0.0f; // Raise the top towards its middle by this much (pillowy grass top)
	int cap_smoothing = 30; // Laplacian relaxation passes over the interior heights
	int seed = 1;

	// ---- Look ----
	Ref<Gradient> colors; // Stratum colour: 0 = top layer, 1 = bottom layer
	Ref<Material> material; // Wall (surface 0); default: vertex-colour StandardMaterial3D
	Color top_color = Color(0.45f, 0.72f, 0.30f); // Cap vertex colour when top_material is unset
	Ref<Material> top_material; // Cap (surface 1)
	bool collision_enabled = true;

	// ---- Internals ----
	MeshInstance3D *mesh_instance = nullptr;
	StaticBody3D *static_body = nullptr;
	CollisionShape3D *collision_shape = nullptr;
	Ref<FastNoiseLite> _noise;
	Ref<Material> _fallback_material; // Vertex-colour material for unset slots
	bool _rebuild_queued = false;
	ProceduralSpline3D *_watched_spline = nullptr;

	// tr_cliff.cpp
	void _connect_spline();
	void _disconnect_spline();
	void _ensure_nodes();
	void _apply_materials();
	void _update_collision(const Ref<ArrayMesh> &p_mesh);
	void _make_default_colors();

	// tr_cliff_profile.cpp
	float _rim_offset() const;
	bool _build_stations(
			std::vector<Station> &r_stations,
			bool &r_closed
	) const;
	void _build_profile(
			const Station &p_station,
			float p_total_length,
			bool p_closed,
			std::vector<ProfilePoint> &r_profile
	) const;
	float
	_wobble(float p_distance,
			int p_stratum,
			float p_channel) const;

	// tr_cliff_mesh.cpp
	void _build_cap(
			const std::vector<Vector3> &p_rim,
			PackedVector3Array &r_vertices,
			PackedVector3Array &r_normals,
			PackedColorArray &r_colors,
			PackedInt32Array &r_indices
	) const;
	Ref<ArrayMesh> _build_mesh(
			const std::vector<Station> &p_stations,
			const std::vector<std::vector<ProfilePoint>> &p_profiles,
			bool p_closed
	) const;

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	TerrainSplineCliff();
	~TerrainSplineCliff();

	/// Rebuilds the mesh now (normally scheduled via queue_rebuild).
	void rebuild();
	void queue_rebuild();
	void _on_spline_changed();

	float get_spline_padding() const override {
		return Math::abs(base_offset) + skirt + (step_out + ledge_depth) * strata;
	}

	// clang-format off
#define TR_CLIFF_PROP(m_type, m_name, m_clamp) \
	void set_##m_name(m_type p_value) {        \
		m_name = m_clamp;                      \
		queue_rebuild();                       \
	}                                          \
	m_type get_##m_name() const { return m_name; }

	TR_CLIFF_PROP(float, height, MAX(0.1f, p_value))
	TR_CLIFF_PROP(float, base_offset, p_value)
	TR_CLIFF_PROP(bool, rim_from_deformer, p_value)
	TR_CLIFF_PROP(bool, flip_side, p_value)
	TR_CLIFF_PROP(float, segment_length, MAX(0.5f, p_value))
	TR_CLIFF_PROP(int, strata, CLAMP(p_value, 1, 32))
	TR_CLIFF_PROP(float, strata_variation, CLAMP(p_value, 0.0f, 1.0f))
	TR_CLIFF_PROP(float, step_out, MAX(0.0f, p_value))
	TR_CLIFF_PROP(float, ledge_chance, CLAMP(p_value, 0.0f, 1.0f))
	TR_CLIFF_PROP(float, ledge_depth, MAX(0.0f, p_value))
	TR_CLIFF_PROP(float, bevel, MAX(0.0f, p_value))
	TR_CLIFF_PROP(float, lip, MAX(0.0f, p_value))
	TR_CLIFF_PROP(float, skirt, MAX(0.0f, p_value))
	TR_CLIFF_PROP(float, noise_amplitude, MAX(0.0f, p_value))
	TR_CLIFF_PROP(float, noise_frequency, MAX(0.0f, p_value))
	TR_CLIFF_PROP(float, noise_quantize, MAX(0.0f, p_value))
	TR_CLIFF_PROP(float, column_width, MAX(0.0f, p_value))
	TR_CLIFF_PROP(float, bottom_y, p_value)
	TR_CLIFF_PROP(bool, cap_ends, p_value)
	TR_CLIFF_PROP(bool, cap_top, p_value)
	TR_CLIFF_PROP(float, cap_resolution, MAX(0.5f, p_value))
	TR_CLIFF_PROP(float, cap_dome, p_value)
	TR_CLIFF_PROP(int, cap_smoothing, CLAMP(p_value, 0, 500))
	TR_CLIFF_PROP(Color, top_color, p_value)
	TR_CLIFF_PROP(int, seed, p_value)
	TR_CLIFF_PROP(bool, collision_enabled, p_value)
#undef TR_CLIFF_PROP
	// clang-format on

	void set_bottom_mode(BottomMode p_mode) {
		bottom_mode = p_mode;
		queue_rebuild();
	}
	BottomMode get_bottom_mode() const { return bottom_mode; }
	void set_height_curve(const Ref<Curve> &p_curve);
	Ref<Curve> get_height_curve() const { return height_curve; }
	void set_colors(const Ref<Gradient> &p_colors);
	Ref<Gradient> get_colors() const { return colors; }
	void set_material(const Ref<Material> &p_material);
	Ref<Material> get_material() const { return material; }
	void set_top_material(const Ref<Material> &p_material);
	Ref<Material> get_top_material() const { return top_material; }
	void _on_resource_changed();
};

} // namespace godot

VARIANT_ENUM_CAST(godot::TerrainSplineCliff::BottomMode);

#endif // TR_CLIFF_H
