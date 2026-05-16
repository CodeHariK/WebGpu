#include "tennis_player.h"
#include "tennis_ball.h"
#include "../../game_manager/player_input.h"
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/area3d.hpp>
#include <godot_cpp/classes/box_shape3d.hpp>
#include "tennis_manager.h"
#include "../../game_manager/game_manager.h"
#include <godot_cpp/classes/input.hpp>
#include <godot_cpp/classes/collision_shape3d.hpp>
#include <godot_cpp/classes/box_mesh.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>

namespace godot {

void TennisPlayer::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_move_speed", "speed"), &TennisPlayer::set_move_speed);
	ClassDB::bind_method(D_METHOD("get_move_speed"), &TennisPlayer::get_move_speed);
	
	ClassDB::bind_method(D_METHOD("set_hitting_speed", "speed"), &TennisPlayer::set_hitting_speed);
	ClassDB::bind_method(D_METHOD("get_hitting_speed"), &TennisPlayer::get_hitting_speed);

	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "move_speed"), "set_move_speed", "get_move_speed");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "hitting_speed"), "set_hitting_speed", "get_hitting_speed");
}

TennisPlayer::TennisPlayer() {}
TennisPlayer::~TennisPlayer() {}

void TennisPlayer::_ready() {
	if (Engine::get_singleton()->is_editor_hint()) return;

	player_input = PlayerInput::get_singleton();

	// Create Cuboid Collider if it doesn't exist
	if (find_child("CollisionShape3D", true, false) == nullptr) {
		CollisionShape3D *col = memnew(CollisionShape3D);
		BoxShape3D *box = memnew(BoxShape3D);
		box->set_size(Vector3(1.0f, 2.0f, 1.0f));
		col->set_shape(box);
		add_child(col);
	}

	// Register with GameManager (as active target)
	GameManager *gm = GameManager::get_singleton();
	if (gm) {
		if (gm->get_active_target() == nullptr) {
			gm->set_active_target(this);
		}

		// Register with TennisManager if it exists
		TennisManager *tm = gm->get_tennis_manager();
		if (tm) {
			tm->register_player(this);
		}
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
	if (!player_input) return;

	const ActionState &state = player_input->get_state();
	bool hit_pressed = (state.tennis.shot_a_just_pressed || state.tennis.shot_b_just_pressed || 
	                    state.tennis.shot_x_just_pressed || state.tennis.shot_y_just_pressed);

	if (!hit_pressed) return;

	// Find ball via manager
	GameManager *gm = GameManager::get_singleton();
	if (!gm || !gm->get_tennis_manager()) return;
	TennisBall *ball = gm->get_tennis_manager()->get_ball();
	if (!ball) return;

	// DX-Ball Style Position Check
	Vector3 ball_pos = ball->get_global_position();
	Vector3 my_pos = get_global_position();

	// Check 3D distance between player and ball
	float distance = my_pos.distance_to(ball_pos);
	float hit_radius = 4.5f;

	if (distance < hit_radius) {
		TennisBall::ShotType shot_type = TennisBall::SHOT_FLAT;
		if (state.tennis.shot_a_just_pressed) shot_type = TennisBall::SHOT_TOPSPIN;
		else if (state.tennis.shot_b_just_pressed) shot_type = TennisBall::SHOT_SLICE;

		// Hit direction based on relative position (DX-Ball style)
		// We subtract positions to get the vector from player to ball
		Vector3 hit_dir = (ball_pos - my_pos);
		hit_dir.y = 0.2f; // Give it some arc
		
		// Ensure the hit always goes toward the opponent's side (Human is at Z+, so hit toward Z-)
		if (hit_dir.z > -0.5f) hit_dir.z = -1.0f; 

		ball->hit(hit_dir.normalized(), hitting_speed, shot_type);
		UtilityFunctions::print("Human Player: DX-Ball Style Hit!");
	}
}

} // namespace godot
