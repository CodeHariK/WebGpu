#include "tennis_player.h"
#include "tennis_ball.h"
#include "../../game_manager/player_input.h"
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/input.hpp>
#include <godot_cpp/classes/collision_shape3d.hpp>

namespace godot {

void TennisPlayer::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_move_speed", "speed"), &TennisPlayer::set_move_speed);
	ClassDB::bind_method(D_METHOD("get_move_speed"), &TennisPlayer::get_move_speed);
	
	ClassDB::bind_method(D_METHOD("_on_swing_area_body_entered", "body"), &TennisPlayer::_on_swing_area_body_entered);
	ClassDB::bind_method(D_METHOD("_on_swing_area_body_exited", "body"), &TennisPlayer::_on_swing_area_body_exited);

	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "move_speed"), "set_move_speed", "get_move_speed");
}

TennisPlayer::TennisPlayer() {}
TennisPlayer::~TennisPlayer() {}

void TennisPlayer::_ready() {
	if (Engine::get_singleton()->is_editor_hint()) return;

	player_input = PlayerInput::get_singleton();

	// Find Swing Area
	swing_area = Object::cast_to<Area3D>(find_child("SwingArea", true, false));
	if (swing_area) {
		swing_area->connect("body_entered", Callable(this, "_on_swing_area_body_entered"));
		swing_area->connect("body_exited", Callable(this, "_on_swing_area_body_exited"));
	}

	// Create Cuboid Collider if it doesn't exist
	if (find_child("CollisionShape3D", true, false) == nullptr) {
		CollisionShape3D *col = memnew(CollisionShape3D);
		BoxShape3D *box = memnew(BoxShape3D);
		box->set_size(Vector3(1.0f, 2.0f, 1.0f));
		col->set_shape(box);
		add_child(col);
	}
}

void TennisPlayer::_physics_process(double delta) {
	if (Engine::get_singleton()->is_editor_hint()) return;

	_handle_movement(delta);
	_handle_swing();
}

void TennisPlayer::_handle_movement(double delta) {
	if (!player_input) return;

	Vector2 input = player_input->get_move_axis();
	Vector3 direction = Vector3(input.x, 0, input.y);

	Vector3 vel = get_velocity();
	if (direction.length() > 0.1f) {
		vel.x = UtilityFunctions::move_toward(vel.x, direction.x * move_speed, acceleration * delta);
		vel.z = UtilityFunctions::move_toward(vel.z, direction.z * move_speed, acceleration * delta);
		
		// Look in movement direction
		Vector3 look_target = get_global_position() + direction;
		look_at(look_target, Vector3(0, 1, 0));
	} else {
		vel.x = UtilityFunctions::move_toward(vel.x, 0.0f, friction * delta);
		vel.z = UtilityFunctions::move_toward(vel.z, 0.0f, friction * delta);
	}

	set_velocity(vel);
	move_and_slide();
}

void TennisPlayer::_handle_swing() {
	if (!player_input || !target_ball) return;

	const ActionState &state = player_input->get_state();
	TennisBall::ShotType shot_type;
	bool hit_pressed = false;

	if (state.tennis.shot_a_just_pressed) {
		shot_type = TennisBall::SHOT_TOPSPIN;
		hit_pressed = true;
	} else if (state.tennis.shot_b_just_pressed) {
		shot_type = TennisBall::SHOT_SLICE;
		hit_pressed = true;
	} else if (state.tennis.shot_y_just_pressed) {
		shot_type = TennisBall::SHOT_FLAT;
		hit_pressed = true;
	} else if (state.tennis.shot_x_just_pressed) {
		// Lob if pressing 'Up' (move_forward), else Drop
		if (state.character.move_axis.y < -0.5f) {
			shot_type = TennisBall::SHOT_LOB;
		} else {
			shot_type = TennisBall::SHOT_DROP;
		}
		hit_pressed = true;
	}

	if (hit_pressed) {
		// Hit direction based on movement axis + forward bias
		Vector2 move = player_input->get_move_axis();
		Vector3 hit_dir = Vector3(move.x, 0, -1.0f).normalized(); // Default to hitting forward (-Z)
		
		// Adjust for which side of the court we are on? 
		// For now, let's just use the player's forward vector
		hit_dir = -get_global_transform().get_basis().get_column(2); // Forward is -Z

		target_ball->hit(hit_dir, 20.0f, shot_type);
	}
}

void TennisPlayer::_on_swing_area_body_entered(Node *p_body) {
	TennisBall *ball = Object::cast_to<TennisBall>(p_body);
	if (ball) {
		target_ball = ball;
	}
}

void TennisPlayer::_on_swing_area_body_exited(Node *p_body) {
	if (target_ball == p_body) {
		target_ball = nullptr;
	}
}

} // namespace godot
