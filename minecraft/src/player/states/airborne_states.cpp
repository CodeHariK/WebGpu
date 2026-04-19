#include "airborne_states.h"
#include "../../game_manager/player_input.h"
#include "../celeste_controller.h"
#include "grounded_states.h"
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

// --- AIRBORNE STATE (Parent) ---

void CelesteAirborneState::physics_update(float delta) {
	if (!controller)
		return;

	PlayerInput *input = PlayerInput::get_singleton();
	if (!input)
		return;
	const ActionState &state = input->get_state();

	// Transition to Grounded if floor touched
	if (controller->is_on_floor()) {
		if (state.character.move_axis.length() > 0.1f) {
			controller->change_state(controller->move_state);
		} else {
			controller->change_state(controller->idle_state);
		}
		return;
	}

	// Apply Gravity
	Vector3 v = controller->get_velocity();
	float current_gravity = (v.y > 0.0f) ? controller->jump_gravity : controller->fall_gravity;

	v.y -= current_gravity * delta;

	// Horizontal Air Control
	Transform3D transform = controller->get_global_transform();
	Vector3 forward = -transform.basis.get_column(2).normalized();
	Vector3 right = transform.basis.get_column(0).normalized();
	Vector3 move_dir = (forward * -state.character.move_axis.y + right * state.character.move_axis.x);
	if (move_dir.length() > 1.0f)
		move_dir.normalize();

	float accel = controller->acceleration;
	Vector3 target_vel = move_dir * controller->max_speed;

	v.x = Math::move_toward(v.x, target_vel.x, accel * delta);
	v.z = Math::move_toward(v.z, target_vel.z, accel * delta);
	controller->set_velocity(v);
}

// --- JUMP STATE ---

void CelesteJumpState::enter() {
	controller->is_jumping = true;
}

void CelesteJumpState::physics_update(float delta) {
	CelesteAirborneState::physics_update(delta);
	if (controller->current_state != this)
		return;

	PlayerInput *input = PlayerInput::get_singleton();
	if (!input)
		return;

	Vector3 v = controller->get_velocity();
	if (v.y <= 0.0f) {
		controller->change_state(controller->fall_state);
	}
}

// --- FALL STATE ---

void CelesteFallState::enter() {
	// Optional: Animation trigger
}

void CelesteFallState::physics_update(float delta) {
	CelesteAirborneState::physics_update(delta);
}

} // namespace godot
