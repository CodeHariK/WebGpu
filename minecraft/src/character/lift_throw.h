#ifndef LIFT_THROW_H
#define LIFT_THROW_H

#include "../utils/fx/puff_emitter.h"

#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/rigid_body3d.hpp>

#include <cstdint>
#include <vector>

namespace godot {

class GameCamera;
class TrajectoryPreview;

/**
 * LiftThrow — Hulk-style "F: lift, F: throw" for any RigidBody3D (cars, crates, balls).
 * -----------------------------------------------------------------------------------
 * Owned by a character (like BowAim). Each tick: update(dt, f_held, f_just_pressed, cam, active).
 *
 *   Lift (F near a rigid body, within `reach`): the body is frozen kinematic (so it can't
 *     shove or be shoved), collision with the carrier is excepted, and it is hoisted overhead
 *     with an overshoot, settling upright. While held it lags a little behind a bobbing hold
 *     point above the head and turns with the carrier. Carrying slows the carrier
 *     (carry_speed_scale; the owner applies it).
 *   Aim (press and hold F again): the throw arc is drawn with a TrajectoryPreview. If an enemy
 *     is roughly where the camera looks (within throw_range, flyers preferred) the throw is
 *     auto-aimed at it: a ballistic arc to where it WILL be (its velocity led), so moving
 *     helicopters get hit. No target: thrown along the camera with `loft`.
 *   Throw (release F): unfrozen with that velocity plus an end-over-end tumble. For a few
 *     seconds the flight is watched: passing an enemy hits it for `impact_damage` with a
 *     puff burst (enemies are kinematic, so this is a distance test, not physics contact).
 * Arc gravity = project gravity * gravity_scale, plus an ArcadeVehicle's in-air downforce,
 * so the preview, the auto-aim and the real flight agree.
 */
class LiftThrow {
public:
	// --- Tunables ---
	float reach = 3.2f; ///< How close a body must be to lift it (m, centre to centre).
	float hoist_time = 0.35f; ///< Seconds to swing it overhead.
	float throw_speed = 18.0f; ///< Horizontal speed of an auto-aimed throw (m/s); free throws use it as launch speed.
	float throw_range = 45.0f; ///< Auto-aim reach (m).
	float loft = 0.35f; ///< Added to camera pitch for a free throw (rad).
	float carry_speed_scale = 0.6f; ///< Carrier move speed while holding.
	float impact_damage = 3.0f; ///< Damage to an enemy hit by a thrown body.

	void setup(
			Node3D *p_owner,
			float p_owner_half_height
	);
	void update(
			float p_dt,
			bool p_held,
			bool p_just_pressed,
			GameCamera *p_cam,
			bool p_active
	);
	/// Let go without throwing (restores the body where it is).
	void drop();

	bool is_holding() const { return phase != PHASE_NONE; }
	bool is_aiming() const { return aiming; }
	float get_aim_yaw() const { return aim_yaw; }

private:
	enum Phase {
		PHASE_NONE,
		PHASE_HOIST,
		PHASE_HOLD,
	};

	Node3D *owner = nullptr;
	float owner_half_height = 1.0f;
	TrajectoryPreview *preview = nullptr;
	PuffEmitter puffs;

	// held body
	Phase phase = PHASE_NONE;
	uint64_t held_id = 0;
	bool saved_freeze = false;
	int saved_freeze_mode = 0;
	float held_radius = 1.0f;
	float hoist_t = 0.0f;
	Transform3D hoist_from;
	float yaw_offset = 0.0f; ///< body yaw relative to the carrier, kept while carrying
	Vector3 carry_pos;
	Vector3 carry_vel;
	float clock = 0.0f;
	bool aiming = false;
	float aim_yaw = 0.0f;

	// thrown body (impact watch)
	uint64_t thrown_id = 0;
	RID camera_rid; ///< Body the follow camera's rays ignore, from lift until the throw lands.
	float thrown_t = -1.0f;
	float thrown_radius = 1.0f;
	std::vector<uint64_t> already_hit;

	RigidBody3D *_resolve(uint64_t p_id) const;
	RigidBody3D *_find_liftable() const;
	static float _radius_of(RigidBody3D *p_body);
	static float _gravity_of(RigidBody3D *p_body);
	Vector3 _hold_point() const;
	Node3D *_find_target(
			GameCamera *p_cam,
			const Vector3 &p_from
	) const;
	Vector3 _throw_velocity(
			GameCamera *p_cam,
			RigidBody3D *p_body
	);
	void _lift(RigidBody3D *p_body);
	void _carry(
			RigidBody3D *p_body,
			float p_dt
	);
	void _throw(
			RigidBody3D *p_body,
			const Vector3 &p_velocity,
			GameCamera *p_cam
	);
	void _release(RigidBody3D *p_body);
	void _watch_thrown(float p_dt);
};

} // namespace godot

#endif // LIFT_THROW_H
