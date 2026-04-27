#include "tennis_ai.h"
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

void TennisAI::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_ball", "ball"), &TennisAI::set_ball);
}

TennisAI::TennisAI() {
	ball_reference = nullptr;
	is_tracking_ball = false;
	predicted_landing_spot = Vector3(0, 0, 0);
}

TennisAI::~TennisAI() {
}

void TennisAI::_ready() {
	TennisPlayer::_ready();
}

void TennisAI::_physics_process(double delta) {
	TennisPlayer::_physics_process(delta);
	
	if (Engine::get_singleton()->is_editor_hint()) {
		return;
	}

	if (ball_reference) {
		// A very simplistic AI that just runs toward the ball's X and Z coordinates.
		// A more advanced AI would predict the landing spot based on ball velocity.
		Vector3 ball_pos = ball_reference->get_global_position();
		Vector3 my_pos = get_global_position();
		
		Vector3 diff = ball_pos - my_pos;
		Vector2 input_dir(0, 0);
		
		// AI is positioned at Z < 0
		if (ball_pos.z < 0) { 
			if (Math::abs(diff.x) > 0.5f || Math::abs(diff.z) > 1.0f) {
				Vector2 dir(diff.x, diff.z);
				dir = dir.normalized();
				input_dir = dir;
			} else {
				// We are close enough to the ball, try to swing
				// Check if the ball is within hit height
				if (ball_pos.y < 3.0f && ball_pos.y > 0.5f) {
					swing();
				}
			}
		} else {
			// Move back to center on AI side
			Vector3 center_pos(0, 0, -10.0f); // Default AI base position
			Vector3 center_diff = center_pos - my_pos;
			if (center_diff.length() > 1.0f) {
				Vector2 dir(center_diff.x, center_diff.z);
				input_dir = dir.normalized();
			}
		}
		
		move(input_dir);
	}
}

void TennisAI::set_ball(Node3D *p_ball) {
	ball_reference = p_ball;
}

} // namespace godot
