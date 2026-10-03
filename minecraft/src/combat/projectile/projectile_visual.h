#ifndef PROJECTILE_VISUAL_H
#define PROJECTILE_VISUAL_H

#include "../../utils/fx/puff_emitter.h"
#include "projectile_profile.h"

#include <godot_cpp/classes/mesh.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>

namespace godot {

/**
 * ProjectileVisual — the cartoon look of a homing projectile. Visual only.
 * -----------------------------------------------------------------------
 * The projectile node flies the exact steering path (that's the hitbox). Everything here
 * moves a `pivot` child underneath it, so the wobble can be as silly as we like without
 * lying about where the projectile really is.
 *
 *   Missile: chubby capsule with eyes and cross fins; banks into turns, leaves smoke puffs.
 *            Two wobble styles (profile `wobble_style`):
 *              Shake     - the nose twitches with a fast, irregular angle jitter (layered sines
 *                          at odd ratios, never a clean wave): straining to reach you. Menacing.
 *              Corkscrew - the model circles the true path with a matching sway and breathes
 *                          (squash & stretch): a goofy toy rocket.
 *   Arrow:   thin shaft, cone tip, fletching. Fishtails on release (decaying wiggle),
 *            fletching flutters, banks into turns.
 *   Both:    pop-in with overshoot on launch; swell + puff burst when it dies.
 * `p_shake` scales the shake: the owner ramps it up as the shot closes in (more menace).
 */
class ProjectileVisual {
public:
	void build(
			Node3D *p_owner,
			const Ref<ProjectileProfile> &p_profile
	);
	/// p_turn_rate: signed yaw rate (rad/s) of the flight path, for banking.
	void update(
			float p_dt,
			float p_age,
			float p_turn_rate,
			float p_shake
	);
	void start_pop();
	/// Advances the death animation; returns true when it's finished (safe to free).
	bool update_pop(float p_dt);

private:
	static const int PUFF_COUNT = 12;

	Node3D *owner = nullptr;
	Node3D *pivot = nullptr; ///< Everything wobbly hangs off this.
	Node3D *fletching = nullptr; ///< Arrow only: flutters on its own.
	Ref<StandardMaterial3D> body_mat;
	Ref<StandardMaterial3D> accent_mat;
	Ref<StandardMaterial3D> dark_mat;

	int style = ProjectileProfile::STYLE_MISSILE;
	int wobble_style = ProjectileProfile::WOBBLE_SHAKE;
	float amp = 0.1f;
	float freq = 2.5f;
	float model_scale = 1.0f;
	float phase = 0.0f; ///< Random so two missiles never wobble in sync.
	float bank = 0.0f;
	float pop_t = -1.0f;

	PuffEmitter puffs; ///< Missile smoke trail + pop burst.
	float puff_timer = 0.0f;

	static Ref<StandardMaterial3D> _toon(const Color &p_color);
	MeshInstance3D *_part(
			Node3D *p_parent,
			const Ref<Mesh> &p_mesh,
			const Ref<StandardMaterial3D> &p_mat,
			const Vector3 &p_pos,
			const Vector3 &p_rot = Vector3()
	);
	void _build_missile();
	void _build_arrow();
	void _apply_shake(
			float p_a,
			float p_k,
			float p_shake,
			float p_grow
	);
	void _apply_corkscrew(
			float p_a,
			float p_k,
			float p_settle,
			float p_grow
	);
};

} // namespace godot

#endif // PROJECTILE_VISUAL_H
