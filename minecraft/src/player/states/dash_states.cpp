#include "dash_states.h"
#include "../../game_manager/player_input.h"
#include "../celeste_controller.h"
#include "airborne_states.h"
#include "grounded_states.h"
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

void CelesteDashState::enter() {
	controller->is_dashing = true;
	controller->can_dash = false;
	controller->dash_timer = controller->dash_duration;
	controller->dash_cooldown_timer = controller->dash_cooldown;

	PlayerInput *input = PlayerInput::get_singleton();
	Vector2 move_axis = input->get_state().character.move_axis;

	if (move_axis.length() < 0.1f) {
		// Dash forward if no input
		Transform3D transform = controller->get_global_transform();
		controller->dash_direction = -transform.basis.get_column(2).normalized();
	} else {
		// Dash in move direction (camera relative)
		Transform3D transform = controller->get_global_transform();
		Vector3 forward = -transform.basis.get_column(2).normalized();
		Vector3 right = transform.basis.get_column(0).normalized();
		
		controller->dash_direction = (forward * -move_axis.y + right * move_axis.x).normalized();
	}

	// Apply Dash Velocity
	controller->set_velocity(controller->dash_direction * controller->dash_speed);
}

void CelesteDashState::physics_update(float delta) {
	if (!controller) return;
	
	controller->dash_timer -= delta;

	// Constant Velocity Dash (ignores gravity)
	controller->set_velocity(controller->dash_direction * controller->dash_speed);

	// Jump Dash (Super Jump)
	PlayerInput *input = PlayerInput::get_singleton();
	if (input && input->get_state().character.jump_just_pressed) {
		Vector3 vel = controller->get_velocity();
		// Carry momentum into the jump
		vel.y = controller->_jump_velocity0;
		vel += controller->dash_direction * (controller->dash_speed * 0.5f);
		controller->set_velocity(vel);
		controller->is_jumping = true;
		controller->change_state(controller->jump_state);
		return;
	}

	if (controller->dash_timer <= 0.0f) {
		controller->is_dashing = false;
		if (controller->is_hovering) {
			controller->change_state(controller->idle_state);
		} else {
			controller->change_state(controller->fall_state);
		}
	}
}

} // namespace godot
