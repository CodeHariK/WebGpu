#include "character_states.h"
#include "../../game_manager/game_manager.h"
#include "../physics_character.h"
#include <godot_cpp/classes/input.hpp>
#include <godot_cpp/classes/rigid_body3d.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include "../../camera/camera.h"
#include "../../utils/raycast/mc_raycast.h"

namespace godot {

// --- GROUNDED STATE (Parent) ---

void CharGroundedState::physics_update(float delta) {
	if (!character)
		return;

	// Check if we lost ground
	if (!character->is_grounded) {
		character->change_state(character->fall_state);
		return;
	}

	// Handle Jump input (Space) via ActionState
	if (!character->get_player_input()) return;

	const ActionState &state = character->get_player_input()->get_state();

	if (state.jump_just_pressed) {
		character->change_state(character->jump_state);
		return;
	}

	// Basic Horizontal Movement Input (ActionState Axis)
	character->input_dir = Vector3(state.move_axis.x, 0, state.move_axis.y);

	// State transition logic based on input
	if (character->input_dir.length() > 0.01f) {
		if (character->current_state == character->idle_state) {
			character->change_state(character->move_state);
		}
	} else {
		if (character->current_state == character->move_state) {
			character->change_state(character->idle_state);
		}
	}
}

// --- AIRBORNE STATE (Parent) ---

void CharAirborneState::physics_update(float delta) {
	if (!character)
		return;

	// Grounded check in air (with upward velocity hysteresis)
	if (character->is_grounded && character->get_linear_velocity().y <= 0.1f) {
		character->change_state(character->idle_state);
		return;
	}

	// Dropkick and Move input via ActionState
	if (!character->get_player_input()) return;

	const ActionState &state = character->get_player_input()->get_state();

	// Trigger Dropkick
	if (state.kick_just_pressed) {
		character->change_state(character->dropkick_state);
		return;
	}

	character->input_dir = Vector3(state.move_axis.x, 0, state.move_axis.y);
}

// --- IDLE STATE ---

void CharIdleState::enter() {
	// Potential animation triggers
}

void CharIdleState::physics_update(float delta) {
	CharGroundedState::physics_update(delta);

	if (!character)
		return;

	// Apply horizontal damping to prevent sliding
	Vector3 vel = character->get_linear_velocity();
	Vector3 horizontal_vel = Vector3(vel.x, 0.0f, vel.z);

	if (horizontal_vel.length() > 0.1f) {
		character->apply_central_force(-horizontal_vel * character->spring_damping * 0.5f);
	}
}

// --- MOVE STATE ---

void CharMoveState::physics_update(float delta) {
	CharGroundedState::physics_update(delta);

	if (!character || character->input_dir.length() < 0.01f)
		return;

	// Calculate target horizontal velocity
	Vector3 target_vel = character->input_dir * character->max_speed;
	Vector3 current_vel = character->get_linear_velocity();
	Vector3 current_horizontal_vel = Vector3(current_vel.x, 0.0f, current_vel.z);

	// Simple acceleration force
	Vector3 force = (target_vel - current_horizontal_vel) * character->acceleration;
	character->apply_central_force(force);
}

// --- JUMP STATE ---

void CharJumpState::enter() {
	if (!character)
		return;

	// Apply upward impulse
	Vector3 vel = character->get_linear_velocity();
	character->set_linear_velocity(Vector3(vel.x, 0.0f, vel.z)); // Reset Y for consistent jump
	character->apply_central_impulse(Vector3(0, character->jump_impulse, 0));

	UtilityFunctions::print("Character: Jumping!");
}


// --- DROPKICK STATE ---

void CharDropkickState::enter() {
	if (!character)
		return;

	GameManager *gm = character->get_game_manager();
	if (!gm || !gm->get_camera()) {
		has_target = false;
		character->change_state(character->fall_state);
		return;
	}

	MCCamera *cam = gm->get_camera();
	Vector3 cam_pos = cam->get_global_position();
	Vector3 cam_forward = -cam->get_global_transform().basis.get_column(2).normalized();

	// Raycast from camera to find target point (Smart Raycast)
	MCRaycastHit hit = raycast_3d(character, cam_pos, cam_pos + cam_forward * character->get_kick_max_distance(), 0xFFFFFFFF);

	if (hit.is_hit) {
		target_point = hit.position;
		has_target = true;
		kick_timer = 0.0f;
		UtilityFunctions::print("Character: Dropkick target locked at ", target_point);
	} else {
		has_target = false;
		character->change_state(character->fall_state);
	}
}

void CharDropkickState::physics_update(float delta) {
	if (!character || !has_target)
		return;

	Vector3 current_pos = character->get_global_position();
	Vector3 to_target = target_point - current_pos;
	float dist = to_target.length();

	// Impact detection (proximity or timeout)
	if (dist < 1.2f || kick_timer > 0.8f) {
		UtilityFunctions::print("Character: Dropkick IMPACT!");

		// Transfer kinetic energy to target if it's a RigidBody
		MCRaycastHit hit = raycast_3d(character, current_pos, target_point + (to_target.normalized() * 0.5f), 0xFFFFFFFF);
		if (hit.is_hit) {
			RigidBody3D *rb = Object::cast_to<RigidBody3D>(hit.collider);
			if (rb) {
				Vector3 impact_dir = to_target.normalized();
				rb->apply_impulse(impact_dir * character->get_kick_impact_force(), hit.position - rb->get_global_position());
				UtilityFunctions::print("Character: Transferred ", character->get_kick_impact_force(), " force to ", rb->get_name());
			}
		}

		// Physical response: Bounce back and up
		Vector3 bounce = -to_target.normalized() * 8.0f + Vector3(0, 6, 0);
		character->set_linear_velocity(bounce);

		// Transition back to fall
		character->change_state(character->fall_state);
		return;
	}

	// Hybrid override: Force precise velocity toward target
	Vector3 velocity = to_target.normalized() * character->get_kick_speed();
	character->set_linear_velocity(velocity);

	kick_timer += delta;
}

void CharJumpState::physics_update(float delta) {
	CharAirborneState::physics_update(delta);

	// Transition to fall on downward velocity
	if (character->get_linear_velocity().y < -0.1f) {
		character->change_state(character->fall_state);
	}
}

// --- FALL STATE ---

void CharFallState::physics_update(float delta) {
	CharAirborneState::physics_update(delta);

	// Optional: Reduced air control logic here
	if (character->input_dir.length() > 0.01f) {
		character->apply_central_force(character->input_dir * character->acceleration * 0.2f);
	}
}

} // namespace godot
