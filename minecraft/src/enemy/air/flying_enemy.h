#ifndef FLYING_ENEMY_H
#define FLYING_ENEMY_H

#include "../../combat/projectile/projectile_profile.h"
#include "../../utils/fx/puff_emitter.h"
#include "../enemy_base.h"

#include <godot_cpp/classes/standard_material3d.hpp>

#include <vector>

namespace godot {

class ProjectileLauncher;

/**
 * FlyingEnemy — shared base for cartoon air enemies (HelicopterEnemy, UfoEnemy).
 * ------------------------------------------------------------------------------
 * Owns everything that isn't personality:
 *   - Flight: kinematic (no physics solver): steer a velocity toward a goal with an arrive
 *     slowdown, holding `altitude` above whatever terrain is below (one ray, refreshed a few
 *     times a second). get_velocity() is kept current so homing shots and camera lead work.
 *   - Look: a `model` pivot that the subclass fills with toy parts; banks into turns, pitches
 *     with speed, squash-"boings" and flashes white when hit.
 *   - Attack plumbing: a ProjectileLauncher at `muzzle_offset` with `projectile_profile`
 *     (the subclass decides WHEN to call _fire()).
 *   - Death: no instant vanish. It spins out trailing smoke, drops, and pops in a puff burst
 *     when it hits the ground. It leaves the EnemyManager at once, so nothing targets a wreck.
 *   - Collision: a sphere on the ENEMY layer, so arrows, missiles, parries, dive kicks and
 *     spin attacks all find it.
 * Subclasses implement _build_model() and _think(); everything else is optional.
 */
class FlyingEnemy : public EnemyBase {
	GDCLASS(FlyingEnemy,
			EnemyBase)

protected:
	// --- Tunables (inspector) ---
	float max_speed = 8.0f; ///< m/s
	float acceleration = 10.0f; ///< m/s^2: how fast it changes velocity (lower = floatier)
	float altitude = 8.0f; ///< Height above the ground it holds (m).
	float aggro_range = 40.0f; ///< Engages the player within this range (m).
	float fire_interval = 3.0f; ///< Seconds between attacks.
	float body_radius = 1.2f; ///< Collision sphere radius (m).
	Ref<ProjectileProfile> projectile_profile;

	// --- Runtime ---
	Node3D *model = nullptr; ///< Pivot for all visual parts: bank / pitch / boing apply here.
	ProjectileLauncher *launcher = nullptr;
	Vector3 muzzle_offset = Vector3(0, -0.5f, -1.0f);
	Vector3 home; ///< Where it was placed: patrols around here when idle.
	Vector3 flight_velocity; ///< Mirrored into CharacterBody3D::set_velocity for body_velocity().
	float age = 0.0f;
	float fire_timer = 0.0f;
	float bank = 0.0f;
	float hit_flash = 0.0f;
	float boing = 0.0f; ///< squash spring position
	float boing_vel = 0.0f;
	std::vector<Ref<StandardMaterial3D>> flash_materials;
	std::vector<Color> flash_base_colors;
	PuffEmitter smoke;

	// crash
	bool crashing = false;
	float crash_time = 0.0f;
	float crash_spin = 0.0f;
	float smoke_timer = 0.0f;
	bool popped = false;

	// ground cache
	float ground_y = 0.0f;
	float ground_timer = 0.0f;

	static void _bind_methods();

	// --- Subclass hooks ---
	/// Fill `model` with parts. Register flashable materials with _flashable().
	virtual void _build_model() {}
	/// Decide where to fly and when to fire. Call _steer_to() / _fire().
	virtual void _think(
			float p_dt,
			Node3D *p_target
	) {}
	/// Per-frame cosmetic animation (rotors, lights...). Runs while crashing too.
	virtual void _animate(float p_dt) {}
	/// Profile used when none is set in the inspector.
	virtual String _default_profile_path() const { return String(); }

	// --- Helpers for subclasses ---
	Ref<StandardMaterial3D> _flashable(const Color &p_color);
	/// Accelerate toward `p_goal` (world), slowing to arrive. Altitude is the caller's job.
	void _steer_to(
			const Vector3 &p_goal,
			float p_dt,
			float p_speed_scale = 1.0f
	);
	/// The point `altitude` above the ground at (x, z).
	Vector3 _hover_point(
			const Vector3 &p_xz,
			float p_altitude
	);
	float _ground_height_at(const Vector3 &p_point);
	/// Turn the body to face a world direction (yaw only), smoothly.
	void _face(
			const Vector3 &p_dir,
			float p_dt,
			float p_rate = 4.0f
	);
	void _fire(Node3D *p_target);
	Node3D *_current_target();

private:
	void _build_common();
	void _update_flight_pose(
			float p_dt,
			const Vector3 &p_prev_velocity
	);
	void _update_hit_fx(float p_dt);
	void _update_crash(float p_dt);

public:
	void _ready() override;
	void _physics_process(double p_delta) override;
	void take_damage(float p_amount) override;
	void die() override;

	bool is_crashing() const { return crashing; }

	void set_max_speed(float p_v) { max_speed = p_v; }
	float get_max_speed() const { return max_speed; }
	void set_acceleration(float p_v) { acceleration = p_v; }
	float get_acceleration() const { return acceleration; }
	void set_altitude(float p_v) { altitude = p_v; }
	float get_altitude() const { return altitude; }
	void set_aggro_range(float p_v) { aggro_range = p_v; }
	float get_aggro_range() const { return aggro_range; }
	void set_fire_interval(float p_v) { fire_interval = p_v; }
	float get_fire_interval() const { return fire_interval; }
	void set_projectile_profile(const Ref<ProjectileProfile> &p_p) { projectile_profile = p_p; }
	Ref<ProjectileProfile> get_projectile_profile() const { return projectile_profile; }
};

} // namespace godot

#endif // FLYING_ENEMY_H
