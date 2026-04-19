#include "airborne_states.h"
#include "../../game_manager/player_input.h"
#include "../physics_character.h"
#include "combat_states.h"
#include "grounded_states.h"
#include <godot_cpp/classes/rigid_body3d.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

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
	if (!character->get_player_input())
		return;

	const ActionState &state = character->get_player_input()->get_state();

	// Trigger Dropkick
	if (state.character.kick_just_pressed) {
		character->change_state(character->dropkick_state);
		return;
	}

	character->input_dir = Vector3(state.character.move_axis.x, 0, state.character.move_axis.y);
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
