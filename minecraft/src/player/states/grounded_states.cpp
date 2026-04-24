#include "grounded_states.h"
#include "../../camera/camera.h"
#include "../../game_manager/game_manager.h"
#include "../../game_manager/player_input.h"
#include "../celeste_controller.h"
#include "airborne_states.h"
#include "dash_states.h"
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

// --- GROUNDED STATE (Parent) ---

void CelesteGroundedState::physics_update(float delta) {
	if (!controller)
		return;

	PlayerInput *input = PlayerInput::get_singleton();
	if (!input)
		return;
	const ActionState &state = input->get_state();

	// Transition to Airborne if not on floor
	if (!controller->is_on_floor()) {
		controller->change_state(controller->fall_state);
		return;
	}

	// Jump Input (Direct or Buffered)
	if (state.character.jump_just_pressed || controller->jump_buffer_timer > 0.0f) {
		Vector3 vel = controller->get_velocity();
		vel.y = controller->_jump_velocity0;
		controller->set_velocity(vel);
		controller->is_jumping = true;

		// Consume buffer
		controller->jump_buffer_timer = 0.0f;
		controller->change_state(controller->jump_state);
		return;
	}

	// Dash Input
	if (state.character.dash_just_pressed && controller->can_dash && controller->dash_cooldown_timer <= 0.0f) {
		controller->change_state(controller->dash_state);
		return;
	}
}

// --- IDLE STATE ---

void CelesteIdleState::enter() {
	// Optional: Animation trigger
}

void CelesteIdleState::physics_update(float delta) {
	CelesteGroundedState::physics_update(delta);
	if (controller->current_state != this)
		return;

	// Apply Friction
	Vector3 vel = controller->get_velocity();
	vel.x = Math::move_toward(vel.x, 0.0f, controller->friction * delta);
	vel.z = Math::move_toward(vel.z, 0.0f, controller->friction * delta);
	controller->set_velocity(vel);

	// TPS Rotation
	GameCamera *cam = GameManager::get_singleton() ? GameManager::get_singleton()->get_camera() : nullptr;
	if (cam && cam->get_camera_mode() == GameCamera::MODE_TPS) {
		controller->set_rotation(Vector3(0, cam->get_yaw(), 0));
	}

	// Transition to Move if input detected
	PlayerInput *input = PlayerInput::get_singleton();
	if (input && input->get_state().character.move_axis.length() > 0.1f) {
		controller->change_state(controller->move_state);
	}
}

// --- MOVE STATE ---

void CelesteMoveState::enter() {
	// Optional: Animation trigger
}

void CelesteMoveState::physics_update(float delta) {
	CelesteGroundedState::physics_update(delta);
	if (controller->current_state != this)
		return;

	PlayerInput *input = PlayerInput::get_singleton();
	if (!input)
		return;
	const ActionState &state = input->get_state();

	// Calculate target direction relative to camera
	Transform3D transform = controller->get_global_transform();
	GameCamera *cam = GameManager::get_singleton() ? GameManager::get_singleton()->get_camera() : nullptr;

	bool rotate_to_move = false;
	if (cam) {
		if (cam->get_camera_mode() == GameCamera::MODE_TPS) {
			controller->set_rotation(Vector3(0, cam->get_yaw(), 0));
		} else if (cam->get_camera_mode() == GameCamera::MODE_FIXED) {
			rotate_to_move = true;
		}
		transform = controller->get_global_transform();
	}

	Vector3 forward = -transform.basis.get_column(2).normalized();
	Vector3 right = transform.basis.get_column(0).normalized();

	// If in FIXED mode, we use world axes (camera is fixed)
	if (rotate_to_move) {
		forward = Vector3(0, 0, -1);
		right = Vector3(1, 0, 0);
	}

	Vector3 move_dir = (forward * -state.character.move_axis.y + right * state.character.move_axis.x);
	if (move_dir.length() > 1.0f)
		move_dir.normalize();

	if (move_dir.length() < 0.1f) {
		controller->change_state(controller->idle_state);
		return;
	}

	// Character Rotation for FIXED mode
	if (rotate_to_move) {
		float target_angle = Math::atan2(-move_dir.x, -move_dir.z);
		float current_angle = controller->get_rotation().y;
		float new_angle = Math::lerp_angle(current_angle, target_angle, delta * 20.0f);
		controller->set_rotation(Vector3(0, new_angle, 0));
	}

	float input_strength = input->get_movement_strength(controller->sprint_multiplier);

	Vector3 target_horizontal_vel = move_dir * (controller->max_speed * input_strength);
	Vector3 vel = controller->get_velocity();
	vel.x = Math::move_toward(vel.x, target_horizontal_vel.x, controller->acceleration * delta);
	vel.z = Math::move_toward(vel.z, target_horizontal_vel.z, controller->acceleration * delta);
	controller->set_velocity(vel);
}

} // namespace godot
