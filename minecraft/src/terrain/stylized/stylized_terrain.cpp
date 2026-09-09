/**
 * @file stylized_terrain.cpp
 * @brief StylizedTerrain: mesh/material application, trimesh collision, editor rebuild wiring.
 */
#include "stylized_terrain.h"
#include "terrain/terraspline/tr_toon.h"
#include <godot_cpp/classes/concave_polygon_shape3d.hpp>
#include <godot_cpp/core/class_db.hpp>

namespace godot {

void StylizedTerrain::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_terrain_mesh", "mesh"), &StylizedTerrain::set_terrain_mesh);
	ClassDB::bind_method(D_METHOD("get_terrain_mesh"), &StylizedTerrain::get_terrain_mesh);
	ADD_PROPERTY(
			PropertyInfo(Variant::OBJECT, "terrain_mesh", PROPERTY_HINT_RESOURCE_TYPE, "StylizedTerrainMesh"),
			"set_terrain_mesh", "get_terrain_mesh"
	);
	ClassDB::bind_method(D_METHOD("set_material", "material"), &StylizedTerrain::set_material);
	ClassDB::bind_method(D_METHOD("get_material"), &StylizedTerrain::get_material);
	ADD_PROPERTY(
			PropertyInfo(Variant::OBJECT, "material", PROPERTY_HINT_RESOURCE_TYPE, "Material"), "set_material",
			"get_material"
	);
	ClassDB::bind_method(D_METHOD("set_collision_enabled", "enabled"), &StylizedTerrain::set_collision_enabled);
	ClassDB::bind_method(D_METHOD("get_collision_enabled"), &StylizedTerrain::get_collision_enabled);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "collision_enabled"), "set_collision_enabled", "get_collision_enabled");
	ClassDB::bind_method(D_METHOD("_on_terrain_mesh_changed"), &StylizedTerrain::_on_terrain_mesh_changed);
}

StylizedTerrain::StylizedTerrain() {}
StylizedTerrain::~StylizedTerrain() {}

void StylizedTerrain::_validate_property(PropertyInfo &p_property) const {
	// The displayed mesh and the resolved material are generated; never store them on the node.
	if (p_property.name == StringName("mesh") || p_property.name == StringName("material_override")) {
		p_property.usage &= ~PROPERTY_USAGE_STORAGE;
	}
}

void StylizedTerrain::_notification(int p_what) {
	if (p_what == NOTIFICATION_READY) {
		if (terrain_mesh.is_null()) {
			terrain_mesh.instantiate();
		}
		_apply_mesh();
		_apply_material();
	}
}

void StylizedTerrain::_apply_mesh() {
	if (terrain_mesh.is_valid() && !terrain_mesh->is_connected("changed", Callable(this, "_on_terrain_mesh_changed"))) {
		terrain_mesh->connect("changed", Callable(this, "_on_terrain_mesh_changed"));
	}
	set_mesh(terrain_mesh);
	_update_collision();
}

void StylizedTerrain::_apply_material() {
	if (material.is_valid()) {
		set_material_override(material);
		return;
	}
	if (_toon_material.is_null()) {
		_toon_material = make_toon_solid_material();
	}
	set_material_override(_toon_material);
}

void StylizedTerrain::_update_collision() {
	if (!collision_enabled || terrain_mesh.is_null()) {
		if (static_body) {
			static_body->queue_free();
			static_body = nullptr;
			collision_shape = nullptr;
		}
		return;
	}
	if (!static_body) {
		static_body = memnew(StaticBody3D);
		add_child(static_body, false, INTERNAL_MODE_BACK);
		collision_shape = memnew(CollisionShape3D);
		static_body->add_child(collision_shape);
	}
	collision_shape->set_shape(terrain_mesh->create_trimesh_shape());
}

void StylizedTerrain::_on_terrain_mesh_changed() {
	set_mesh(terrain_mesh); // keep the instance pointing at the (rebuilt) mesh
	_update_collision();
}

void StylizedTerrain::set_terrain_mesh(const Ref<StylizedTerrainMesh> &p_mesh) {
	if (terrain_mesh == p_mesh) {
		return;
	}
	if (terrain_mesh.is_valid() && terrain_mesh->is_connected("changed", Callable(this, "_on_terrain_mesh_changed"))) {
		terrain_mesh->disconnect("changed", Callable(this, "_on_terrain_mesh_changed"));
	}
	terrain_mesh = p_mesh;
	if (is_inside_tree()) {
		_apply_mesh();
	}
}

void StylizedTerrain::set_material(const Ref<Material> &p_material) {
	material = p_material;
	if (is_inside_tree()) {
		_apply_material();
	}
}

void StylizedTerrain::set_collision_enabled(bool p_enabled) {
	collision_enabled = p_enabled;
	if (is_inside_tree()) {
		_update_collision();
	}
}

} // namespace godot
