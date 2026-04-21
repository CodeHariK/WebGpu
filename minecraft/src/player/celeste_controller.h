#ifndef CELESTE_CONTROLLER_H
#define CELESTE_CONTROLLER_H

#include <godot_cpp/classes/character_body3d.hpp>
#include <godot_cpp/variant/vector3.hpp>

namespace godot {

class CelesteState;
class CelesteGroundedState;
class CelesteIdleState;
class CelesteMoveState;
class CelesteAirborneState;
class CelesteJumpState;
class CelesteFallState;

class CelesteController : public CharacterBody3D {
	GDCLASS(CelesteController, CharacterBody3D)

	friend class CelesteState;
	friend class CelesteGroundedState;
	friend class CelesteIdleState;
	friend class CelesteMoveState;
	friend class CelesteAirborneState;
	friend class CelesteJumpState;
	friend class CelesteFallState;

private:
	// Movement Settings (Celeste-style)
	float max_speed = 10.0f;
	float acceleration = 80.0f;
	float friction = 60.0f;
	float air_resistance = 20.0f;

	// Jump Math (Kinematic)
	float jump_height = 2.5f;
	float jump_time_to_peak = 0.4f;
	float jump_time_to_descent = 0.35f;

	// Calculated Values
	float jump_velocity = 0.0f;
	float jump_gravity = 0.0f;
	float fall_gravity = 0.0f;

	// Runtime State
	bool is_jumping = false;
	void _update_jump_math();

	// HSM States
	CelesteState *current_state = nullptr;
	CelesteGroundedState *grounded_state = nullptr;
	CelesteIdleState *idle_state = nullptr;
	CelesteMoveState *move_state = nullptr;
	CelesteAirborneState *airborne_state = nullptr;
	CelesteJumpState *jump_state = nullptr;
	CelesteFallState *fall_state = nullptr;

protected:
	static void _bind_methods();

public:
	CelesteController();
	~CelesteController();

	void _ready() override;
	void _exit_tree() override;
	void _physics_process(double delta) override;

	void change_state(CelesteState *p_new_state);

	// Getters/Setters for properties
	void set_max_speed(float p_val) { max_speed = p_val; }
	float get_max_speed() const { return max_speed; }

	void set_acceleration(float p_val) { acceleration = p_val; }
	float get_acceleration() const { return acceleration; }

	void set_friction(float p_val) { friction = p_val; }
	float get_friction() const { return friction; }

	void set_air_resistance(float p_val) { air_resistance = p_val; }
	float get_air_resistance() const { return air_resistance; }

	void set_jump_height(float p_val) {
		jump_height = p_val;
		_update_jump_math();
	}
	float get_jump_height() const { return jump_height; }

	void set_jump_time_to_peak(float p_val) {
		jump_time_to_peak = p_val;
		_update_jump_math();
	}
	float get_jump_time_to_peak() const { return jump_time_to_peak; }

	void set_jump_time_to_descent(float p_val) {
		jump_time_to_descent = p_val;
		_update_jump_math();
	}
	float get_jump_time_to_descent() const { return jump_time_to_descent; }
};

} // namespace godot

#endif // CELESTE_CONTROLLER_H
