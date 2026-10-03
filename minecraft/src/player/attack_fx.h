#ifndef ATTACK_FX_H
#define ATTACK_FX_H

#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>

namespace godot {

/**
 * AttackFx — the look of the hit-button attacks (no gameplay here).
 * -----------------------------------------------------------------
 * The skin has no attack clips, so the attacks are procedural poses on the skin's transform
 * (the physics body never rotates for them), restored exactly on stop():
 *   Spin: one full whirl (fast start, eased stop) + squash & stretch, while a flat swoosh
 *         ring flashes out from the waist to the hit radius.
 *   Dive: tuck on the wind-up, then lean back so the feet lead along the dive (kick), stretched.
 *   Burst: an impact ring that pops out where the dive connects; runs on its own via tick(),
 *         so it outlives the attack state.
 */
class AttackFx {
public:
	void setup(
			Node3D *p_owner,
			Node3D *p_skin
	);

	void play_spin(float p_radius);
	/// p_progress: 0 at the start of the spin .. 1 at its end.
	void update_spin(float p_progress);

	void play_dive();
	/// p_lean: pitch in radians (positive = lean back / feet forward). p_stretch: 0..1.
	void update_dive(
			float p_lean,
			float p_stretch
	);

	/// Impact ring at `p_at` (world), animated by tick().
	void burst(const Vector3 &p_at);
	/// Restore the skin, hide the spin ring (a running burst keeps going).
	void stop();
	/// Advance the burst. Call every physics frame.
	void tick(float p_dt);

private:
	Node3D *owner = nullptr;
	Node3D *skin = nullptr;
	Transform3D skin_base; ///< Rest transform of the skin, restored on stop().
	MeshInstance3D *ring = nullptr;
	Ref<StandardMaterial3D> ring_mat;
	float radius = 2.0f;
	bool posing = false;
	float burst_t = -1.0f; ///< <0: no burst running.

	void _begin_pose();
	void _set_skin_pose(const Basis &p_extra_before, const Vector3 &p_scale);
	void _place_ring(
			const Vector3 &p_world,
			float p_radius,
			float p_alpha
	);
};

} // namespace godot

#endif // ATTACK_FX_H
