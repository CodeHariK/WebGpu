#include "player_input.h"
#include <godot_cpp/classes/input.hpp>

namespace godot {

PlayerInput *PlayerInput::singleton = nullptr;

void PlayerInput::_bind_methods() {
}

PlayerInput::PlayerInput() {
	if (singleton == nullptr) {
		singleton = this;
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
	if (!input) return;

	// 1. Movement Axis (Normalized)
	current_state.move_axis = Vector2(0, 0);
	if (input->is_physical_key_pressed(KEY_D)) current_state.move_axis.x += 1.0f;
	if (input->is_physical_key_pressed(KEY_A)) current_state.move_axis.x -= 1.0f;
	if (input->is_physical_key_pressed(KEY_S)) current_state.move_axis.y += 1.0f;
	if (input->is_physical_key_pressed(KEY_W)) current_state.move_axis.y -= 1.0f;
	
	if (current_state.move_axis.length() > 1.0f) {
		current_state.move_axis = current_state.move_axis.normalized();
	}

	// 2. Actions (State)
	current_state.jump = input->is_physical_key_pressed(KEY_SPACE);
	current_state.kick = input->is_mouse_button_pressed(MOUSE_BUTTON_LEFT);
	current_state.swap_target = input->is_physical_key_pressed(KEY_TAB);

	// 3. Just Pressed Logic
	current_state.jump_just_pressed = (current_state.jump && !prev_jump);
	current_state.kick_just_pressed = (current_state.kick && !prev_kick);
	current_state.swap_target_just_pressed = (current_state.swap_target && !prev_swap);
	
	// 4. Vehicle Specifics
	current_state.throttle = current_state.move_axis.y < 0 ? -current_state.move_axis.y : 0.0f;
	current_state.brake = current_state.move_axis.y > 0 ? current_state.move_axis.y : 0.0f;
	current_state.steering = current_state.move_axis.x;
	current_state.handbrake = input->is_physical_key_pressed(KEY_SHIFT);

	// Update previous states
	prev_jump = current_state.jump;
	prev_kick = current_state.kick;
	prev_swap = current_state.swap_target;
}

} // namespace godot
