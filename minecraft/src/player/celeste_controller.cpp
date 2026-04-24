#include "celeste_controller.h"
#include "../camera/camera.h"
#include "../debug_draw/debug_manager.h"
#include "../game_manager/game_manager.h"
#include "../game_manager/player_input.h"
#include "../utils/raycast/mc_raycast.h"
#include "celeste_ui.h"
#include "cui/cui.h"
#include "states/airborne_states.h"
#include "states/grounded_states.h"
#include "states/ledge_states.h"
#include <godot_cpp/classes/config_file.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

void CelesteController::_bind_methods() {
	ClassDB::bind_method(D_METHOD("_on_ui_toggle"), &CelesteController::_on_ui_toggle);
	ClassDB::bind_method(D_METHOD("save_settings"), &CelesteController::save_settings);
	ClassDB::bind_method(D_METHOD("load_settings"), &CelesteController::load_settings);
	ClassDB::bind_method(D_METHOD("_on_ui_slider_value_changed", "value", "property"), &CelesteController::_on_ui_slider_value_changed);
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
	if (ui_helper) {
		delete ui_helper;
		ui_helper = nullptr;
	}
	delete idle_state;
	delete move_state;
	delete jump_state;
	delete fall_state;
	delete grounded_state;
	delete airborne_state;
	delete double_jump_state;
	delete float_state;
	delete ledge_climb_state;
	delete ledge_jump_state;
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
	double_jump_state = new CelesteDoubleJumpState(this, airborne_state);
	float_state = new CelesteFloatState(this, airborne_state);
	ledge_climb_state = new CelesteLedgeClimbState(this, nullptr);
	ledge_jump_state = new CelesteLedgeJumpState(this, nullptr);

	current_state = fall_state;
	current_state->enter();

	GameManager *gm = GameManager::get_singleton();
	if (gm) {
		gm->register_celeste_controller(this);
	}

	// Setup UI Vars Map
	ui_vars["max_speed"] = &max_speed;
	ui_vars["acceleration"] = &acceleration;
	ui_vars["friction"] = &friction;
	ui_vars["air_resistance"] = &air_resistance;
	ui_vars["jump_height"] = &jump_height;
	ui_vars["jump_time_to_peak"] = &jump_time_to_peak;
	ui_vars["jump_time_to_descent"] = &jump_time_to_descent;

	// Setup UI
	ui_root = CUI::create_on_new_layer(this);
	ui_helper = new CelesteUI();
	ui_helper->setup(this, ui_root);
	load_settings();
}

void CelesteController::_exit_tree() {
	GameManager *gm = GameManager::get_singleton();
	if (gm && gm->get_celeste_controller() == this) {
		gm->register_celeste_controller(nullptr);
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

	// Debug visualization of state (updated after move_and_slide for perfect tracking)
	if (current_state) {
		String id = "state_" + get_name();
		DebugManager::get_singleton()->draw_text(id, current_state->get_name(), get_global_position() + Vector3(0, 2.2f, 0), 0.001f, Color(0, 1, 0));
	}
}

void CelesteController::_on_ui_slider_value_changed(double p_value, String p_property) {
	if (ui_vars.count(p_property)) {
		*ui_vars[p_property] = (float)p_value;

		// Re-calculate math if jump parameters changed
		if (p_property == "jump_height" || p_property == "jump_time_to_peak" || p_property == "jump_time_to_descent") {
			_update_jump_math();
		}
	}
}

void CelesteController::_on_ui_toggle() {
	if (ui_helper) {
		ui_helper->toggle_visibility();
	}
}

void CelesteController::save_settings() {
	Ref<ConfigFile> config;
	config.instantiate();

	config->set_value("Celeste", "max_speed", max_speed);
	config->set_value("Celeste", "acceleration", acceleration);
	config->set_value("Celeste", "friction", friction);
	config->set_value("Celeste", "air_resistance", air_resistance);
	config->set_value("Celeste", "jump_height", jump_height);
	config->set_value("Celeste", "jump_time_to_peak", jump_time_to_peak);
	config->set_value("Celeste", "jump_time_to_descent", jump_time_to_descent);

	config->save("user://celeste_settings.cfg");
	UtilityFunctions::print("CelesteController: Settings saved to user://celeste_settings.cfg");
}

void CelesteController::load_settings() {
	Ref<ConfigFile> config;
	config.instantiate();

	Error err = config->load("user://celeste_settings.cfg");
	if (err != OK)
		return;

	max_speed = config->get_value("Celeste", "max_speed", 10.0f);
	acceleration = config->get_value("Celeste", "acceleration", 80.0f);
	friction = config->get_value("Celeste", "friction", 60.0f);
	air_resistance = config->get_value("Celeste", "air_resistance", 20.0f);
	jump_height = config->get_value("Celeste", "jump_height", 2.5f);
	jump_time_to_peak = config->get_value("Celeste", "jump_time_to_peak", 0.4f);
	jump_time_to_descent = config->get_value("Celeste", "jump_time_to_descent", 0.35f);

	_update_jump_math();

	UtilityFunctions::print("CelesteController: Settings loaded from user://celeste_settings.cfg");
}

} // namespace godot
