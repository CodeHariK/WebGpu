#ifndef CELESTE_CONTROLLER_H
#define CELESTE_CONTROLLER_H

#include <godot_cpp/classes/character_body3d.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <map>

namespace godot {

class CelesteUI;
class CUI;
class CelesteState;
class CelesteIdleState;
class CelesteMoveState;
class CelesteJumpState;
class CelesteFallState;
class CelesteGroundedState;
class CelesteAirborneState;
class CelesteDoubleJumpState;
class CelesteJumpKickState;

class CelesteController : public CharacterBody3D {
	GDCLASS(CelesteController, CharacterBody3D)

	friend class CelesteState;
	friend class CelesteGroundedState;
	friend class CelesteIdleState;
	friend class CelesteMoveState;
	friend class CelesteAirborneState;
	friend class CelesteJumpState;
	friend class CelesteFallState;
	friend class CelesteDoubleJumpState;
	friend class CelesteDashState;
	friend class CelesteJumpKickState;

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
	float melee_range = 3.0f;
	float melee_lunge_speed = 50.0f;
	uint32_t melee_target_layer = 4; // Layer 3 (bit 2)

	float ride_height = 0.2f;
	float min_ride_height = 0.2f;
	float max_ride_height = 1.2f;
	float ride_height_speed = 5.0f;
	float spring_stiffness = 800.0f;
	float spring_damping = 40.0f;

	// Runtime State
	bool is_jumping = false;
	bool can_double_jump = true;
	float double_jump_multiplier = 0.8f;
	float coyote_timer = 0.0f;
	float jump_buffer_timer = 0.0f;
	bool is_hovering = false;
	float last_spring_error = 0.0f;

	// Dash state
	float dash_timer = 0.0f;
	float dash_cooldown_timer = 0.0f;
	bool can_dash = true;
	bool is_dashing = false;
	Vector3 dash_direction;

	void _update_jump_math();
	Node3D *_find_melee_target();

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
	CelesteJumpKickState *jumpkick_state = nullptr;

protected:
	static void _bind_methods();

public:
	CelesteController();
	~CelesteController();

	void _ready() override;
	float get_speed_percent() const;
	void _exit_tree() override;
	void _physics_process(double delta) override;

	void change_state(CelesteState *p_new_state);

	// UI Logic
	void _on_ui_slider_value_changed(double p_value, String p_property);
	void _on_ui_toggle();
	void save_settings();
	void load_settings();
	float get_ui_var(const String &p_name) const {
		if (ui_vars.count(p_name))
			return *ui_vars.at(p_name);
		return 0.0f;
	}

private:
	CelesteUI *ui_helper = nullptr;
	CUI *ui_root = nullptr;
	std::map<String, float *> ui_vars;
	std::map<String, bool *> ui_bools;

	void debug_draw_trajectory(float p_delta);
	void debug_draw_label();
	void debug_draw_bottom();
};

} // namespace godot

#endif // CELESTE_CONTROLLER_H
