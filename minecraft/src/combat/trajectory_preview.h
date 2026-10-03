#ifndef TRAJECTORY_PREVIEW_H
#define TRAJECTORY_PREVIEW_H

#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/multi_mesh_instance3d.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/variant/typed_array.hpp>

namespace godot {

/**
 * TrajectoryPreview — a dotted arc showing where a ballistic shot will go.
 * -----------------------------------------------------------------------
 * Call show_arc() every frame while aiming. It steps the same motion the projectile uses
 * (p += v dt, v.y -= g dt), raycasts each step so the arc stops at the first wall/floor/
 * enemy, then lays dots at even spacing along the path and a ring on the landing spot,
 * tilted to the surface. The dots drift toward the landing spot, so the line reads as
 * "flowing" in the shot's direction.
 *
 * Cheap: all dots are one MultiMesh (one draw call); ~1 raycast per step. Children are
 * top-level, so the preview stays in world space however its parent moves.
 */
class TrajectoryPreview : public Node3D {
	GDCLASS(TrajectoryPreview,
			Node3D)

private:
	// --- Tunables ---
	float dot_spacing = 0.45f; ///< Metres between dots along the arc.
	float dot_radius = 0.06f;
	float flow_speed = 1.5f; ///< Dots drift this many metres/second toward the landing spot.
	float max_time = 3.0f; ///< Longest arc simulated (s).
	int max_dots = 48;
	Color color = Color(1.0f, 1.0f, 1.0f, 0.9f);

	// --- Runtime ---
	MultiMeshInstance3D *dots = nullptr;
	MeshInstance3D *ring = nullptr;
	Ref<StandardMaterial3D> dot_mat;
	Ref<StandardMaterial3D> ring_mat;
	float flow = 0.0f;
	float clock = 0.0f;
	bool has_hit = false;
	Vector3 landing;

	void _build();
	void _place_dots(const PackedVector3Array &p_path);
	void _place_ring(
			const Vector3 &p_point,
			const Vector3 &p_normal,
			bool p_hit
	);

protected:
	static void _bind_methods();

public:
	void _ready() override;

	/// Simulate + draw. p_dt drives the dot flow. p_mask: what stops the arc.
	/// Returns true if the arc hits something within max_time (landing point in get_landing()).
	bool show_arc(
			const Vector3 &p_origin,
			const Vector3 &p_velocity,
			float p_gravity,
			float p_dt,
			uint32_t p_mask,
			const TypedArray<RID> &p_exclude = TypedArray<RID>()
	);
	void hide_arc();

	Vector3 get_landing() const { return landing; }
	bool get_has_hit() const { return has_hit; }
	void set_color(const Color &p_color) { color = p_color; }
	Color get_color() const { return color; }
	void set_dot_spacing(float p_v) { dot_spacing = p_v; }
	float get_dot_spacing() const { return dot_spacing; }
	void set_max_time(float p_v) { max_time = p_v; }
	float get_max_time() const { return max_time; }
};

} // namespace godot

#endif // TRAJECTORY_PREVIEW_H
