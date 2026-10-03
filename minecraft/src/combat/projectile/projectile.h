#ifndef PROJECTILE_H
#define PROJECTILE_H

#include "projectile_profile.h"
#include "projectile_visual.h"
#include "target_telegraph.h"

#include <godot_cpp/classes/node3d.hpp>

namespace godot {

/**
 * Projectile — a slow, readable, dodgeable homing missile, arrow or mortar.
 * -----------------------------------------------------------------------
 * Same class for player arrows, boss missiles, turret darts and mortars; a ProjectileProfile
 * holds the numbers. Fire it with fire(); a ProjectileLauncher does that for you.
 *
 * Homing flight phases (each one is a fairness rule):
 *   LAUNCH  unguided climb for `launch_time`: shot steeply up, `launch_gravity` bends it over,
 *           so it reads as a lob and the player sees it coming.
 *   SEEK    steers toward the (partly) predicted aim point, turn capped at `turn_rate`.
 *   COMMIT  within `commit_distance`: homing off, flies straight. Late sidesteps win.
 *   LOST    target left the `lose_lock_angle` cone (or vanished): gives up, wobbles, flies on.
 *   POP     hit something / expired: swell + puff burst, then frees itself.
 *
 * Ballistic (guidance = BALLISTIC): one phase, BALLISTIC. fire() picks the landing spot (the
 * target's ground point, led by its velocity), solves a single arc to it and never steers
 * again. The landing spot is shown as a filling danger zone; impact damages everything
 * within `blast_radius`.
 *
 * Movement is kinematic: one ray from last position to new position per tick (no tunnelling,
 * no physics body), plus a segment-to-centre distance test against the target for a fat hit.
 *
 * Parry: if the target is "parrying" (has `is_parrying()` returning true, e.g. Celeste while
 * dashing or jump-kicking) within `parry_radius`, the shot flips sides, re-targets its shooter,
 * speeds up and leads fully.
 *
 * The cartoon angle shake gets stronger as the shot closes in (see _shake()).
 *
 * Signals: hit(body) (once per body hit or caught in the blast), parried(), expired().
 */
class Projectile : public Node3D {
	GDCLASS(Projectile,
			Node3D)

public:
	enum Phase {
		PHASE_LAUNCH,
		PHASE_SEEK,
		PHASE_COMMIT,
		PHASE_LOST,
		PHASE_BALLISTIC,
		PHASE_POP,
	};

private:
	Ref<ProjectileProfile> profile;
	bool debug_draw = false; ///< Line to the aim point + aim sphere via DebugManager.

	// --- Runtime ---
	uint64_t target_id = 0;
	uint64_t shooter_id = 0;
	Vector3 velocity;
	float speed = 0.0f;
	float lead = 0.0f;
	float age = 0.0f;
	float phase_age = 0.0f;
	Phase phase = PHASE_LAUNCH;
	Vector3 aim;
	Vector3 impact_point; ///< Ballistic: where the arc lands.
	float flight_time = 0.0f; ///< Ballistic: seconds from launch (or parry) to impact.
	bool parried = false;
	bool fired = false;
	ProjectileVisual visual;
	TargetTelegraph telegraph;

	Node3D *_resolve(uint64_t p_id) const;
	void _set_phase(Phase p_phase);
	void _update_phase(
			Node3D *p_target,
			const Vector3 &p_pos
	);
	float _steer(
			Node3D *p_target,
			const Vector3 &p_pos,
			float p_dt
	);
	bool _try_parry(
			Node3D *p_target,
			const Vector3 &p_pos
	);
	bool _sweep(
			Node3D *p_target,
			const Vector3 &p_from,
			const Vector3 &p_to
	);
	bool _is_ballistic() const;
	void _aim_ballistic(
			Node3D *p_target,
			const Vector3 &p_from
	);
	Vector3 _ground_below(
			const Vector3 &p_point,
			Node3D *p_ignore
	) const;
	float _lock(
			Node3D *p_target,
			const Vector3 &p_pos
	) const;
	float _shake(
			Node3D *p_target,
			const Vector3 &p_pos
	) const;
	void _impact(Object *p_hit);
	void _blast(
			const Vector3 &p_pos,
			Object *p_direct
	);
	void _damage(Object *p_hit);
	void _orient();
	void _update_telegraph(
			Node3D *p_target,
			const Vector3 &p_pos,
			float p_dt
	);
	void _draw_debug(const Vector3 &p_pos);
	void _clear_debug();

protected:
	static void _bind_methods();

public:
	Projectile();

	void _ready() override;
	void _exit_tree() override;
	void _physics_process(double p_delta) override;

	/// Launch from `p_origin` toward `p_dir`, homing on `p_target`. `p_shooter` is never hit
	/// by its own shot (and becomes the target if the shot is parried). Either may be null.
	void fire(
			const Vector3 &p_origin,
			const Vector3 &p_dir,
			Node3D *p_target,
			Node3D *p_shooter
	);

	/// Free shot with an explicit launch velocity (e.g. an aimed player arrow): a pure
	/// ballistic arc under the profile's ballistic gravity, no target, never steers.
	void fire_velocity(
			const Vector3 &p_origin,
			const Vector3 &p_velocity,
			Node3D *p_shooter
	);

	void set_profile(const Ref<ProjectileProfile> &p_profile) { profile = p_profile; }
	Ref<ProjectileProfile> get_profile() const { return profile; }
	void set_debug_draw(bool p_on) { debug_draw = p_on; }
	bool get_debug_draw() const { return debug_draw; }

	/// Layers a shot collides with. Aim previews use the same mask so the drawn arc stops
	/// exactly where the real shot would.
	static uint32_t get_hit_mask();

	int get_phase() const { return (int)phase; }
	Vector3 get_velocity() const { return velocity; }
	Vector3 get_aim_point() const { return aim; }
	Vector3 get_impact_point() const { return impact_point; }
	bool is_parried() const { return parried; }
	Node3D *get_target() const { return _resolve(target_id); }
};

} // namespace godot

VARIANT_ENUM_CAST(Projectile::Phase);

#endif // PROJECTILE_H
