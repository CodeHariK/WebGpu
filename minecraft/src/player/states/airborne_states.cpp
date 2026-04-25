#include "airborne_states.h"
#include "../../camera/camera.h"
#include "../../game_manager/game_manager.h"
#include "../../game_manager/player_input.h"
#include "../celeste_controller.h"
#include "dash_states.h"
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

	// Coyote Time Jump
	if (state.character.jump_just_pressed && controller->coyote_timer > 0.0f) {
		Vector3 vel = controller->get_velocity();
		vel.y = controller->_jump_velocity0;
		controller->set_velocity(vel);
		controller->is_jumping = true;

		// Consume coyote timer
		controller->coyote_timer = 0.0f;

		controller->change_state(controller->jump_state);
		return;
	}

	// Dash Transition
	if (state.character.dash_just_pressed && controller->can_dash && controller->dash_cooldown_timer <= 0.0f) {
		controller->change_state(controller->dash_state);
		return;
	}

	// Double Jump
	if (state.character.jump_just_pressed && controller->can_double_jump && !controller->has_double_jumped) {
		controller->change_state(controller->double_jump_state);
		return;
	}

	// Horizontal Air Control
	GameCamera *cam = GameManager::get_singleton() ? GameManager::get_singleton()->get_camera() : nullptr;

	bool rotate_to_move = false;
	if (cam) {
		if (cam->get_camera_mode() == GameCamera::MODE_TPS) {
			controller->set_rotation(Vector3(0, cam->get_yaw(), 0));
		} else if (cam->get_camera_mode() == GameCamera::MODE_FIXED) {
			rotate_to_move = true;
		}
	}

	Transform3D transform = controller->get_global_transform();

	Vector3 forward = -transform.basis.get_column(2).normalized();
	Vector3 right = transform.basis.get_column(0).normalized();

	if (rotate_to_move) {
		forward = Vector3(0, 0, -1);
		right = Vector3(1, 0, 0);
	}

	Vector3 move_dir = (forward * -state.character.move_axis.y + right * state.character.move_axis.x);
	if (move_dir.length() > 1.0f)
		move_dir.normalize();

	// Wall Jump
	if (state.character.jump_just_pressed && controller->is_on_wall()) {
		Vector3 wall_normal = controller->get_wall_normal();
		// Use max_speed for horizontal push and normal jump velocity for vertical
		Vector3 wall_jump_vel = (wall_normal * controller->max_speed) + (Vector3(0, 1, 0) * controller->_jump_velocity0);
		controller->set_velocity(wall_jump_vel);
		controller->is_jumping = true;
		controller->has_double_jumped = false; // Reset double jump on wall jump
		controller->change_state(controller->jump_state);
		return;
	}

	// Apply Gravity
	Vector3 v = controller->get_velocity();
	float current_gravity = (v.y > 0.0f) ? controller->_jump_gravity : controller->_fall_gravity;

	v.y -= current_gravity * delta;

	if (v.y < -controller->max_fall_velocity) {
		v.y = -controller->max_fall_velocity;
	}

	// Character Rotation for FIXED mode
	if (rotate_to_move && move_dir.length() > 0.1f) {
		float target_angle = Math::atan2(-move_dir.x, -move_dir.z);
		float current_angle = controller->get_rotation().y;
		float new_angle = Math::lerp_angle(current_angle, target_angle, delta * 20.0f);
		controller->set_rotation(Vector3(0, new_angle, 0));
	}

	float accel = controller->acceleration;
	float input_strength = input->get_movement_strength(controller->sprint_multiplier);

	Vector3 target_vel = move_dir * (controller->max_speed * input_strength);

	v.x = Math::move_toward(v.x, target_vel.x, accel * delta);
	v.z = Math::move_toward(v.z, target_vel.z, accel * delta);

	// Apply Horizontal Air Resistance (Drag) - Only if falling naturally, not from a jump
	if (controller->current_state == (CelesteState *)controller->fall_state && !controller->is_jumping) {
		float drag = 1.0f - (controller->air_resistance * delta);
		v.x *= Math::max(0.0f, drag);
		v.z *= Math::max(0.0f, drag);
	}

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

// --- DOUBLE JUMP STATE ---

void CelesteDoubleJumpState::enter() {
	Vector3 vel = controller->get_velocity();
	vel.y = controller->_jump_velocity0 * controller->double_jump_multiplier;
	controller->set_velocity(vel);
	controller->is_jumping = true;
	controller->has_double_jumped = true;
}

void CelesteDoubleJumpState::physics_update(float delta) {
	CelesteAirborneState::physics_update(delta);
	if (controller->current_state != this)
		return;

	Vector3 v = controller->get_velocity();
	if (v.y <= 0.0f) {
		controller->change_state(controller->fall_state);
	}
}

} // namespace godot
