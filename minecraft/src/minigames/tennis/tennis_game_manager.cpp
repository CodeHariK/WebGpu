#include "tennis_game_manager.h"
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

void TennisGameManager::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_ball", "ball"), &TennisGameManager::set_ball);
	ClassDB::bind_method(D_METHOD("set_player", "player"), &TennisGameManager::set_player);
	ClassDB::bind_method(D_METHOD("set_ai", "ai"), &TennisGameManager::set_ai);
	ClassDB::bind_method(D_METHOD("start_serve"), &TennisGameManager::start_serve);
	ClassDB::bind_method(D_METHOD("add_point", "player_won"), &TennisGameManager::add_point);

	BIND_ENUM_CONSTANT(WAITING_FOR_SERVE);
	BIND_ENUM_CONSTANT(RALLY);
	BIND_ENUM_CONSTANT(POINT_OVER);
}

TennisGameManager::TennisGameManager() {
	current_state = WAITING_FOR_SERVE;
	player_score = 0;
	ai_score = 0;
	ball = nullptr;
	player = nullptr;
	ai = nullptr;
}

TennisGameManager::~TennisGameManager() {
}

void TennisGameManager::_ready() {
	if (Engine::get_singleton()->is_editor_hint()) {
		return;
	}
}

void TennisGameManager::_process(double delta) {
	if (Engine::get_singleton()->is_editor_hint()) {
		return;
	}
	
	if (current_state == WAITING_FOR_SERVE && ball && ball->get_is_active()) {
		current_state = RALLY;
	}
	
	if (current_state == RALLY && ball) {
		// Basic out of bounds check
		Vector3 ball_pos = ball->get_global_position();
		
		// If ball bounces twice, or goes out of bounds (let's say court is -10 to 10 in x, -20 to 20 in z)
		if (ball->get_bounce_count() >= 2) {
			// Whoever's side it bounced on loses the point
			if (ball_pos.z > 0) {
				// Player's side (assuming player is at Z > 0)
				add_point(false); // AI won
			} else {
				// AI's side
				add_point(true); // Player won
			}
		} else if (Math::abs(ball_pos.x) > 12.0f || Math::abs(ball_pos.z) > 22.0f) {
			// Out of bounds
			if (ball_pos.z > 0) {
				// Ball went out on player's side
				add_point(false);
			} else {
				add_point(true);
			}
		}
	}
}

void TennisGameManager::set_ball(Node *p_ball) {
	ball = Object::cast_to<TennisBall>(p_ball);
}

void TennisGameManager::set_player(Node *p_player) {
	player = Object::cast_to<TennisPlayer>(p_player);
}

void TennisGameManager::set_ai(Node *p_ai) {
	ai = Object::cast_to<TennisPlayer>(p_ai);
}

void TennisGameManager::start_serve() {
	if (ball && player) {
		current_state = WAITING_FOR_SERVE;
		// Position exactly inside the swing hitbox (offset is approx 0, 0.5, -1.5)
		ball->reset_ball(player->get_global_position() + Vector3(0, 0.5f, -1.5f));
		
		// Set player to SERVING state so they can toss/hit
		player->set_state(TennisPlayer::SERVING);
	}
}

void TennisGameManager::add_point(bool p_player_won) {
	if (current_state == POINT_OVER) return;
	
	current_state = POINT_OVER;
	
	if (p_player_won) {
		player_score++;
		UtilityFunctions::print("Player wins point! Score: ", get_score_string(player_score), " - ", get_score_string(ai_score));
	} else {
		ai_score++;
		UtilityFunctions::print("AI wins point! Score: ", get_score_string(player_score), " - ", get_score_string(ai_score));
	}
	
	// Reset after a short delay (this is simple immediate reset for prototype)
	start_serve();
}

String TennisGameManager::get_score_string(int p_score) const {
	switch(p_score) {
		case 0: return "0";
		case 1: return "15";
		case 2: return "30";
		case 3: return "40";
		default: return "Game";
	}
}

} // namespace godot
