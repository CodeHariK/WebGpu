/**
 * @file convex_hull_rock.cpp
 * @brief ConvexHullRock: mesh hookup and collision maintenance.
 */
#include "convex_hull_rock.h"
#include <godot_cpp/classes/convex_polygon_shape3d.hpp>
#include <godot_cpp/core/class_db.hpp>

namespace godot {

void ConvexHullRock::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_rock_mesh", "mesh"), &ConvexHullRock::set_rock_mesh);
	ClassDB::bind_method(D_METHOD("get_rock_mesh"), &ConvexHullRock::get_rock_mesh);
	ADD_PROPERTY(
			PropertyInfo(Variant::OBJECT, "rock_mesh", PROPERTY_HINT_RESOURCE_TYPE, "ConvexHullRockMesh"),
			"set_rock_mesh", "get_rock_mesh"
	);
	ClassDB::bind_method(D_METHOD("set_collision_enabled", "enabled"), &ConvexHullRock::set_collision_enabled);
	ClassDB::bind_method(D_METHOD("get_collision_enabled"), &ConvexHullRock::get_collision_enabled);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "collision_enabled"), "set_collision_enabled", "get_collision_enabled");

	ClassDB::bind_method(D_METHOD("_on_rock_mesh_changed"), &ConvexHullRock::_on_rock_mesh_changed);
}

ConvexHullRock::ConvexHullRock() {}
ConvexHullRock::~ConvexHullRock() {}

/// The displayed mesh is derived from rock_mesh: show it in the inspector but never save it.
void ConvexHullRock::_validate_property(PropertyInfo &p_property) const {
	if (p_property.name == StringName("mesh")) {
		p_property.usage &= ~PROPERTY_USAGE_STORAGE;
	}
}

void ConvexHullRock::_notification(int p_what) {
	if (p_what == NOTIFICATION_READY) {
		if (rock_mesh.is_null()) {
			set_rock_mesh(memnew(ConvexHullRockMesh));
		} else {
			_apply_mesh();
		}
	}
}

void ConvexHullRock::set_rock_mesh(const Ref<ConvexHullRockMesh> &p_mesh) {
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

void ConvexHullRock::set_collision_enabled(bool p_enabled) {
	if (collision_enabled != p_enabled) {
		collision_enabled = p_enabled;
		_update_collision();
	}
}

void ConvexHullRock::_on_rock_mesh_changed() { _update_collision(); }

void ConvexHullRock::_apply_mesh() {
	set_mesh(rock_mesh);
	_update_collision();
}

// ---------------------------------------------------------------------------------------------
// Collision helpers (unowned children: rebuilt every load, never serialized)
// ---------------------------------------------------------------------------------------------

void ConvexHullRock::_ensure_collision_nodes() {
	if (!static_body) {
		static_body = Object::cast_to<StaticBody3D>(get_node_or_null("StaticBody3D"));
		if (!static_body) {
			static_body = memnew(StaticBody3D);
			static_body->set_name("StaticBody3D");
			add_child(static_body, false, INTERNAL_MODE_BACK);
		}
	}
	if (!collision_shape) {
		collision_shape = Object::cast_to<CollisionShape3D>(static_body->get_node_or_null("CollisionShape3D"));
		if (!collision_shape) {
			collision_shape = memnew(CollisionShape3D);
			collision_shape->set_name("CollisionShape3D");
			static_body->add_child(collision_shape, false, INTERNAL_MODE_BACK);
		}
	}
}

void ConvexHullRock::_free_collision_nodes() {
	if (static_body) {
		static_body->queue_free();
	}
	static_body = nullptr;
	collision_shape = nullptr;
}

void ConvexHullRock::_update_collision() {
	if (!is_inside_tree()) {
		return;
	}
	if (!collision_enabled || rock_mesh.is_null() || rock_mesh->get_surface_count() == 0) {
		_free_collision_nodes();
		return;
	}
	_ensure_collision_nodes();
	collision_shape->set_shape(rock_mesh->create_convex_shape(/*clean=*/true, /*simplify=*/false));
}

} // namespace godot
