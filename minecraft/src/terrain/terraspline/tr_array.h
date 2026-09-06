/**
 * @file tr_array.h
 * @brief TerrainSplineArray: meshes placed at regular intervals along the parent spline.
 */
#ifndef TR_ARRAY_H
#define TR_ARRAY_H

#include "utils/spline3d/procedural_spline3d.h"
#include <godot_cpp/classes/mesh.hpp>
#include <godot_cpp/classes/multi_mesh_instance3d.hpp>
#include <godot_cpp/classes/shape3d.hpp>
#include <godot_cpp/classes/static_body3d.hpp>
#include <vector>

namespace godot {

/**
 * @class TerrainSplineArray
 * @brief SplineComponent that instances one mesh every `spacing` metres along its parent
 * ProceduralSpline3D - guardrail posts, lamp posts, checkpoint gates, boost pads, bridge pillars.
 * Regular where TerrainSplineScatter is random. Placement follows the spline frame (tangent, and the
 * tilt when `follow_tilt`), offset sideways on one or both sides, with deterministic jitter from
 * `seed`. `stretch_to_ground` raycasts down from each instance and scales it to reach the ground -
 * pillars under a floating TerrainSplineRoad. One MultiMeshInstance3D (one draw call) plus an optional
 * StaticBody3D with a collision shape per instance; both internal, nothing saved but parameters.
 *
 * Implementation:
 *  - tr_array.cpp        bindings, properties, rebuild scheduling, child nodes
 *  - tr_array_place.cpp  stations along the spline -> instance transforms
 */
class TerrainSplineArray : public SplineComponent {
	GDCLASS(TerrainSplineArray,
			SplineComponent)

public:
	enum Side { SIDE_CENTER = 0, SIDE_LEFT = 1, SIDE_RIGHT = 2, SIDE_BOTH = 3 };

private:
	// ---- What ----
	Ref<Mesh> mesh; // Pivot at the base, +Y up, facing -Z when align_to_tangent matters
	Ref<Shape3D> collision_shape;
	bool collision_enabled = false;
	bool mesh_centered = false; // Godot primitives are centred on their origin; props usually sit on their base

	// ---- Where ----
	float spacing = 10.0f;
	float start_offset = 0.0f; // Arc length before the first instance
	float section_start = 0.0f; // 0..0 = whole spline
	float section_end = 0.0f;
	Side side = SIDE_CENTER;
	float lateral_offset = 0.0f; // Metres from the spline (per side)
	float vertical_offset = 0.0f;
	bool align_to_tangent = true; // Yaw so the mesh's -Z follows the spline (else world-aligned)
	bool follow_tilt = false; // Also take the spline's roll (banked rails); off = stays upright
	bool face_inward = true; // With LEFT/RIGHT/BOTH: turn the instance to face the spline
	bool stretch_to_ground = false; // Scale Y so the base reaches the ground below (raycast)
	float ground_max_distance = 500.0f;

	// ---- Variation ----
	Vector3 scale = Vector3(1, 1, 1);
	float scale_jitter = 0.0f; // 0..1 uniform random scale range
	float yaw_jitter_deg = 0.0f;
	float spacing_jitter = 0.0f; // 0..1 of spacing
	int seed = 1;

	// ---- Internals ----
	MultiMeshInstance3D *mmi = nullptr;
	StaticBody3D *static_body = nullptr;
	bool _rebuild_queued = false;
	ProceduralSpline3D *_watched_spline = nullptr;
	int _ground_retries = 0; // stretch_to_ground: the terrain streams in after us; retry a few times
	mutable bool _ground_missing = false;

	// tr_array.cpp
	void _connect_spline();
	void _disconnect_spline();
	void _ensure_nodes();
	void _apply_transforms(const std::vector<Transform3D> &p_transforms);
	void _update_collision(const std::vector<Transform3D> &p_transforms);

	// tr_array_place.cpp
	bool _build_transforms(std::vector<Transform3D> &r_transforms) const;
	float _ground_drop(const Vector3 &p_local_from) const;

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	TerrainSplineArray();
	~TerrainSplineArray();

	void rebuild();
	void queue_rebuild();
	void _on_spline_changed();
	void _on_resource_changed();

	float get_spline_padding() const override { return Math::abs(lateral_offset) + 2.0f; }

	// clang-format off
#define TR_ARRAY_PROP(m_type, m_name, m_clamp) \
	void set_##m_name(m_type p_value) {        \
		m_name = m_clamp;                      \
		queue_rebuild();                       \
	}                                          \
	m_type get_##m_name() const { return m_name; }

	TR_ARRAY_PROP(bool, collision_enabled, p_value)
	TR_ARRAY_PROP(bool, mesh_centered, p_value)
	TR_ARRAY_PROP(float, spacing, MAX(0.1f, p_value))
	TR_ARRAY_PROP(float, start_offset, MAX(0.0f, p_value))
	TR_ARRAY_PROP(float, section_start, MAX(0.0f, p_value))
	TR_ARRAY_PROP(float, section_end, MAX(0.0f, p_value))
	TR_ARRAY_PROP(Side, side, p_value)
	TR_ARRAY_PROP(float, lateral_offset, p_value)
	TR_ARRAY_PROP(float, vertical_offset, p_value)
	TR_ARRAY_PROP(bool, align_to_tangent, p_value)
	TR_ARRAY_PROP(bool, follow_tilt, p_value)
	TR_ARRAY_PROP(bool, face_inward, p_value)
	TR_ARRAY_PROP(bool, stretch_to_ground, p_value)
	TR_ARRAY_PROP(float, ground_max_distance, MAX(1.0f, p_value))
	TR_ARRAY_PROP(Vector3, scale, p_value)
	TR_ARRAY_PROP(float, scale_jitter, CLAMP(p_value, 0.0f, 1.0f))
	TR_ARRAY_PROP(float, yaw_jitter_deg, CLAMP(p_value, 0.0f, 180.0f))
	TR_ARRAY_PROP(float, spacing_jitter, CLAMP(p_value, 0.0f, 0.9f))
	TR_ARRAY_PROP(int, seed, p_value)
#undef TR_ARRAY_PROP
	// clang-format on

	void set_mesh(const Ref<Mesh> &p_mesh);
	Ref<Mesh> get_mesh() const { return mesh; }
	void set_collision_shape(const Ref<Shape3D> &p_shape);
	Ref<Shape3D> get_collision_shape() const { return collision_shape; }
};

} // namespace godot

VARIANT_ENUM_CAST(godot::TerrainSplineArray::Side);

#endif // TR_ARRAY_H
