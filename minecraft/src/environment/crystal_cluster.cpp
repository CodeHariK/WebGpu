/**
 * @file crystal_cluster.cpp
 * @brief CrystalCluster: mesh hookup, prop material, collision maintenance.
 */
#include "crystal_cluster.h"
#include "prop_material.h"
#include <godot_cpp/core/class_db.hpp>

namespace godot {

void CrystalCluster::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_crystal_mesh", "mesh"), &CrystalCluster::set_crystal_mesh);
	ClassDB::bind_method(D_METHOD("get_crystal_mesh"), &CrystalCluster::get_crystal_mesh);
	ADD_PROPERTY(
			PropertyInfo(Variant::OBJECT, "crystal_mesh", PROPERTY_HINT_RESOURCE_TYPE, "CrystalClusterMesh"),
			"set_crystal_mesh", "get_crystal_mesh"
	);
	ClassDB::bind_method(D_METHOD("set_material", "material"), &CrystalCluster::set_material);
	ClassDB::bind_method(D_METHOD("get_material"), &CrystalCluster::get_material);
	ADD_PROPERTY(
			PropertyInfo(Variant::OBJECT, "material", PROPERTY_HINT_RESOURCE_TYPE, "Material"), "set_material",
			"get_material"
	);
	ClassDB::bind_method(D_METHOD("set_glow", "value"), &CrystalCluster::set_glow);
	ClassDB::bind_method(D_METHOD("get_glow"), &CrystalCluster::get_glow);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "glow", PROPERTY_HINT_RANGE, "0,4,0.05"), "set_glow", "get_glow");
	ClassDB::bind_method(D_METHOD("set_transmission", "value"), &CrystalCluster::set_transmission);
	ClassDB::bind_method(D_METHOD("get_transmission"), &CrystalCluster::get_transmission);
	ADD_PROPERTY(
			PropertyInfo(Variant::FLOAT, "transmission", PROPERTY_HINT_RANGE, "0,2,0.05"), "set_transmission",
			"get_transmission"
	);
	ClassDB::bind_method(D_METHOD("set_collision_enabled", "enabled"), &CrystalCluster::set_collision_enabled);
	ClassDB::bind_method(D_METHOD("get_collision_enabled"), &CrystalCluster::get_collision_enabled);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "collision_enabled"), "set_collision_enabled", "get_collision_enabled");
	ClassDB::bind_method(D_METHOD("_on_crystal_mesh_changed"), &CrystalCluster::_on_crystal_mesh_changed);
}

CrystalCluster::CrystalCluster() {}
CrystalCluster::~CrystalCluster() {}

/// The displayed mesh and material are derived: visible in the inspector, never saved.
void CrystalCluster::_validate_property(PropertyInfo &p_property) const {
	if (p_property.name == StringName("mesh") || p_property.name == StringName("material_override")) {
		p_property.usage &= ~PROPERTY_USAGE_STORAGE;
	}
}

void CrystalCluster::_notification(int p_what) {
	if (p_what == NOTIFICATION_READY) {
		if (crystal_mesh.is_null()) {
			set_crystal_mesh(memnew(CrystalClusterMesh));
		} else {
			_apply_mesh();
		}
	}
}

void CrystalCluster::set_crystal_mesh(const Ref<CrystalClusterMesh> &p_mesh) {
	const Callable cb(this, "_on_crystal_mesh_changed");
	if (crystal_mesh.is_valid() && crystal_mesh->is_connected("changed", cb)) {
		crystal_mesh->disconnect("changed", cb);
	}
	crystal_mesh = p_mesh;
	if (crystal_mesh.is_valid()) {
		crystal_mesh->connect("changed", cb);
	}
	_apply_mesh();
}

void CrystalCluster::set_material(const Ref<Material> &p_material) {
	material = p_material;
	_apply_material();
}

void CrystalCluster::set_glow(float p_glow) {
	glow = MAX(0.0f, p_glow);
	_apply_material();
}

void CrystalCluster::set_transmission(float p_amount) {
	transmission = MAX(0.0f, p_amount);
	_apply_material();
}

void CrystalCluster::set_collision_enabled(bool p_enabled) {
	if (collision_enabled != p_enabled) {
		collision_enabled = p_enabled;
		_update_collision();
	}
}

void CrystalCluster::_on_crystal_mesh_changed() { _update_collision(); }

void CrystalCluster::_apply_mesh() {
	set_mesh(crystal_mesh);
	_apply_material();
	_update_collision();
}

/// `material` if set, else the shared prop shader with this node's glow / transmission.
void CrystalCluster::_apply_material() {
	if (material.is_valid()) {
		set_material_override(material);
		return;
	}
	if (_prop_material.is_null()) {
		_prop_material = make_prop_material();
	}
	_prop_material->set_shader_parameter("glow", glow);
	_prop_material->set_shader_parameter("transmission", transmission);
	set_material_override(_prop_material);
}

// ---------------------------------------------------------------------------------------------
// Collision (internal children: rebuilt every load, never serialized)
// ---------------------------------------------------------------------------------------------

void CrystalCluster::_update_collision() {
	if (!collision_enabled || crystal_mesh.is_null()) {
		if (static_body) {
			static_body->queue_free();
			static_body = nullptr;
			collision_shape = nullptr;
		}
		return;
	}
	Ref<Shape3D> shape = crystal_mesh->create_collision_shape();
	if (shape.is_null()) {
		return; // Not built yet; the mesh's `changed` signal brings us back
	}
	if (!static_body) {
		static_body = memnew(StaticBody3D);
		static_body->set_name("CrystalBody");
		collision_shape = memnew(CollisionShape3D);
		static_body->add_child(collision_shape, false, INTERNAL_MODE_BACK);
		add_child(static_body, false, INTERNAL_MODE_BACK);
	}
	collision_shape->set_shape(shape);
}

} // namespace godot
