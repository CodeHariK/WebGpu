#include "celeste_controller.h"
#include "../camera/camera.h"
#include "../debug_draw/debug_manager.h"
#include "../game_manager/game_manager.h"
#include "../game_manager/player_input.h"
#include "../utils/raycast/mc_raycast.h"
#include "celeste_ui.h"
#include "cui/cui.h"
#include "states/airborne_states.h"
#include "states/dash_states.h"
#include "states/grounded_states.h"
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
	ClassDB::bind_method(D_METHOD("get_speed_percent"), &CelesteController::get_speed_percent);
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
	delete dash_state;
}

void CelesteController::_update_jump_math() {
	_jump_velocity0 = (2.0f * jump_height) / jump_time_to_peak;
	_jump_gravity = (2.0f * jump_height) / (jump_time_to_peak * jump_time_to_peak);
	_fall_gravity = (2.0f * jump_height) / (jump_time_to_descent * jump_time_to_descent);
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
	dash_state = new CelesteDashState(this, airborne_state);

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
	ui_vars["sprint_multiplier"] = &sprint_multiplier;
	ui_vars["air_resistance"] = &air_resistance;
	ui_vars["jump_height"] = &jump_height;
	ui_vars["jump_time_to_peak"] = &jump_time_to_peak;
	ui_vars["jump_time_to_descent"] = &jump_time_to_descent;
	ui_vars["max_fall_velocity"] = &max_fall_velocity;
	ui_vars["coyote_time"] = &coyote_time_max;
	ui_vars["jump_buffer"] = &jump_buffer_max;
	ui_vars["double_jump_mult"] = &double_jump_multiplier;
	ui_vars["dash_speed"] = &dash_speed;
	ui_vars["dash_duration"] = &dash_duration;

	ui_vars["ride_height"] = &ride_height;
	ui_vars["spring_stiffness"] = &spring_stiffness;
	ui_vars["spring_damping"] = &spring_damping;

	ui_vars["floor_snap"] = &floor_snap_length;

	// Setup UI
	ui_root = CUI::create_on_new_layer(this);
	ui_helper = new CelesteUI();
	ui_helper->setup(this, ui_root);
	load_settings();

	// Apply floor snapping defaults
	set_floor_snap_length(floor_snap_length);
	set_floor_constant_speed_enabled(floor_constant_speed);
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

	// Update Coyote Timer
	if (is_on_floor()) {
		coyote_timer = coyote_time_max;
	} else {
		coyote_timer -= f_delta;
	}

	// Update Jump Buffer Timer
	if (state.character.jump_just_pressed) {
		jump_buffer_timer = jump_buffer_max;
	} else {
		jump_buffer_timer -= f_delta;
	}

	if (is_hovering) {
		is_jumping = false;
		can_dash = true;
		can_double_jump = true;
	}

	// Update Timers
	dash_cooldown_timer -= f_delta;

	// 1. Execute State Logic
	if (current_state) {
		current_state->physics_update(f_delta);
	}

	// 2. Hover Spring Logic (PD Controller)
	is_hovering = false;
	last_spring_error = 0.0f;
	DebugManager *dm = DebugManager::get_singleton();

	if (!is_jumping) {
		Vector3 ray_origin = get_global_position() + Vector3(0, 0.2f, 0);
		// Increase ray length to catch ground earlier
		Vector3 ray_dir = Vector3(0, -(ride_height + 1.8f), 0);

		TypedArray<RID> exclude;
		exclude.append(get_rid());
		MCRaycastHit hit = raycast_3d(this, ray_origin, ray_origin + ray_dir, 1, exclude);

		if (hit.is_hit) {
			is_hovering = true;
			float dist = (ray_origin - hit.position).length() - 0.2f;
			float error = ride_height - dist;
			last_spring_error = error;

			Vector3 vel = get_velocity();

			// PD Controller: Force = Stiffness * error - Damping * velocity
			float spring_force = (error * spring_stiffness) - (vel.y * spring_damping);
			vel.y += spring_force * f_delta;

			set_velocity(vel);
			set_floor_snap_length(0.0f);

			if (dm) {
				dm->draw_line("hover_ray", ray_origin, hit.position, 0.05f, Color(1, 0, 1, 0.8f), 0.1f);
			}
		} else {
			set_floor_snap_length(floor_snap_length);
			if (dm)
				dm->clear_line("hover_ray");
		}
	} else {
		set_floor_snap_length(0.0f);
		if (dm)
			dm->clear_line("hover_ray");
	}

	// 3. Apply Movement
	move_and_slide();

	// Debug visualization of state and velocity
	if (current_state) {
		String id = "state_" + get_name();
		Vector3 v = get_velocity();
		float speed = v.length();
		float h_speed = Vector3(v.x, 0, v.z).length();

		String label_text = String(current_state->get_name()) +
				"\nVel: (" + String::num(v.x, 1) + ", " + String::num(v.y, 1) + ", " + String::num(v.z, 1) + ")" +
				"\nSpeed: " + String::num(speed, 1) + " (H: " + String::num(h_speed, 1) + ")" +
				"\nHover: " + (is_hovering ? "YES" : "NO") + " Err: " + String::num(last_spring_error, 2);

		DebugManager::get_singleton()->draw_text(id, label_text, get_global_position() + Vector3(0, 2.5f, 0), 0.001f, Color(0, 1, 0));
	}

	if (ui_helper) {
		ui_helper->update_graph(get_velocity().length());
	}

	{
		// 3. Trajectory Tracking
		trajectory_timer += f_delta;
		if (trajectory_timer >= trajectory_interval) {
			trajectory_timer = 0.0f;
			trajectory_points.push_back(get_global_position());
			if (trajectory_points.size() > max_trajectory_points) {
				trajectory_points.erase(trajectory_points.begin());
			}
		}

		// Draw Trajectory
		DebugManager *dm = DebugManager::get_singleton();
		if (dm && trajectory_points.size() > 1) {
			for (size_t i = 0; i < trajectory_points.size() - 1; ++i) {
				String traj_id = "traj_" + String::num_int64(i);
				dm->draw_line(traj_id, trajectory_points[i], trajectory_points[i + 1], 0.2f, Color(0.2f, 0.8f, 1.0f, 0.6f), 0.1f);
			}
			// Connection to current position
			dm->draw_line("traj_head", trajectory_points.back(), get_global_position(), 0.02f, Color(1, 1, 0, 0.8f), 0.1f);
		}
	}
}

void CelesteController::_on_ui_slider_value_changed(double p_value, String p_property) {
	if (ui_vars.count(p_property)) {
		*ui_vars[p_property] = (float)p_value;

		// Re-calculate math if jump parameters changed
		if (p_property == "jump_height" || p_property == "jump_time_to_peak" || p_property == "jump_time_to_descent") {
			_update_jump_math();
		}

		if (p_property == "floor_snap") {
			set_floor_snap_length((float)p_value);
		}
	}
}

float CelesteController::get_speed_percent() const {
	if (max_speed <= 0.001f)
		return 0.0f;
	Vector3 h_vel = get_velocity();
	h_vel.y = 0.0f;
	return h_vel.length() / max_speed;
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
	config->set_value("Celeste", "sprint_multiplier", sprint_multiplier);
	config->set_value("Celeste", "air_resistance", air_resistance);
	config->set_value("Celeste", "jump_height", jump_height);
	config->set_value("Celeste", "jump_time_to_peak", jump_time_to_peak);
	config->set_value("celeste_physics", "jump_time_to_descent", jump_time_to_descent);
	config->set_value("celeste_physics", "max_fall_velocity", max_fall_velocity);
	config->set_value("celeste_physics", "floor_snap_length", floor_snap_length);
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
	sprint_multiplier = config->get_value("Celeste", "sprint_multiplier", 1.5f);
	air_resistance = config->get_value("Celeste", "air_resistance", 20.0f);
	jump_height = config->get_value("Celeste", "jump_height", 2.5f);
	jump_time_to_peak = config->get_value("Celeste", "jump_time_to_peak", 0.4f);
	jump_time_to_descent = config->get_value("celeste_physics", "jump_time_to_descent", 0.2f);
	max_fall_velocity = config->get_value("celeste_physics", "max_fall_velocity", 20.0f);
	floor_snap_length = config->get_value("celeste_physics", "floor_snap_length", 0.5f);

	_update_jump_math();
	set_floor_snap_length(floor_snap_length);
	set_floor_constant_speed_enabled(floor_constant_speed);

	if (ui_root) {
		for (auto const &[name, ptr] : ui_vars) {
			ui_root->set_value(name, *ptr);
		}
	}

	UtilityFunctions::print("CelesteController: Settings loaded from user://celeste_settings.cfg");
}

} // namespace godot
