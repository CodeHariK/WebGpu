/**
 * @file rock.cpp
 * @brief Rock: mesh hookup, matte prop material, per-blob convex colliders.
 */
#include "rock.h"
#include "prop_material.h"
#include <godot_cpp/classes/convex_polygon_shape3d.hpp>
#include <godot_cpp/core/class_db.hpp>

namespace godot {

void Rock::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_rock_mesh", "mesh"), &Rock::set_rock_mesh);
	ClassDB::bind_method(D_METHOD("get_rock_mesh"), &Rock::get_rock_mesh);
	ADD_PROPERTY(
			PropertyInfo(Variant::OBJECT, "rock_mesh", PROPERTY_HINT_RESOURCE_TYPE, "RockMesh"), "set_rock_mesh",
			"get_rock_mesh"
	);
	ClassDB::bind_method(D_METHOD("set_custom_mesh", "mesh"), &Rock::set_custom_mesh);
	ClassDB::bind_method(D_METHOD("get_custom_mesh"), &Rock::get_custom_mesh);
	ADD_PROPERTY(
			PropertyInfo(Variant::OBJECT, "custom_mesh", PROPERTY_HINT_RESOURCE_TYPE, "Mesh"), "set_custom_mesh",
			"get_custom_mesh"
	);
	ClassDB::bind_method(D_METHOD("set_material", "material"), &Rock::set_material);
	ClassDB::bind_method(D_METHOD("get_material"), &Rock::get_material);
	ADD_PROPERTY(
			PropertyInfo(Variant::OBJECT, "material", PROPERTY_HINT_RESOURCE_TYPE, "Material"), "set_material",
			"get_material"
	);
	ClassDB::bind_method(D_METHOD("set_collision_enabled", "enabled"), &Rock::set_collision_enabled);
	ClassDB::bind_method(D_METHOD("get_collision_enabled"), &Rock::get_collision_enabled);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "collision_enabled"), "set_collision_enabled", "get_collision_enabled");
	ClassDB::bind_method(D_METHOD("set_seed_from_position", "enabled"), &Rock::set_seed_from_position);
	ClassDB::bind_method(D_METHOD("get_seed_from_position"), &Rock::get_seed_from_position);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "seed_from_position"), "set_seed_from_position", "get_seed_from_position");
	ClassDB::bind_method(D_METHOD("_on_rock_mesh_changed"), &Rock::_on_rock_mesh_changed);
}

Rock::Rock() {}
Rock::~Rock() {}

void Rock::_validate_property(PropertyInfo &p_property) const {
	if (p_property.name == StringName("mesh") || p_property.name == StringName("material_override")) {
		p_property.usage &= ~PROPERTY_USAGE_STORAGE;
	}
}

void Rock::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY:
			if (rock_mesh.is_null() && custom_mesh.is_null()) {
				set_rock_mesh(memnew(RockMesh));
			} else {
				_apply_mesh();
			}
			set_notify_transform(seed_from_position);
			_reseed_from_position();
			break;
		case NOTIFICATION_TRANSFORM_CHANGED:
			_reseed_from_position();
			break;
		default:
			break;
	}
}

/**
 * @brief With seed_from_position, the mesh becomes this node's own copy (so siblings sharing the
 * resource are untouched) and its seed is a hash of the world position, quantized to 0.5 m so tiny
 * nudges don't reroll the rock. Only rebuilds when the seed actually changes.
 */
void Rock::_reseed_from_position() {
	if (!seed_from_position || rock_mesh.is_null() || !is_inside_tree()) {
		return;
	}
	const Vector3 p = get_global_position();
	const int32_t qx = (int32_t)Math::floor(p.x * 2.0f), qy = (int32_t)Math::floor(p.y * 2.0f),
				  qz = (int32_t)Math::floor(p.z * 2.0f);
	const int seed = (int)((uint32_t)qx * 73856093u ^ (uint32_t)qy * 19349663u ^ (uint32_t)qz * 83492791u) & 0x7fffffff;
	if (seed == _applied_seed) {
		return;
	}
	_applied_seed = seed;
	if (rock_mesh->get_reference_count() > 1) { // Shared with another node or the scene: take a private copy
		set_rock_mesh(rock_mesh->duplicate());
	}
	rock_mesh->set_seed(seed);
}

void Rock::set_seed_from_position(bool p_enabled) {
	seed_from_position = p_enabled;
	_applied_seed = 0x7fffffff;
	set_notify_transform(p_enabled);
	_reseed_from_position();
}

void Rock::set_rock_mesh(const Ref<RockMesh> &p_mesh) {
	const Callable cb(this, "_on_rock_mesh_changed");
	if (rock_mesh.is_valid() && rock_mesh->is_connected("changed", cb)) {
		rock_mesh->disconnect("changed", cb);
	}
	rock_mesh = p_mesh;
	if (rock_mesh.is_valid()) {
		rock_mesh->connect("changed", cb);
	}
	_apply_mesh();
}

void Rock::set_custom_mesh(const Ref<Mesh> &p_mesh) {
	custom_mesh = p_mesh;
	_apply_mesh();
}

void Rock::set_material(const Ref<Material> &p_material) {
	material = p_material;
	_apply_material();
}

void Rock::set_collision_enabled(bool p_enabled) {
	if (collision_enabled != p_enabled) {
		collision_enabled = p_enabled;
		_update_collision();
	}
}

void Rock::_on_rock_mesh_changed() { _update_collision(); }

void Rock::_apply_mesh() {
	set_mesh(custom_mesh.is_valid() ? custom_mesh : Ref<Mesh>(rock_mesh));
	_apply_material();
	_update_collision();
}

/// Matte stone: the prop shader with a whisper of specular and no transmission.
void Rock::_apply_material() {
	if (material.is_valid()) {
		set_material_override(material);
		return;
	}
	if (_prop_material.is_null()) {
		_prop_material = make_stone_material();
	}
	set_material_override(_prop_material);
}

// ---------------------------------------------------------------------------------------------
// Collision: one convex shape per blob (internal children, never saved)
// ---------------------------------------------------------------------------------------------

void Rock::_update_collision() {
	std::vector<Ref<Shape3D>> shapes;
	if (collision_enabled) {
		if (custom_mesh.is_valid()) {
			shapes.push_back(custom_mesh->create_convex_shape());
		} else if (rock_mesh.is_valid()) {
			for (int i = 0; i < rock_mesh->get_collision_shape_count(); ++i) {
				Ref<Shape3D> s = rock_mesh->create_collision_shape(i);
				if (s.is_valid()) {
					shapes.push_back(s);
				}
			}
		}
	}
	if (shapes.empty()) {
		if (static_body) {
			static_body->queue_free();
			static_body = nullptr;
			_shapes.clear();
		}
		return;
	}
	if (!static_body) {
		static_body = memnew(StaticBody3D);
		static_body->set_name("RockBody");
		add_child(static_body, false, INTERNAL_MODE_BACK);
	}
	while (_shapes.size() < shapes.size()) {
		CollisionShape3D *cs = memnew(CollisionShape3D);
		static_body->add_child(cs, false, INTERNAL_MODE_BACK);
		_shapes.push_back(cs);
	}
	while (_shapes.size() > shapes.size()) {
		_shapes.back()->queue_free();
		_shapes.pop_back();
	}
	for (size_t i = 0; i < shapes.size(); ++i) {
		_shapes[i]->set_shape(shapes[i]);
	}
}

} // namespace godot
