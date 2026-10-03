#ifndef CELESTE_CONTROLLER_H
#define CELESTE_CONTROLLER_H

#include "../character/character_animator.h"
#include "../character/character_audio.h"
#include "attack_fx.h"
#include "../character/bow_aim.h"
#include "../character/lift_throw.h"
#include "../character/character_controls.h"
#include "../game_manager/player_input.h"
#include "../cui/tuning_section.h"
#include <godot_cpp/classes/character_body3d.hpp>
#include <godot_cpp/classes/node3d.hpp>

namespace godot {

class CelesteState;
class CelesteIdleState;
class CelesteMoveState;
class CelesteJumpState;
class CelesteFallState;
class CelesteGroundedState;
class CelesteAirborneState;
class CelesteDoubleJumpState;
class CelesteAttackState;
class CelesteDiveKickState;

class CelesteController : public CharacterBody3D {
	GDCLASS(CelesteController,
			CharacterBody3D)

	friend class CelesteState;
	friend class CelesteGroundedState;
	friend class CelesteIdleState;
	friend class CelesteMoveState;
	friend class CelesteAirborneState;
	friend class CelesteJumpState;
	friend class CelesteFallState;
	friend class CelesteDoubleJumpState;
	friend class CelesteDashState;
	friend class CelesteAttackState;
	friend class CelesteDiveKickState;

private:
	// Movement Settings (Celeste-style)
	float max_speed = 10.0f;
	float acceleration = 80.0f;
	float friction = 60.0f;
	float air_resistance = 20.0f;
	float sprint_multiplier = 1.5f;

	// Jump Math (Kinematic)
	float jump_height = 2.5f;
	float jump_time_to_peak = 0.4f;
	float jump_time_to_descent = 0.2f;
	float coyote_time_max = 0.15f;
	float jump_buffer_max = 0.1f;

	// Calculated Values
	float _jump_velocity0 = 0.0f;
	float _jump_gravity = 0.0f;
	float _fall_gravity = 0.0f;

	// Dash parameters
	float dash_speed = 30.0f;
	float dash_duration = 0.15f;
	float dash_cooldown = 0.3f;

	// Fall Math (Kinematic)
	float max_fall_velocity = 20.0f;

	// Melee Parameters
	float half_height = 1.0f;
	float melee_range = 2.2f; ///< Spin attack hit radius (m).
	bool can_air_spin = true; ///< One air spin per airtime (refilled on landing).
	float dive_range = 12.0f; ///< Air + hit dives onto an enemy within this range (m).
	uint32_t melee_target_layer = 4; // Layer 3 (bit 2)

	float ride_height = 0.1f;
	float min_ride_height = 0.1f;
	float max_ride_height = 1.2f;
	float ride_height_speed = 5.0f;
	float spring_stiffness = 800.0f;
	float spring_damping = 40.0f;

	// --- Control scheme (shared logic: character/character_controls.h) ---
	// 0 = Steer (MODE_CHARACTER heading cam), 1 = Camera Relative (MODE_PLATFORMER),
	// 2 = Legacy (the original TPS / Fixed camera behaviour). V flips 0 <-> 1 in game.
	int control_scheme = 0;
	CharacterControls controls;
	BowAim bow; ///< Hold R: draw, see the arc, release to fire an arrow.
	LiftThrow lift; ///< F: lift a rigid body overhead; F again (hold to aim): throw it.
	float parry_window = 0.2f; ///< Seconds after pressing hit during which projectiles are parried.
	float _parry_timer = 0.0f;
	bool _scheme_key_was_down = false;
	bool _active = false; ///< This frame: are we the GameManager's active target?

	// --- Animated skin + sounds (optional child exposing idle/move/jump/fall) ---
	Node3D *_skin = nullptr;
	CharacterAnimator _animator;
	CharacterAudio _audio;
	float _prev_yaw = 0.0f; ///< For the lean (turn-rate) signal.
	bool _jump_event = false; ///< Set when a jump state is entered; consumed by the audio.

	// --- Step detection: last measured sole clearance, so probe rays start at the floor ---
	float _last_ground_dist = 0.0f;
	bool _has_last_ground = false;
	bool _ride_probe_ground = false; ///< Last frame had ground, so the step rays can start at the floor.

	// Runtime State
	bool is_jumping = false;
	bool can_double_jump = true;
	float double_jump_multiplier = 0.8f;
	float coyote_timer = 0.0f;
	float jump_buffer_timer = 0.0f;
	bool is_hovering = false;
	float last_spring_error = 0.0f;
	Vector3 platform_velocity = Vector3(0, 0, 0);

	// Dash state
	float dash_timer = 0.0f;
	float dash_cooldown_timer = 0.0f;
	bool can_dash = true;
	bool is_dashing = false;
	Vector3 dash_direction;

	void _update_jump_math();
	void _update_controls(float p_delta);
	void _update_bow(float p_delta);
	void _update_lift(float p_delta);
	void _update_ride_height(
			const Vector3 &p_bottom,
			float p_delta
	);
	void _find_skin();
	void _update_skin_and_audio(float p_delta);
	void _spin_hit(); ///< Spin attack connects: damage every enemy within melee_range.
	/// Air + hit: the enemy to dive onto (in reach, not above us, roughly ahead), or null.
	Node3D *_find_dive_target();

	// HSM States
	CelesteState *current_state = nullptr;
	CelesteGroundedState *grounded_state = nullptr;
	CelesteIdleState *idle_state = nullptr;
	CelesteMoveState *move_state = nullptr;
	CelesteJumpState *jump_state = nullptr;
	CelesteFallState *fall_state = nullptr;
	CelesteAirborneState *airborne_state = nullptr;
	CelesteDoubleJumpState *double_jump_state = nullptr;
	class CelesteDashState *dash_state = nullptr;
	CelesteAttackState *attack_state = nullptr;
	CelesteDiveKickState *dive_state = nullptr;
	AttackFx attack_fx; ///< Look of the attacks (spin whirl + swoosh, dive pose, impact ring).
	uint64_t _dive_target_id = 0; ///< Chosen by _find_dive_target, read by the dive state on enter.

protected:
	static void _bind_methods();

public:
	CelesteController();
	~CelesteController();

	void _ready() override;
	float get_speed_percent() const;
	Vector3 get_platform_velocity() const { return platform_velocity; }
	void _exit_tree() override;
	void _physics_process(double delta) override;

	void change_state(CelesteState *p_new_state);

	// Input as seen by the states: empty unless we are the active target, so an
	// inactive character never reacts to keys meant for another one.
	const ActionState &input_state() const;
	/// True for `parry_window` seconds after the hit (kick) button is pressed: a projectile
	/// reaching us then gets batted away (see Projectile::_try_parry).
	bool is_parrying() const { return _parry_timer > 0.0f; }
	void set_parry_window(float p_v) { parry_window = p_v; }
	float get_parry_window() const { return parry_window; }
	bool is_aiming() const { return bow.is_aiming() || lift.is_aiming(); }
	bool is_carrying() const { return lift.is_holding(); }
	/// On (hovering over) the ground. Follow cameras use it to hold their height mid-jump.
	bool is_grounded() const { return is_hovering; }
	/// Top run speed right now (slower while carrying something heavy).
	float move_speed() const { return max_speed * (lift.is_holding() ? lift.carry_speed_scale : 1.0f); }
	void set_throw_speed(float p_v) { lift.throw_speed = p_v; }
	float get_throw_speed() const { return lift.throw_speed; }
	void set_throw_damage(float p_v) { lift.impact_damage = p_v; }
	float get_throw_damage() const { return lift.impact_damage; }

	void set_bow_min_speed(float p_v) { bow.min_speed = p_v; }
	float get_bow_min_speed() const { return bow.min_speed; }
	void set_bow_max_speed(float p_v) { bow.max_speed = p_v; }
	float get_bow_max_speed() const { return bow.max_speed; }
	void set_bow_draw_time(float p_v) { bow.draw_time = p_v; }
	float get_bow_draw_time() const { return bow.draw_time; }
	void set_bow_loft(float p_v) { bow.loft = p_v; }
	float get_bow_loft() const { return bow.loft; }
	float movement_strength() const;
	bool uses_scheme() const { return control_scheme != 2; }
	Vector3 scheme_move_dir() const { return controls.get_move_dir(); }
	bool has_move_intent() const;

	void set_control_scheme(int p_scheme);
	int get_control_scheme() const { return control_scheme; }

private:
	TuningSection tuning; ///< this controller's tab in the shared TuningPanel
	void _setup_tuning();

	void debug_draw_trajectory(float p_delta);
	void debug_draw_label();
	void debug_draw_bottom();

	Vector3 _collide_and_slide(
			const Vector3 &p_velocity,
			const Vector3 &p_normal
	);
};

} // namespace godot

#endif // CELESTE_CONTROLLER_H
