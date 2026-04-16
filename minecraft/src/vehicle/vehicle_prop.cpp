#include "vehicle_prop.h"
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/classes/engine.hpp>

namespace godot {

void VehicleProp::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_mesh", "mesh"), &VehicleProp::set_mesh);
	ClassDB::bind_method(D_METHOD("get_mesh"), &VehicleProp::get_mesh);

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "mesh", PROPERTY_HINT_RESOURCE_TYPE, "Mesh"), "set_mesh", "get_mesh");
}

VehicleProp::VehicleProp() {
}

VehicleProp::~VehicleProp() {
}

void VehicleProp::_ready() {
	if (Engine::get_singleton()->is_editor_hint()) return;
	_update_prop();
}

void VehicleProp::set_mesh(const Ref<Mesh> &p_mesh) {
	mesh = p_mesh;
	if (is_inside_tree()) {
		_update_prop();
	}
}

Ref<Mesh> VehicleProp::get_mesh() const {
	return mesh;
}

void VehicleProp::_update_prop() {
	if (mesh.is_null()) return;

	// 1. Setup MeshInstance if it doesn't exist
	if (!mesh_instance) {
		mesh_instance = memnew(MeshInstance3D);
		mesh_instance->set_name("PropMesh");
		add_child(mesh_instance);
	}
	mesh_instance->set_mesh(mesh);

	// 2. Setup Collision if it doesn't exist
	if (!collision_shape) {
		collision_shape = memnew(CollisionShape3D);
		collision_shape->set_name("PropCollider");
		add_child(collision_shape);
	}

	// 3. Generate Concave (Trimesh) Collision from the mesh
	Ref<ConcavePolygonShape3D> trimesh_shape = mesh->create_trimesh_shape();
	collision_shape->set_shape(trimesh_shape);
}

} // namespace godot
