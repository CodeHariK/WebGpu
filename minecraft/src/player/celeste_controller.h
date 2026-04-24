#ifndef CELESTE_CONTROLLER_H
#define CELESTE_CONTROLLER_H

#include <godot_cpp/classes/character_body3d.hpp>
#include <godot_cpp/variant/vector3.hpp>
#include <map>

namespace godot {

class CelesteState;
class CUI;
class CelesteGroundedState;
class CelesteIdleState;
class CelesteMoveState;
class CelesteAirborneState;
class CelesteJumpState;
class CelesteFallState;
class CelesteUI;
class CelesteDoubleJumpState;
class CelesteFloatState;
class CelesteLedgeClimbState;
class CelesteLedgeJumpState;

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
	friend class CelesteFloatState;
	friend class CelesteLedgeClimbState;
	friend class CelesteLedgeJumpState;

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

	// Fall Math (Kinematic)
	float fall_velocity = 0.0f;
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
	CelesteDoubleJumpState *double_jump_state = nullptr;
	CelesteFloatState *float_state = nullptr;
	CelesteLedgeClimbState *ledge_climb_state = nullptr;
	CelesteLedgeJumpState *ledge_jump_state = nullptr;

protected:
	static void _bind_methods();

public:
	CelesteController();
	~CelesteController();

	void _ready() override;
	void _exit_tree() override;
	void _physics_process(double delta) override;

	void change_state(CelesteState *p_new_state);


	// UI Logic
	void _on_ui_slider_value_changed(double p_value, String p_property);
	void _on_ui_toggle();
	void save_settings();
	void load_settings();
	float get_ui_var(const String &p_name) const {
		if (ui_vars.count(p_name)) return *ui_vars.at(p_name);
		return 0.0f;
	}

private:
	CelesteUI *ui_helper = nullptr;
	CUI *ui_root = nullptr;
	std::map<String, float *> ui_vars;
};

} // namespace godot

#endif // CELESTE_CONTROLLER_H
