#ifndef TARGET_TELEGRAPH_H
#define TARGET_TELEGRAPH_H

#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>

namespace godot {

/**
 * TargetTelegraph — tells the player "this thing is coming for you, and here's where".
 * -----------------------------------------------------------------------------------
 *   Reticle: a spinning ring on the ground under the target. Yellow while tracking, shrinking
 *            and turning red as the projectile closes in; blinks once homing has committed
 *            (that's the cue to sidestep now).
 *   Marker:  a ghost sphere at the predicted aim point (only when the shot leads). Players
 *            learn to "get off the dot".
 *   Zone:    (ballistic shots) a red ring on the landing spot, sized to the blast, with a disc
 *            that fills from the centre as impact approaches and a ring that blinks faster.
 *            The spot is fixed at launch: get out of the circle before it's full.
 * Both are top-level children of the projectile, so they vanish with it.
 */
class TargetTelegraph {
public:
	void build(Node3D *p_owner);
	/// p_lock: 0 (far) .. 1 (about to hit). p_committed: homing has switched off.
	void update(
			float p_dt,
			Node3D *p_target,
			const Vector3 &p_aim,
			float p_lock,
			bool p_committed,
			bool p_show_aim
	);
	/// Ballistic impact zone at `p_center` (ground point). p_progress: 0 at launch .. 1 at impact.
	void update_zone(
			float p_dt,
			const Vector3 &p_center,
			float p_radius,
			float p_progress
	);
	void hide();

private:
	Node3D *owner = nullptr;
	MeshInstance3D *reticle = nullptr;
	MeshInstance3D *marker = nullptr;
	MeshInstance3D *zone_ring = nullptr;
	MeshInstance3D *zone_fill = nullptr;
	Ref<StandardMaterial3D> zone_ring_mat;
	Ref<StandardMaterial3D> zone_fill_mat;
	Ref<StandardMaterial3D> reticle_mat;
	Ref<StandardMaterial3D> marker_mat;
	float spin = 0.0f;
	float clock = 0.0f;
	float blink_phase = 0.0f;

	static Ref<StandardMaterial3D> _glow(float p_alpha);
	MeshInstance3D *_top_level_mesh(
			const Ref<Mesh> &p_mesh,
			const Ref<StandardMaterial3D> &p_mat
	);
	void _build_zone();
	float _ground_y(Node3D *p_target) const;
};

} // namespace godot

#endif // TARGET_TELEGRAPH_H
