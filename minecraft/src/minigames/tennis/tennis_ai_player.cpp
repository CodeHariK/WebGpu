#include "tennis_ai_player.h"
#include "tennis_ball.h"
#include "tennis_manager.h"
#include "../../game_manager/game_manager.h"
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

void TennisAIPlayer::_bind_methods() {
}

TennisAIPlayer::TennisAIPlayer() {
}

TennisAIPlayer::~TennisAIPlayer() {
}

void TennisAIPlayer::_ready() {
	TennisPlayer::_ready();
	
	// AI shouldn't be the active target for camera usually
}

void TennisAIPlayer::_physics_process(double delta) {
	if (Engine::get_singleton()->is_editor_hint()) return;

	// 1. Find ball if we don't have it
	if (!ball_ref) {
		GameManager *gm = GameManager::get_singleton();
		if (gm && gm->get_tennis_manager()) {
			ball_ref = Object::cast_to<TennisBall>(get_parent()->find_child("TennisBall", true, false));
		}
	}

	if (!ball_ref) return;

	// 2. Simple AI Movement: Follow ball X
	Vector3 ball_pos = ball_ref->get_global_position();
	Vector3 my_pos = get_global_position();
	
	// Move toward ball X, but stay on our side (Z is negative for opponent)
	float target_x = ball_pos.x;
	float target_z = -20.0f; // Opponent baseline area
	
	Vector3 direction = Vector3(target_x - my_pos.x, 0, target_z - my_pos.y); // Wait, my_pos.y is wrong, should be my_pos.z
	direction.z = target_z - my_pos.z;
	direction.y = 0;

	if (direction.length() > 0.1f) {
		direction.normalize();
		Vector3 vel = get_velocity();
		vel.x = UtilityFunctions::lerp(vel.x, direction.x * get_move_speed(), 0.1f);
		vel.z = UtilityFunctions::lerp(vel.z, direction.z * get_move_speed(), 0.1f);
		set_velocity(vel);
	} else {
		Vector3 vel = get_velocity();
		vel.x = UtilityFunctions::lerp(vel.x, 0.0f, 0.1f);
		vel.z = UtilityFunctions::lerp(vel.z, 0.0f, 0.1f);
		set_velocity(vel);
	}

	move_and_slide();

	// 3. AI Swing Logic (Auto-Hit DX-Ball style)
	float distance = my_pos.distance_to(ball_pos);
	float hit_radius = 4.5f;

	if (distance < hit_radius) {
		// Hit direction based on relative position
		Vector3 hit_dir = (ball_pos - my_pos);
		hit_dir.y = 0.2f;
		
		// Ensure hit goes toward player's side (AI is at Z-, so hit toward Z+)
		if (hit_dir.z < 0.5f) hit_dir.z = 1.0f;

		ball_ref->hit(hit_dir.normalized(), get_hitting_speed(), TennisBall::SHOT_FLAT);
		UtilityFunctions::print("AI Player: DX-Ball Style Hit!");
	}
}

} // namespace godot
