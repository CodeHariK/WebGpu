#ifndef CELESTE_CONTROLLER_H
#define CELESTE_CONTROLLER_H

#include <godot_cpp/classes/character_body3d.hpp>
#include <godot_cpp/variant/vector3.hpp>

namespace godot {

class PlayerInput;

class CelesteController : public CharacterBody3D {
	GDCLASS(CelesteController, CharacterBody3D)

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

	// Apex Control (Airy jumps)
	float apex_gravity_multiplier = 0.5f;
	float apex_speed_multiplier = 1.2f;
	float apex_threshold = 2.0f; // Velocity Y range to trigger apex

	// Feel/Assistance
	float coyote_time_duration = 0.15f;
	float jump_buffer_duration = 0.15f;
	float max_fall_speed = 15.0f;

	// Wall Interaction
	float wall_jump_force = 12.0f;
	float wall_slide_speed = 2.5f;
	float wall_jump_lock_duration = 0.2f; // Time player loses horizontal control after a kick

	// Hover Spring Settings
	float ride_height = 1.2f;
	float spring_stiffness = 250.0f;
	float spring_damping = 25.0f;
	bool is_hovering = false;

	// Calculated Values
	float jump_velocity = 0.0f;
	float jump_gravity = 0.0f;
	float fall_gravity = 0.0f;

	// Runtime State
	float coyote_timer = 0.0f;
	float jump_buffer_timer = 0.0f;
	float wall_jump_lock_timer = 0.0f;
	bool is_jumping = false;

	void _update_jump_math();

protected:
	static void _bind_methods();

public:
	CelesteController();
	~CelesteController();

	void _ready() override;
	void _physics_process(double delta) override;

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

	// Apex Setters
	void set_apex_gravity_multiplier(float p_val) { apex_gravity_multiplier = p_val; }
	float get_apex_gravity_multiplier() const { return apex_gravity_multiplier; }

	void set_apex_speed_multiplier(float p_val) { apex_speed_multiplier = p_val; }
	float get_apex_speed_multiplier() const { return apex_speed_multiplier; }

	void set_apex_threshold(float p_val) { apex_threshold = p_val; }
	float get_apex_threshold() const { return apex_threshold; }

	// Wall Setters
	void set_wall_jump_force(float p_val) { wall_jump_force = p_val; }
	float get_wall_jump_force() const { return wall_jump_force; }

	void set_wall_slide_speed(float p_val) { wall_slide_speed = p_val; }
	float get_wall_slide_speed() const { return wall_slide_speed; }

	void set_wall_jump_lock_duration(float p_val) { wall_jump_lock_duration = p_val; }
	float get_wall_jump_lock_duration() const { return wall_jump_lock_duration; }

	void set_coyote_time_duration(float p_val) { coyote_time_duration = p_val; }
	float get_coyote_time_duration() const { return coyote_time_duration; }

	void set_jump_buffer_duration(float p_val) { jump_buffer_duration = p_val; }
	float get_jump_buffer_duration() const { return jump_buffer_duration; }

	void set_max_fall_speed(float p_val) { max_fall_speed = p_val; }
	float get_max_fall_speed() const { return max_fall_speed; }

	// Hover Setters
	void set_ride_height(float p_val) { ride_height = p_val; }
	float get_ride_height() const { return ride_height; }

	void set_spring_stiffness(float p_val) { spring_stiffness = p_val; }
	float get_spring_stiffness() const { return spring_stiffness; }

	void set_spring_damping(float p_val) { spring_damping = p_val; }
	float get_spring_damping() const { return spring_damping; }
};

} // namespace godot

#endif // CELESTE_CONTROLLER_H
