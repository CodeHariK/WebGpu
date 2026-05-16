#include "tennis_manager.h"
#include "tennis_ball.h"
#include "../../game_manager/player_input.h"
#include <godot_cpp/classes/box_mesh.hpp>
#include <godot_cpp/classes/capsule_mesh.hpp>
#include <godot_cpp/classes/box_shape3d.hpp>
#include <godot_cpp/classes/collision_shape3d.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/sphere_mesh.hpp>
#include <godot_cpp/classes/sphere_shape3d.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/static_body3d.hpp>
#include "../../game_manager/game_manager.h"
#include "../../player/celeste_controller.h"
#include "tennis_player.h"
#include "tennis_ai_player.h"

namespace godot {

void TennisManager::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_court_width", "width"), &TennisManager::set_court_width);
	ClassDB::bind_method(D_METHOD("get_court_width"), &TennisManager::get_court_width);
	ClassDB::bind_method(D_METHOD("set_court_length", "length"), &TennisManager::set_court_length);
	ClassDB::bind_method(D_METHOD("get_court_length"), &TennisManager::get_court_length);
	ClassDB::bind_method(D_METHOD("set_wall_height", "height"), &TennisManager::set_wall_height);
	ClassDB::bind_method(D_METHOD("get_wall_height"), &TennisManager::get_wall_height);
	ClassDB::bind_method(D_METHOD("set_serve_speed", "speed"), &TennisManager::set_serve_speed);
	ClassDB::bind_method(D_METHOD("get_serve_speed"), &TennisManager::get_serve_speed);

	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "court_width"), "set_court_width", "get_court_width");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "court_length"), "set_court_length", "get_court_length");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "wall_height"), "set_wall_height", "get_wall_height");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "serve_speed"), "set_serve_speed", "get_serve_speed");

	ClassDB::bind_method(D_METHOD("register_player", "player"), &TennisManager::register_player);
}

void TennisManager::_process(double delta) {
	if (Engine::get_singleton()->is_editor_hint())
		return;

	PlayerInput *input = PlayerInput::get_singleton();
	if (!input || !ball_ref)
		return;

	// Press 'A' (or whatever shot_a is mapped to) to serve when ball is dead/idle
	if (input->get_state().tennis.shot_a_just_pressed) {
		if (ball_ref->get_state() == TennisBall::STATE_DEAD || ball_ref->get_state() == TennisBall::STATE_IDLE) {
			// Serve from player1 position
			if (player1) {
				float p_hit_speed = player1->get_hitting_speed();

				Vector3 serve_pos = player1->get_global_position() + Vector3(0, 1.5f, 0);
				// Hitting towards -Z (opponent side)
				Vector3 serve_dir = Vector3(0, 0.15f, -1.0f).normalized();
				ball_ref->serve(serve_pos, serve_dir, p_hit_speed);
			}
		}
	}
}

TennisManager::TennisManager() {}
TennisManager::~TennisManager() {}

void TennisManager::_ready() {
	if (Engine::get_singleton()->is_editor_hint())
		return;

	// Register with GameManager
	GameManager *gm = GameManager::get_singleton();
	if (gm) {
		gm->register_tennis_manager(this);
	}

	_spawn_court();
	_spawn_walls();
	_spawn_ball();
	_spawn_human_player();
	_spawn_ai_player();
}

void TennisManager::_spawn_court() {
	// 1. Create Ground
	StaticBody3D *ground = memnew(StaticBody3D);
	ground->set_name("TennisCourt");

	CollisionShape3D *col = memnew(CollisionShape3D);
	BoxShape3D *box = memnew(BoxShape3D);
	box->set_size(Vector3(court_width, 1.0f, court_length));
	col->set_shape(box);
	ground->add_child(col);

	MeshInstance3D *mesh = memnew(MeshInstance3D);
	BoxMesh *bm = memnew(BoxMesh);
	bm->set_size(Vector3(court_width, 1.0f, court_length));
	mesh->set_mesh(bm);

	StandardMaterial3D *mat = memnew(StandardMaterial3D);
	mat->set_albedo(Color(0.2f, 0.5f, 0.2f)); // Green court
	mesh->set_material_override(mat);
	ground->add_child(mesh);

	add_child(ground);
	ground->set_global_position(Vector3(0, -0.5f, 0));

	// 2. Create Net
	StaticBody3D *net = memnew(StaticBody3D);
	net->set_name("TennisNet");

	CollisionShape3D *ncol = memnew(CollisionShape3D);
	BoxShape3D *nbox = memnew(BoxShape3D);
	nbox->set_size(Vector3(32.0f, 1.5f, 0.2f));
	ncol->set_shape(nbox);
	net->add_child(ncol);

	MeshInstance3D *nmesh = memnew(MeshInstance3D);
	BoxMesh *nbm = memnew(BoxMesh);
	nbm->set_size(Vector3(32.0f, 1.5f, 0.2f));
	nmesh->set_mesh(nbm);

	StandardMaterial3D *nmat = memnew(StandardMaterial3D);
	nmat->set_albedo(Color(0.9f, 0.9f, 0.9f)); // White net
	nmesh->set_material_override(nmat);
	net->add_child(nmesh);

	add_child(net);
	net->set_global_position(Vector3(0, 0.75f, 0));
}

void TennisManager::_spawn_walls() {
	// 1. Back Wall (+Z)
	_create_wall(Vector3(court_width, wall_height, wall_thickness),
			Vector3(0, wall_height / 2.0f, court_length / 2.0f + wall_thickness / 2.0f),
			"BackWall");

	// 2. Front Wall (-Z)
	_create_wall(Vector3(court_width, wall_height, wall_thickness),
			Vector3(0, wall_height / 2.0f, -court_length / 2.0f - wall_thickness / 2.0f),
			"FrontWall");

	// 3. Left Wall (-X)
	_create_wall(Vector3(wall_thickness, wall_height, court_length + wall_thickness * 2.0f),
			Vector3(-court_width / 2.0f - wall_thickness / 2.0f, wall_height / 2.0f, 0),
			"LeftWall");

	// 4. Right Wall (+X)
	_create_wall(Vector3(wall_thickness, wall_height, court_length + wall_thickness * 2.0f),
			Vector3(court_width / 2.0f + wall_thickness / 2.0f, wall_height / 2.0f, 0),
			"RightWall");
}

void TennisManager::_create_wall(Vector3 size, Vector3 position, String name) {
	StaticBody3D *wall = memnew(StaticBody3D);
	wall->set_name(name);

	CollisionShape3D *col = memnew(CollisionShape3D);
	BoxShape3D *box = memnew(BoxShape3D);
	box->set_size(size);
	col->set_shape(box);
	wall->add_child(col);

	MeshInstance3D *mesh = memnew(MeshInstance3D);
	BoxMesh *bm = memnew(BoxMesh);
	bm->set_size(size);
	mesh->set_mesh(bm);

	StandardMaterial3D *mat = memnew(StandardMaterial3D);
	mat->set_albedo(Color(0.5f, 0.5f, 0.8f, 0.3f)); // Semi-transparent blue
	mat->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
	mesh->set_material_override(mat);
	wall->add_child(mesh);

	add_child(wall);
	wall->set_global_position(position);
}

void TennisManager::_spawn_ball() {
	TennisBall *ball = memnew(TennisBall);
	ball->set_name("TennisBall");
	
	// Add visual for the ball
	MeshInstance3D *mesh = memnew(MeshInstance3D);
	SphereMesh *sm = memnew(SphereMesh);
	sm->set_radius(0.15f);
	sm->set_height(0.3f);
	mesh->set_mesh(sm);
	
	StandardMaterial3D *mat = memnew(StandardMaterial3D);
	mat->set_albedo(Color(0.8f, 0.9f, 0.1f)); // Yellow tennis ball
	mesh->set_material_override(mat);
	ball->add_child(mesh);

	// Add collision for the ball
	CollisionShape3D *col = memnew(CollisionShape3D);
	SphereShape3D *ss = memnew(SphereShape3D);
	ss->set_radius(0.15f);
	col->set_shape(ss);
	ball->add_child(col);

	add_child(ball);
	ball->set_global_position(Vector3(0, 5, 0)); // Spawn in middle air
	ball->set_state(TennisBall::STATE_DEAD); // Start as dead so we can serve
	ball_ref = ball;
}

void TennisManager::_spawn_human_player() {
	TennisPlayer *player = memnew(TennisPlayer);
	player->set_name("Human_Player");

	// Add visual for Player (Blue Capsule)
	MeshInstance3D *mesh = memnew(MeshInstance3D);
	CapsuleMesh *cm = memnew(CapsuleMesh);
	mesh->set_mesh(cm);

	StandardMaterial3D *mat = memnew(StandardMaterial3D);
	mat->set_albedo(Color(0.2f, 0.2f, 0.8f)); // Blue Player
	mesh->set_material_override(mat);
	player->add_child(mesh);

	add_child(player);
	player->set_global_position(Vector3(0, 1.0f, 18.0f)); // Human baseline

	// Register as player 1
	register_player(player);
}

void TennisManager::_spawn_ai_player() {
	TennisAIPlayer *ai = memnew(TennisAIPlayer);
	ai->set_name("AI_Opponent");
	
	// Add visual for AI (Red Capsule)
	MeshInstance3D *mesh = memnew(MeshInstance3D);
	CapsuleMesh *cm = memnew(CapsuleMesh);
	mesh->set_mesh(cm);
	
	StandardMaterial3D *mat = memnew(StandardMaterial3D);
	mat->set_albedo(Color(0.8f, 0.2f, 0.2f)); // Red AI
	mesh->set_material_override(mat);
	ai->add_child(mesh);

	add_child(ai);
	ai->set_global_position(Vector3(0, 1.0f, -18.0f)); // Opposite baseline
	ai->rotate_y(Math_PI); // Face the player
	
	// Register as player 2
	register_player(ai);
}

void TennisManager::register_player(TennisPlayer *p_player) {
	if (!player1) {
		player1 = p_player;
		UtilityFunctions::print("TennisManager: Player 1 Registered.");
	} else if (!player2) {
		player2 = p_player;
		UtilityFunctions::print("TennisManager: Player 2 Registered.");
	}
}

} // namespace godot
