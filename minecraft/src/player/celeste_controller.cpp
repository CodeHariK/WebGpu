#include "celeste_controller.h"
#include "../camera/camera.h"
#include "../game_manager/game_manager.h"
#include "../game_manager/player_input.h"
#include "../utils/raycast/mc_raycast.h"
#include "states/grounded_states.h"
#include "states/airborne_states.h"
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

void CelesteController::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_max_speed", "p_val"), &CelesteController::set_max_speed);
	ClassDB::bind_method(D_METHOD("get_max_speed"), &CelesteController::get_max_speed);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "max_speed"), "set_max_speed", "get_max_speed");

	ClassDB::bind_method(D_METHOD("set_acceleration", "p_val"), &CelesteController::set_acceleration);
	ClassDB::bind_method(D_METHOD("get_acceleration"), &CelesteController::get_acceleration);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "acceleration"), "set_acceleration", "get_acceleration");

	ClassDB::bind_method(D_METHOD("set_friction", "p_val"), &CelesteController::set_friction);
	ClassDB::bind_method(D_METHOD("get_friction"), &CelesteController::get_friction);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "friction"), "set_friction", "get_friction");

	ClassDB::bind_method(D_METHOD("set_air_resistance", "p_val"), &CelesteController::set_air_resistance);
	ClassDB::bind_method(D_METHOD("get_air_resistance"), &CelesteController::get_air_resistance);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "air_resistance"), "set_air_resistance", "get_air_resistance");

	ClassDB::bind_method(D_METHOD("set_jump_height", "p_val"), &CelesteController::set_jump_height);
	ClassDB::bind_method(D_METHOD("get_jump_height"), &CelesteController::get_jump_height);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "jump_height"), "set_jump_height", "get_jump_height");

	ClassDB::bind_method(D_METHOD("set_jump_time_to_peak", "p_val"), &CelesteController::set_jump_time_to_peak);
	ClassDB::bind_method(D_METHOD("get_jump_time_to_peak"), &CelesteController::get_jump_time_to_peak);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "jump_time_to_peak"), "set_jump_time_to_peak", "get_jump_time_to_peak");

	ClassDB::bind_method(D_METHOD("set_jump_time_to_descent", "p_val"), &CelesteController::set_jump_time_to_descent);
	ClassDB::bind_method(D_METHOD("get_jump_time_to_descent"), &CelesteController::get_jump_time_to_descent);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "jump_time_to_descent"), "set_jump_time_to_descent", "get_jump_time_to_descent");
}

CelesteController::CelesteController() {
	max_speed = 10.0f;
	acceleration = 80.0f;
	friction = 60.0f;

	jump_height = 4.5f; // Higher jump
	jump_time_to_peak = 0.6f; // Slower ascent
	jump_time_to_descent = 0.55f; // Slower descent

	_update_jump_math();
}

CelesteController::~CelesteController() {
	delete idle_state;
	delete move_state;
	delete jump_state;
	delete fall_state;
	delete grounded_state;
	delete airborne_state;
}

void CelesteController::_update_jump_math() {
	jump_velocity = (2.0f * jump_height) / jump_time_to_peak;
	jump_gravity = (2.0f * jump_height) / (jump_time_to_peak * jump_time_to_peak);
	fall_gravity = (2.0f * jump_height) / (jump_time_to_descent * jump_time_to_descent);
}

void CelesteController::_ready() {
	if (Engine::get_singleton()->is_editor_hint()) {
		return;
	}

	_update_jump_math();

	// Initialize HSM States
	grounded_state = new CelesteGroundedState(this, nullptr);
	airborne_state = new CelesteAirborneState(this, nullptr);

	idle_state = new CelesteIdleState(this, grounded_state);
	move_state = new CelesteMoveState(this, grounded_state);
	jump_state = new CelesteJumpState(this, airborne_state);
	fall_state = new CelesteFallState(this, airborne_state);

	current_state = fall_state;
	current_state->enter();

	GameManager *gm = GameManager::get_singleton();
	if (gm) {
		gm->register_celeste_controller(this);
	}
}

void CelesteController::change_state(CelesteState *p_new_state) {
	if (current_state == p_new_state)
		return;
	if (current_state)
		current_state->exit();
	current_state = p_new_state;
	if (current_state)
		current_state->enter();
}

void CelesteController::_physics_process(double delta) {
	if (Engine::get_singleton()->is_editor_hint()) {
		return;
	}

	float f_delta = (float)delta;
	PlayerInput *input = PlayerInput::get_singleton();
	if (!input)
		return;

	const ActionState &state = input->get_state();

	if (is_on_floor()) {
		is_jumping = false;
	}

	// 1. Execute State Logic
	if (current_state) {
		current_state->physics_update(f_delta);
	}

	// 2. Apply Movement
	move_and_slide();
}

} // namespace godot
