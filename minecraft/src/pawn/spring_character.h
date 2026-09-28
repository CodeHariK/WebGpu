#ifndef SPRING_CHARACTER_H
#define SPRING_CHARACTER_H

#include <godot_cpp/classes/collision_shape3d.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/rigid_body3d.hpp>
#include <godot_cpp/variant/vector3.hpp>

namespace godot {

class PlayerInput;

/**
 * SpringCharacter — a physics "floating capsule" character controller, a faithful
 * port of the Toyful Games *Very Very Valet* toy controller.
 *
 * It is a RigidBody3D suspended over the ground by a downward ray + PD spring
 * (never touching the floor), kept vertical by a PD torque spring (so it can be
 * knocked and stands back up), and driven by a goal-velocity model whose steering
 * force is clamped — snappy but weighty. Being a real rigid body, it presses on
 * whatever it stands on (the ride spring pushes the ground body back) and is
 * shoved by the world — the thing a kinematic controller cannot do.
 *
 * Jump is a Mario-style arc on top: coyote time, jump buffer, variable height
 * (early release cuts it short), light rise gravity and heavier fall gravity.
 *
 * Self-registers as a GameManager target (TAB-switchable, camera auto-TPS).
 */
class SpringCharacter : public RigidBody3D {
	GDCLASS(SpringCharacter,
			RigidBody3D)

private:
	// --- Body (built on demand if the scene provides no CollisionShape3D) ---
	float capsule_radius = 0.4f;
	float capsule_height = 1.3f; // half = 0.65

	// --- Ride spring (float): PD spring holding the body at ride_height ---
	// ride_height also sets the max walkable step: anything shorter than
	// (ride_height - capsule_half) passes under the collider and the spring lifts.
	float ride_height = 1.2f;
	float ride_spring_strength = 1200.0f; // Hooke k (force units, mass baked in)
	float ride_spring_damper = 60.0f; // critical-ish damping of vertical motion
	float ride_ray_extra = 1.0f; // ray reaches ride_height + this below the origin

	// --- Upright torque spring: keeps the body vertical AND faces travel dir ---
	float upright_strength = 60.0f;
	float upright_damper = 12.0f;

	// --- Movement (VVV goal-velocity + clamped acceleration force) ---
	float max_speed = 18.0f;
	float acceleration = 200.0f; // how fast the goal velocity ramps
	float max_accel_force = 150.0f; // cap on the steering force (per unit mass)
	float accel_turn_boost = 2.5f; // extra accel when reversing direction

	// --- Jump / gravity (Mario arc) ---
	float jump_height = 3.0f;
	float coyote_time = 0.12f;
	float jump_buffer = 0.12f;
	float fall_gravity_extra = 22.0f; // extra downward accel while falling
	float low_jump_gravity_extra = 30.0f; // extra downward accel when rising, jump released
	float jump_lock_time = 0.18f; // disable the ride spring briefly after a jump

	// --- Runtime ---
	PlayerInput *player_input = nullptr;
	CollisionShape3D *_collider = nullptr;
	MeshInstance3D *_mesh = nullptr;

	Vector3 _goal_vel = Vector3(0, 0, 0); // smoothed target velocity
	float _jump_velocity = 7.0f;
	float _coyote_timer = 0.0f;
	float _jump_buffer_timer = 0.0f;
	float _jump_lock = 0.0f;

	// Ground probe results, refreshed each physics frame.
	bool _has_support = false;
	bool _grounded = false;
	float _ground_distance = 0.0f;
	Vector3 _ground_normal = Vector3(0, 1, 0);
	Vector3 _ground_point = Vector3(0, 0, 0);
	Object *_ground_body = nullptr;

	// Pipeline (one responsibility each), called from _physics_process.
	void _ensure_body();
	void _recompute_jump();
	void _cast_ground();
	Vector3 _camera_wish(const Vector2 &p_move_axis) const;
	void _apply_float_spring();
	void _apply_upright_and_facing(const Vector3 &p_wish);
	void _apply_movement(const Vector3 &p_wish, float p_dt);
	void _apply_jump_and_gravity(bool p_jump_now, bool p_jump_held, float p_dt);

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

	void set_max_speed(float v) { max_speed = v; }
	float get_max_speed() const { return max_speed; }
	void set_jump_height(float v) {
		jump_height = v;
		_recompute_jump();
	}
	float get_jump_height() const { return jump_height; }
	void set_ride_height(float v) { ride_height = v; }
	float get_ride_height() const { return ride_height; }
	void set_ride_spring_strength(float v) { ride_spring_strength = v; }
	float get_ride_spring_strength() const { return ride_spring_strength; }
	void set_ride_spring_damper(float v) { ride_spring_damper = v; }
	float get_ride_spring_damper() const { return ride_spring_damper; }
	void set_upright_strength(float v) { upright_strength = v; }
	float get_upright_strength() const { return upright_strength; }
	void set_upright_damper(float v) { upright_damper = v; }
	float get_upright_damper() const { return upright_damper; }
};

} // namespace godot

#endif // SPRING_CHARACTER_H
