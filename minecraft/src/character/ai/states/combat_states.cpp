#include "combat_states.h"
#include "../../../camera/camera.h"
#include "../../../game_manager/game_manager.h"
#include "../../../game_manager/player_input.h"
#include "../../../utils/raycast/mc_raycast.h"
#include "../../physics_character.h"
#include "airborne_states.h"
#include <godot_cpp/classes/input.hpp>
#include <godot_cpp/classes/rigid_body3d.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

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

	GameCamera *cam = gm->get_camera();
	Vector3 cam_pos = cam->get_global_position();
	Vector3 cam_forward = -cam->get_global_transform().basis.get_column(2).normalized();

	// Raycast from camera to find target point (Smart Raycast)
	MCRaycastHit hit = raycast_3d(character, cam_pos, cam_pos + cam_forward * character->get_kick_max_distance(), 0xFFFFFFFF);

	// Visualize Dropkick Search Ray (Cyan)
	character->draw_debug_ray(cam_pos, hit.is_hit ? hit.position : cam_pos + cam_forward * character->get_kick_max_distance(), Color(0, 1, 1), 0.5f);

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

// --- GRAB STATE ---

void CharGrabState::enter() {
	if (!character)
		return;

	GameManager *gm = character->get_game_manager();
	if (!gm || !gm->get_camera()) {
		character->change_state(character->idle_state);
		return;
	}

	GameCamera *cam = gm->get_camera();
	Vector3 cam_pos = cam->get_global_position();
	Vector3 cam_forward = -cam->get_global_transform().basis.get_column(2).normalized();

	// Raycast from camera to find a RigidBody (Hulk Grab)
	MCRaycastHit hit = raycast_3d(character, cam_pos, cam_pos + cam_forward * character->grab_distance * 2.5f, 0xFFFFFFFF);

	// Visualize Grab Search Ray (Yellow)
	character->draw_debug_ray(character->get_global_position(), hit.is_hit ? hit.position : cam_pos + cam_forward * character->grab_distance * 2.5f, Color(1, 1, 0), 0.5f);

	if (hit.is_hit) {
		RigidBody3D *rb = Object::cast_to<RigidBody3D>(hit.collider);
		if (rb && rb != character) {
			character->grabbed_body = rb;

			// Freeze physics while carrying
			rb->set_freeze_enabled(true);
			rb->set_freeze_mode(RigidBody3D::FREEZE_MODE_STATIC);

			UtilityFunctions::print("Character: GRABBED ", rb->get_name());
			return;
		}
	}

	// If no hit or not a RB, go back to idle
	character->change_state(character->idle_state);
}

void CharGrabState::exit() {
	if (character && character->grabbed_body) {
		character->grabbed_body->set_freeze_enabled(false);
		character->grabbed_body = nullptr;
	}
}

void CharGrabState::physics_update(float delta) {
	if (!character || !character->grabbed_body) {
		character->change_state(character->idle_state);
		return;
	}

	const ActionState &state = character->get_player_input()->get_state();

	// Target position: Lift object and hold in front
	Transform3D char_t = character->get_global_transform();
	Vector3 forward = -char_t.basis.get_column(2).normalized();
	Vector3 right = char_t.basis.get_column(0).normalized();
	Vector3 up = char_t.basis.get_column(1).normalized();

	// Relative hold point
	Vector3 hold_pos = char_t.origin +
			(up * character->hold_offset.y) +
			(forward * character->hold_offset.z);

	// Smoothly move the grabbed body (or just snap for Hulk feel)
	character->grabbed_body->set_global_position(hold_pos);

	// Keep the same rotation as the player or just keep it flat
	character->grabbed_body->set_global_rotation(char_t.basis.get_euler_normalized());

	// Movement: Reduced speed while carrying
	character->input_dir = Vector3(state.character.move_axis.x, 0, state.character.move_axis.y);
	Vector3 target_vel = character->input_dir * (character->max_speed * 0.6f);
	Vector3 current_vel = character->get_linear_velocity();
	Vector3 force = (target_vel - Vector3(current_vel.x, 0, current_vel.z)) * character->acceleration;
	character->apply_central_force(force);

	// THROW logic
	if (state.character.grab_just_pressed || state.character.kick_just_pressed) {
		UtilityFunctions::print("Character: THROW!");

		GameManager *gm = character->get_game_manager();
		Vector3 throw_dir = forward;
		if (gm && gm->get_camera()) {
			throw_dir = -gm->get_camera()->get_global_transform().basis.get_column(2).normalized();
		}

		RigidBody3D *rb = character->grabbed_body;
		rb->set_freeze_enabled(false);

		// Apply massive impulse + upward arc
		Vector3 impulse = (throw_dir * character->throw_force) + (Vector3(0, 1, 0) * character->throw_upward_bias);
		rb->set_linear_velocity(impulse);

		// Add some random rotation for flavor
		rb->set_angular_velocity(Vector3(UtilityFunctions::randf_range(-5, 5), UtilityFunctions::randf_range(-5, 5), UtilityFunctions::randf_range(-5, 5)));

		character->grabbed_body = nullptr;
		character->change_state(character->idle_state);
	}
}

} // namespace godot
