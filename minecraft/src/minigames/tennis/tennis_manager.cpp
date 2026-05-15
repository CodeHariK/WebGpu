#include "tennis_manager.h"
#include <godot_cpp/classes/static_body3d.hpp>
#include <godot_cpp/classes/collision_shape3d.hpp>
#include <godot_cpp/classes/box_shape3d.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/box_mesh.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/engine.hpp>

namespace godot {

void TennisManager::_bind_methods() {
}

TennisManager::TennisManager() {}
TennisManager::~TennisManager() {}

void TennisManager::_ready() {
	if (Engine::get_singleton()->is_editor_hint()) return;
	_spawn_court();
}

void TennisManager::_spawn_court() {
	// 1. Create Ground
	StaticBody3D *ground = memnew(StaticBody3D);
	ground->set_name("TennisCourt");
	
	CollisionShape3D *col = memnew(CollisionShape3D);
	BoxShape3D *box = memnew(BoxShape3D);
	box->set_size(Vector3(30.0f, 1.0f, 60.0f));
	col->set_shape(box);
	ground->add_child(col);

	MeshInstance3D *mesh = memnew(MeshInstance3D);
	BoxMesh *bm = memnew(BoxMesh);
	bm->set_size(Vector3(30.0f, 1.0f, 60.0f));
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

} // namespace godot
