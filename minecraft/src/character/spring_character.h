#ifndef SPRING_CHARACTER_H
#define SPRING_CHARACTER_H

#include <godot_cpp/classes/collision_shape3d.hpp>
#include "character_animator.h"
#include "character_audio.h"
#include "../cui/tuning_section.h"
#include "jump_metrics.h"
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/rigid_body3d.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/vector3.hpp>


namespace godot {

class PlayerInput;

// Moveset states (src/character/ai/) — friends so each can read the tunables and the
// working velocity `_vel`, and call the _apply_* helpers on the owner.
class CharacterState;
class CharacterGroundedState;
class CharacterAirborneState;
class CharacterWalkState;
class CharacterFallState;
class CharacterDashState;
class CharacterGroundPoundState;

/**
 * SpringCharacter — a physics "floating capsule" character controller.
 *
 * It stays a RigidBody3D (so it still presses on the ground body and gets shoved
 * by the world), but it is driven by DETERMINISTIC VELOCITY CONTROL rather than
 * accumulated forces: engine gravity is off (`gravity_scale = 0`) and every
 * physics frame we compute the exact velocity — a Celeste-precise, Mario-snappy
 * arc — and assign it. No soft force lag, no mid-air float.
 *
 *   - Horizontal: velocity ramps toward `wish * max_speed` (move_toward).
 *   - Vertical: a kinematic jump (`v0 = 2h/t_peak`) with self-integrated
 *     ASYMMETRIC gravity (lighter rising, heavier falling), variable height,
 *     and a terminal speed.
 *   - Grounded: a tight vertical servo holds the body exactly at `ride_height`
 *     (planted, not hovering) and presses the ground body down (pushes the car).
 *
 * The moveset (walk / fall / wall-climb / dash / ground-pound, plus double jump)
 * lives in a small hierarchical state machine under src/character/ai/; this class owns
 * the states, casts the ground/wall probes, and decides transitions each frame.
 *
 * Self-registers as a GameManager target (TAB-switchable, camera auto-TPS).
 */
class SpringCharacter : public RigidBody3D {
	GDCLASS(SpringCharacter,
			RigidBody3D)

public:
	/// How the stick is interpreted. Press V in-game to flip between them live.
	enum ControlScheme {
		CONTROL_STEER, ///< L/R rotate the heading, U/D drive along it; camera locked behind (MODE_CHARACTER).
		CONTROL_CAMERA_RELATIVE ///< Stick = screen-space direction, facing follows movement; free cam (MODE_PLATFORMER).
	};

private:

	friend class CharacterState;
	friend class CharacterGroundedState;
	friend class CharacterAirborneState;
	friend class CharacterWalkState;
	friend class CharacterFallState;
	friend class CharacterDashState;
	friend class CharacterGroundPoundState;

private:
	// --- Body (built on demand if the scene provides no CollisionShape3D) ---
	float capsule_radius = 0.4f;
	float capsule_height = 1.3f; // half = 0.65

	// --- Ride servo (float): holds the body at ride_height, planted not floaty ---
	// ride_height is the hover height of the body centre; it also sets the max walkable
	// step: anything shorter than (ride_height - capsule_half) passes under the collider
	// and the servo lifts. It is now DYNAMIC between min/max (Celeste-style): the body
	// rides low to look grounded, then rises to clear a step a forward raycast finds ahead.
	float ride_height = 0.15f; // live FOOT clearance, driven between min_ride_height and max_ride_height
	float min_ride_height = 0.15f; // relaxed foot clearance (planted / grounded look)
	float max_ride_height = 0.6f; // raised foot clearance to float up and over a step ahead
	float ride_height_speed = 8.0f; // how fast ride_height eases between min and max (m/s)
	float ride_probe_ahead = 0.8f; // forward raycast length that looks for a step to clear
	float ride_follow = 18.0f; // how fast vertical velocity closes the gap (1/s)
	float ride_max_speed = 10.0f; // clamp on the servo correction speed
	float ride_ray_extra = 1.0f; // ray reaches ride_height + this below the origin
	float ground_press = 12.0f; // downward accel pressed onto the ground body (car push)

	float face_turn_rate = 14.0f; // how fast the heading eases toward travel (higher = snappier)

	// --- Movement (deterministic: velocity ramps toward the goal) ---
	float max_speed = 18.0f;
	float acceleration = 180.0f; // m/s^2 toward the target velocity (higher = tighter turns)
	float accel_turn_boost = 2.5f; // extra accel when reversing direction
	ControlScheme control_scheme = CONTROL_STEER; // active control scheme (see ControlScheme)
	float steer_rate = 3.2f; // heading turn speed (rad/s)
	float reverse_speed_mult = 0.5f; // back-up speed as a fraction of max_speed

	// --- Jump / gravity (kinematic Mario arc; engine gravity is off) ---
	float jump_height = 9.0f;
	float jump_time_to_peak = 0.38f; // rise time -> sets jump velocity & rise gravity
	float jump_time_to_descent = 0.30f; // fall time -> sets (heavier) fall gravity
	float low_jump_mult = 2.2f; // extra gravity while rising with jump released (short hop)
	float terminal_velocity = 45.0f; // clamp on fall speed
	float coyote_time = 0.12f;
	float jump_buffer = 0.12f;
	float jump_lock_time = 0.12f; // ignore the ground servo briefly after a jump

	// --- Precision layer (Celeste-style) ---
	float corner_correct_dist = 0.3f; // lateral search for a clear side when the head clips a ledge
	float corner_probe_up = 0.3f; // how far above the head to look for a clip
	float corner_push = 4.0f; // lateral nudge to slip past a clipped corner (keeps upward momentum)

	// --- Movement extras (ported from Celeste) ---
	float sprint_multiplier = 1.6f; // top-speed boost while sprinting (Shift / full stick)


	// --- Moveset: dash (ground or air) ---
	float dash_speed = 26.0f;
	float dash_time = 0.18f;
	float dash_cooldown = 0.6f;

	// --- Moveset: air jump ---
	int air_jumps = 1; // extra jumps before landing (1 = double jump)

	// --- Moveset: wall climb (Prototype / Hulk — run straight up a wall) ---
	float wall_probe = 0.8f; // horizontal ray length used to find a wall
	float wall_jump_up = 27.0f; // vertical part of a wall jump
	float wall_jump_out = 12.0f; // push away from the wall (so you leave it, never cling)
	// Parabolic bounding (Prototype-style up+back hops instead of a smooth glide)

	// --- Moveset: ground pound ---
	float pound_hang_time = 0.18f; // hover before the slam
	float pound_speed = 30.0f; // downward slam speed

	// --- Bubble Wand (signature move; see bubble/bubble.h) ---
	float bubble_cooldown = 0.35f; // seconds between blows
	int max_bubbles = 3; // blowing past this pops the oldest

	// --- Debug ---
	bool debug_trajectory = true; // draw a cyan motion trail like the car

	// --- Runtime ---
	PlayerInput *player_input = nullptr;
	CollisionShape3D *_collider = nullptr;
	MeshInstance3D *_mesh = nullptr; // placeholder capsule; hidden when a skin is present
	// Animated skin (optional scene child exposing idle/move/jump/fall). The controller
	// pins its yaw to the heading and drives it through CharacterAnimator; the skin owns
	// its own AnimationTree and every transition. See character_animator.h.
	Node3D *_skin = nullptr;
	CharacterAnimator _animator;
	CharacterAudio _audio; // footsteps / jump / land one-shots (see character_audio.h)
	float _prev_jump_lock = 0.0f; // edge-detect a jump launch for the audio
	float skin_yaw_offset = 3.14159265f; // Sophia's rig faces +Z; our forward is -Z
	float _prev_face_yaw = 0.0f; // for the lean (turn-rate) signal
	// Steer mode: raw steer input (mouse pixels / keys) accumulates into this TARGET and the
	// real heading eases toward it. Filters mouse-pixel quantisation so the skin never steps.
	float _steer_yaw_target = 0.0f;
	bool _steer_target_valid = false;
	// Steer mode, back input (S / S+A / S+D): run TOWARD the camera like the platformer
	// scheme instead of backpedalling. The camera yaw is frozen at this value (captured when
	// S goes down; the mouse can still turn it) so turning round can't drag the camera along.
	bool _steer_backing = false;
	// Camera stays frozen at _back_cam_yaw after a backward run, until the player drives
	// FORWARD again (W). A/D turning or standing still does not recenter it.
	bool _hold_cam = false;
	float _back_cam_yaw = 0.0f;

	Vector3 _vel = Vector3(0, 0, 0); // working velocity, integrated each physics frame
	float _jump_velocity = 0.0f; // 2h / t_peak
	float _jump_gravity = 0.0f; // 2h / t_peak^2  (while rising)
	float _fall_gravity = 0.0f; // 2h / t_descent^2 (while falling)
	float _coyote_timer = 0.0f;
	float _jump_buffer_timer = 0.0f;
	float _jump_lock = 0.0f;
	float _face_yaw = 0.0f; // persistent heading goal (held when idle, updated when moving)
	bool _face_init = false;
	bool _scheme_key_was_down = false; // edge-detect the V toggle
	float _dt = 0.0f; // last physics step (for frame-rate-independent easing)

	// Per-frame input snapshot, filled at the top of _physics_process.
	Vector3 _wish = Vector3(0, 0, 0); // intended move direction (for wall-cast / dash aim)
	Vector3 _move_target = Vector3(0, 0, 0); // desired horizontal velocity this frame (m/s)
	Vector3 _face_dir = Vector3(0, 0, 0); // heading target for walk/air (zero = hold)
	bool _in_jump_pressed = false;
	bool _in_jump_held = false;
	bool _in_dash_pressed = false;
	float _move_strength = 0.0f; // analog + sprint magnitude for this frame
	Vector3 _platform_velocity = Vector3(0, 0, 0); // carry velocity of the ground platform

	// Moveset runtime.
	int _air_jumps_left = 0;
	bool _air_dash_used = false; // one air dash per airtime (refilled on landing / wall jump)
	float _dash_timer = 0.0f;
	float _dash_cd_timer = 0.0f;
	Vector3 _dash_dir = Vector3(0, 0, 0);
	float _pound_hang_timer = 0.0f;
	float _bubble_cd = 0.0f;

	// Ground probe results, refreshed each physics frame.
	bool _has_support = false;
	bool _grounded = false;
	float _ground_distance = 0.0f;
	Vector3 _ground_normal = Vector3(0, 1, 0);
	Vector3 _ground_point = Vector3(0, 0, 0);
	Object *_ground_body = nullptr;

	// Wall probe results (multi-ray), refreshed each physics frame.
	bool _has_wall = false; // chest + feet rays both on a near-vertical surface
	Vector3 _wall_normal = Vector3(0, 0, 0);

	// --- Hierarchical state machine (src/character/ai/) ---
	CharacterState *current_state = nullptr;
	CharacterGroundedState *grounded_state = nullptr;
	CharacterAirborneState *airborne_state = nullptr;
	CharacterWalkState *walk_state = nullptr;
	CharacterFallState *fall_state = nullptr;
	CharacterDashState *dash_state = nullptr;
	CharacterGroundPoundState *pound_state = nullptr;

	// --- Live tuning: this character's tab in the shared TuningPanel ---
	TuningSection tuning;
	void _setup_tuning();

	void _init_states();
	void _free_states();
	void change_state(CharacterState *p_next);
	void _update_transitions();

	// Pipeline. Probes + timers run in _physics_process; the active state calls the
	// _apply_* helpers, which all mutate the working velocity `_vel` (no forces).
	void _ensure_body();
	void _recompute_jump();
	void _cast_ground();
	void _cast_wall(const Vector3 &p_wish);
	void _blow_bubble(); // Bubble Wand: spawn a bubble ahead of the character
	void _find_skin(); // locate a child skin by its intent API and hide the placeholder
	void _apply_landing_guard(float p_delta); // never let one step carry the sole past ride height
	void _update_ride_height(float p_delta); // Celeste-style: raise ride height for a step ahead
	void _debug_draw_trajectory(float p_delta); // cyan breadcrumb trail of where we've been
	// Collide-and-slide: project a velocity onto the wall plane (removing motion into the
	// face) and re-add a bounded stick, so the body slides along a wall instead of snagging.
	void _tick_timers(float p_dt);
	void _update_jump_timers(float p_dt);
	void _apply_ground_servo();
	void _apply_upright_and_facing(const Vector3 &p_wish);
	void _apply_movement(float p_dt);
	void _apply_air_gravity(bool p_jump_held, float p_dt);
	void _apply_corner_correction();

	static float _accel_factor(float p_vel_dot, float p_boost);

protected:
	static void _bind_methods();

public:
	SpringCharacter();

	void _ready() override;
	void _exit_tree() override;
	void _physics_process(double delta) override;

	void set_player_input(PlayerInput *p_input) { player_input = p_input; }
	bool is_grounded() const { return _grounded; }
	/// Current jump arc as closed-form metrics (reach / airtime) for level design tools.
	JumpMetrics get_jump_metrics() const {
		JumpMetrics m;
		m.height = jump_height;
		m.time_to_peak = jump_time_to_peak;
		m.time_to_descent = jump_time_to_descent;
		m.run_speed = max_speed;
		m.sprint_speed = max_speed * sprint_multiplier;
		m.terminal_velocity = terminal_velocity;
		m.air_jumps = air_jumps;
		m.air_jump_mult = 1.0f; // air jumps relaunch at full jump speed
		m.air_dash = true; // one flat, gravity-free dash per airtime
		m.dash_speed = dash_speed;
		m.dash_time = dash_time;
		m.air_accel = acceleration;
		return m;
	}

	void set_max_speed(float v) { max_speed = v; }
	float get_max_speed() const { return max_speed; }
	void set_acceleration(float v) { acceleration = v; }
	float get_acceleration() const { return acceleration; }
	void set_control_scheme(int v) { control_scheme = (v == 1) ? CONTROL_CAMERA_RELATIVE : CONTROL_STEER; }
	int get_control_scheme() const { return (int)control_scheme; }
	void set_steer_rate(float v) { steer_rate = v; }
	float get_steer_rate() const { return steer_rate; }
	void set_jump_height(float v) {
		jump_height = v;
		_recompute_jump();
	}
	float get_jump_height() const { return jump_height; }
	void set_jump_time_to_peak(float v) {
		jump_time_to_peak = v;
		_recompute_jump();
	}
	float get_jump_time_to_peak() const { return jump_time_to_peak; }
	void set_jump_time_to_descent(float v) {
		jump_time_to_descent = v;
		_recompute_jump();
	}
	float get_jump_time_to_descent() const { return jump_time_to_descent; }
	void set_ride_height(float v) { ride_height = v; }
	float get_ride_height() const { return ride_height; }
	void set_ride_follow(float v) { ride_follow = v; }
	float get_ride_follow() const { return ride_follow; }

	// Moveset tunables (exposed for the feel-tuning pass).
	void set_dash_speed(float v) { dash_speed = v; }
	float get_dash_speed() const { return dash_speed; }
	void set_dash_time(float v) { dash_time = v; }
	float get_dash_time() const { return dash_time; }
	void set_dash_cooldown(float v) { dash_cooldown = v; }
	float get_dash_cooldown() const { return dash_cooldown; }
	void set_air_jumps(int v) { air_jumps = v; }
	int get_air_jumps() const { return air_jumps; }
	void set_wall_jump_up(float v) { wall_jump_up = v; }
	float get_wall_jump_up() const { return wall_jump_up; }
	void set_wall_jump_out(float v) { wall_jump_out = v; }
	float get_wall_jump_out() const { return wall_jump_out; }
	void set_pound_speed(float v) { pound_speed = v; }
	float get_pound_speed() const { return pound_speed; }
	void set_sprint_multiplier(float v) { sprint_multiplier = v; }
	float get_sprint_multiplier() const { return sprint_multiplier; }
};

} // namespace godot

VARIANT_ENUM_CAST(godot::SpringCharacter::ControlScheme);

#endif // SPRING_CHARACTER_H
