#ifndef PHYSICS_CHARACTER_3D_H
#define PHYSICS_CHARACTER_3D_H

#include <godot_cpp/classes/rigid_body3d.hpp>
#include <godot_cpp/classes/sphere_shape3d.hpp>
#include <godot_cpp/classes/collision_shape3d.hpp>
#include <godot_cpp/variant/vector3.hpp>

namespace godot {

class CharacterState;
class CharGroundedState;
class CharAirborneState;
class CharIdleState;
class CharMoveState;
class CharJumpState;
class CharFallState;
class CharDropkickState;

/**
 * A responsive physics-based character controller using a floating capsule approach.
 */
class PhysicsCharacter3D : public RigidBody3D {
	GDCLASS(PhysicsCharacter3D, RigidBody3D)

private:
	// Configuration (Exposed to Godot)
	float ride_height = 1.0f;         // Distance from center to ground
	float spring_stiffness = 600.0f; // PD controller stiffness
	float spring_damping = 40.0f;   // PD controller damping
	float acceleration = 120.0f;
	float max_speed = 8.0f;
	float jump_impulse = 12.0f;

	// Combat
	float kick_speed = 35.0f;
	float kick_max_distance = 20.0f;
	float kick_impact_force = 1500.0f;

	// Runtime state
	bool is_grounded = false;
	Vector3 ground_normal = Vector3(0, 1, 0);
	float dist_to_ground = 0.0f;
	Vector3 input_dir;

	// HSM Nodes
	CharacterState *current_state = nullptr;
	CharGroundedState *grounded_state = nullptr;
	CharAirborneState *airborne_state = nullptr;
	CharIdleState *idle_state = nullptr;
	CharMoveState *move_state = nullptr;
	CharJumpState *jump_state = nullptr;
	CharFallState *fall_state = nullptr;
	CharDropkickState *dropkick_state = nullptr;

	friend class CharacterState;
	friend class CharGroundedState;
	friend class CharAirborneState;
	friend class CharIdleState;
	friend class CharMoveState;
	friend class CharJumpState;
	friend class CharFallState;
	friend class CharDropkickState;

	void _setup_character();
	void _handle_ground_detection(float delta);
	void _apply_hover_spring(float delta);

protected:
	static void _bind_methods();

public:
	PhysicsCharacter3D();
	~PhysicsCharacter3D();

	void _ready() override;
	void _physics_process(double delta) override;

	void change_state(CharacterState *p_new_state);

	// Getters/Setters
	void set_ride_height(float p_val) { ride_height = p_val; }
	float get_ride_height() const { return ride_height; }
	
	void set_spring_stiffness(float p_val) { spring_stiffness = p_val; }
	float get_spring_stiffness() const { return spring_stiffness; }

	void set_spring_damping(float p_val) { spring_damping = p_val; }
	float get_spring_damping() const { return spring_damping; }

	void set_acceleration(float p_val) { acceleration = p_val; }
	float get_acceleration() const { return acceleration; }

	void set_max_speed(float p_val) { max_speed = p_val; }
	float get_max_speed() const { return max_speed; }

	void set_jump_impulse(float p_val) { jump_impulse = p_val; }
	float get_jump_impulse() const { return jump_impulse; }

	void set_kick_speed(float p_val) { kick_speed = p_val; }
	float get_kick_speed() const { return kick_speed; }

	void set_kick_max_distance(float p_val) { kick_max_distance = p_val; }
	float get_kick_max_distance() const { return kick_max_distance; }

	void set_kick_impact_force(float p_val) { kick_impact_force = p_val; }
	float get_kick_impact_force() const { return kick_impact_force; }
};

} // namespace godot

#endif // PHYSICS_CHARACTER_3D_H
