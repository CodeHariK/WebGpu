#include "grounded_states.h"
#include "airborne_states.h"
#include "combat_states.h"
#include "../../physics_character.h"
#include "../../../game_manager/player_input.h"
#include <godot_cpp/classes/rigid_body3d.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

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

	if (state.character.jump_just_pressed) {
		character->change_state(character->jump_state);
		return;
	}

	if (state.character.grab_just_pressed) {
		character->change_state(character->grab_state);
		return;
	}

	// Basic Horizontal Movement Input (ActionState Axis)
	character->input_dir = Vector3(state.character.move_axis.x, 0, state.character.move_axis.y);

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

} // namespace godot
