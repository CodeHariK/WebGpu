#include "player_input.h"
#include <godot_cpp/classes/input.hpp>
#include <godot_cpp/classes/input_event_key.hpp>
#include <godot_cpp/classes/input_event_mouse_button.hpp>
#include <godot_cpp/classes/input_event_mouse_motion.hpp>
#include <godot_cpp/classes/input_map.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

PlayerInput *PlayerInput::singleton = nullptr;

void PlayerInput::_bind_methods() {
}

PlayerInput::PlayerInput() {
	if (singleton == nullptr) {
		singleton = this;
	}
	is_shift_down = false;
	is_mb_middle_down = false;

	if (OS::get_singleton()->has_feature("mobile")) {
		_current_strength_handler = &PlayerInput::_get_strength_mobile;
	} else {
		_current_strength_handler = &PlayerInput::_get_strength_desktop;
	}

	// Setup default actions
	InputMap *im = InputMap::get_singleton();
	if (im) {
	}
}

PlayerInput::~PlayerInput() {
	if (singleton == this) {
		singleton = nullptr;
	}
}

PlayerInput *PlayerInput::get_singleton() {
	return singleton;
}

void PlayerInput::update() {
	Input *input = Input::get_singleton();
	if (!input)
		return;

	// 1. Swap accumulated deltas to current state
	current_state.camera.look_delta = accumulated_look;
	current_state.camera.zoom_delta = accumulated_zoom;

	// Sync event-tracked button states to the active state for this frame
	current_state.camera.is_orbiting = is_mb_middle_down;
	current_state.camera.is_panning = is_mb_middle_down && is_shift_down;

	// Clear accumulated for next frame
	accumulated_look = Vector2(0, 0);
	accumulated_zoom = 0.0f;

	// 2. Character Axis (Using Godot's get_vector for built-in normalization and deadzones)
	current_state.character.move_axis = input->get_vector("move_left", "move_right", "move_forward", "move_backward");

	// 3. Actions (State)
	current_state.character.jump = input->is_action_pressed("jump");
	current_state.character.kick = input->is_action_pressed("kick");
	current_state.character.grab = input->is_action_pressed("grab");
	current_state.character.dash = input->is_action_pressed("dash");
	current_state.system.swap_target = input->is_action_pressed("swap_target");

	// 4. Just Pressed Logic (Native Godot)
	current_state.character.jump_just_pressed = input->is_action_just_pressed("jump");
	current_state.character.kick_just_pressed = input->is_action_just_pressed("kick");
	current_state.character.grab_just_pressed = input->is_action_just_pressed("grab");
	current_state.character.dash_just_pressed = input->is_action_just_pressed("dash");
	current_state.system.swap_target_just_pressed = input->is_action_just_pressed("swap_target");

	// 5. Vehicle Specifics
	// Throttle/Brake derived from forward/backward actions
	current_state.vehicle.throttle = input->get_action_strength("move_forward");
	current_state.vehicle.brake = input->get_action_strength("move_backward");
	current_state.vehicle.steering = input->get_action_strength("move_right") - input->get_action_strength("move_left");
	current_state.vehicle.handbrake = input->is_action_pressed("handbrake");
}

void PlayerInput::handle_input(const Ref<InputEvent> &p_event) {
	Ref<InputEventMouseMotion> mm = p_event;
	if (mm.is_valid()) {
		accumulated_look += mm->get_relative();
	}

	Ref<InputEventMouseButton> mb = p_event;
	if (mb.is_valid()) {
		if (mb->get_button_index() == MOUSE_BUTTON_WHEEL_UP) {
			accumulated_zoom += 1.0f;
		} else if (mb->get_button_index() == MOUSE_BUTTON_WHEEL_DOWN) {
			accumulated_zoom -= 1.0f;
		} else if (mb->get_button_index() == MOUSE_BUTTON_MIDDLE) {
			is_mb_middle_down = mb->is_pressed();
		}
	}

	Ref<InputEventKey> k = p_event;
	if (k.is_valid()) {
		if (k->get_keycode() == KEY_SHIFT) {
			is_shift_down = k->is_pressed();
		}
	}
}

float PlayerInput::get_movement_strength(float p_sprint_multiplier) const {
	if (_current_strength_handler) {
		return (this->*_current_strength_handler)(p_sprint_multiplier);
	}
	return current_state.character.move_axis.length();
}

float PlayerInput::_get_strength_desktop(float p_sprint_multiplier) const {
	float raw_strength = current_state.character.move_axis.length();
	if (is_shift_down) {
		return raw_strength * p_sprint_multiplier;
	}
	return raw_strength;
}

float PlayerInput::_get_strength_mobile(float p_sprint_multiplier) const {
	float raw_strength = current_state.character.move_axis.length();
	// Mobile Logic: Auto-sprint past 0.9
	if (raw_strength > 0.9f) {
		float overdrive = (raw_strength - 0.9f) / 0.1f;
		return 1.0f + (overdrive * (p_sprint_multiplier - 1.0f));
	}
	return raw_strength;
}

} // namespace godot
