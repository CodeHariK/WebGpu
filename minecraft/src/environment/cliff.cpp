/**
 * @file cliff.cpp
 * @brief Cliff: mesh binding, stone material override, and static body collision sync.
 */
#include "cliff.h"
#include "prop_material.h"
#include <godot_cpp/classes/concave_polygon_shape3d.hpp>
#include <godot_cpp/classes/convex_polygon_shape3d.hpp>
#include <godot_cpp/core/class_db.hpp>

namespace godot {

void Cliff::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_cliff_mesh", "mesh"), &Cliff::set_cliff_mesh);
	ClassDB::bind_method(D_METHOD("get_cliff_mesh"), &Cliff::get_cliff_mesh);
	ADD_PROPERTY(
			PropertyInfo(Variant::OBJECT, "cliff_mesh", PROPERTY_HINT_RESOURCE_TYPE, "CliffMesh"), "set_cliff_mesh",
			"get_cliff_mesh"
	);

	ClassDB::bind_method(D_METHOD("set_custom_mesh", "mesh"), &Cliff::set_custom_mesh);
	ClassDB::bind_method(D_METHOD("get_custom_mesh"), &Cliff::get_custom_mesh);
	ADD_PROPERTY(
			PropertyInfo(Variant::OBJECT, "custom_mesh", PROPERTY_HINT_RESOURCE_TYPE, "Mesh"), "set_custom_mesh",
			"get_custom_mesh"
	);

	ClassDB::bind_method(D_METHOD("set_material", "material"), &Cliff::set_material);
	ClassDB::bind_method(D_METHOD("get_material"), &Cliff::get_material);
	ADD_PROPERTY(
			PropertyInfo(Variant::OBJECT, "material", PROPERTY_HINT_RESOURCE_TYPE, "Material"), "set_material",
			"get_material"
	);

	ClassDB::bind_method(D_METHOD("set_collision_enabled", "enabled"), &Cliff::set_collision_enabled);
	ClassDB::bind_method(D_METHOD("get_collision_enabled"), &Cliff::get_collision_enabled);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "collision_enabled"), "set_collision_enabled", "get_collision_enabled");

	ClassDB::bind_method(D_METHOD("set_seed_from_position", "enabled"), &Cliff::set_seed_from_position);
	ClassDB::bind_method(D_METHOD("get_seed_from_position"), &Cliff::get_seed_from_position);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "seed_from_position"), "set_seed_from_position", "get_seed_from_position");

	ClassDB::bind_method(D_METHOD("_on_cliff_mesh_changed"), &Cliff::_on_cliff_mesh_changed);
}

Cliff::Cliff() {}
Cliff::~Cliff() {}

void Cliff::_validate_property(PropertyInfo &p_property) const {
	if (p_property.name == StringName("mesh") || p_property.name == StringName("material_override")) {
		p_property.usage &= ~PROPERTY_USAGE_STORAGE;
	}
}

void Cliff::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY:
			if (cliff_mesh.is_null() && custom_mesh.is_null()) {
				set_cliff_mesh(memnew(CliffMesh));
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

void Cliff::_reseed_from_position() {
	if (!seed_from_position || cliff_mesh.is_null() || !is_inside_tree()) {
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
	if (cliff_mesh->get_reference_count() > 1) {
		set_cliff_mesh(cliff_mesh->duplicate());
	}
	cliff_mesh->set_seed(seed);
}

void Cliff::set_seed_from_position(bool p_enabled) {
	seed_from_position = p_enabled;
	_applied_seed = 0x7fffffff;
	set_notify_transform(p_enabled);
	_reseed_from_position();
}

void Cliff::set_cliff_mesh(const Ref<CliffMesh> &p_mesh) {
	const Callable cb(this, "_on_cliff_mesh_changed");
	if (cliff_mesh.is_valid() && cliff_mesh->is_connected("changed", cb)) {
		cliff_mesh->disconnect("changed", cb);
	}
	cliff_mesh = p_mesh;
	if (cliff_mesh.is_valid()) {
		cliff_mesh->connect("changed", cb);
	}
	_apply_mesh();
}

void Cliff::set_custom_mesh(const Ref<Mesh> &p_mesh) {
	custom_mesh = p_mesh;
	_apply_mesh();
}

void Cliff::set_material(const Ref<Material> &p_material) {
	material = p_material;
	_apply_material();
}

void Cliff::set_collision_enabled(bool p_enabled) {
	if (collision_enabled != p_enabled) {
		collision_enabled = p_enabled;
		_update_collision();
	}
}

void Cliff::_on_cliff_mesh_changed() {
	_update_collision();
}

void Cliff::_apply_mesh() {
	set_mesh(custom_mesh.is_valid() ? custom_mesh : Ref<Mesh>(cliff_mesh));
	_apply_material();
	_update_collision();
}

void Cliff::_apply_material() {
	if (material.is_valid()) {
		set_material_override(material);
		return;
	}
	if (_prop_material.is_null()) {
		_prop_material = make_stone_material();
	}
	set_material_override(_prop_material);
}

void Cliff::_update_collision() {
	Ref<Shape3D> shape;
	if (collision_enabled) {
		if (custom_mesh.is_valid()) {
			shape = custom_mesh->create_convex_shape();
		} else if (cliff_mesh.is_valid()) {
			shape = cliff_mesh->create_collision_shape();
		}
	}

	if (shape.is_null()) {
		if (static_body) {
			static_body->queue_free();
			static_body = nullptr;
			collision_shape = nullptr;
		}
		return;
	}

	if (!static_body) {
		static_body = memnew(StaticBody3D);
		static_body->set_name("CliffBody");
		add_child(static_body, false, INTERNAL_MODE_BACK);
		collision_shape = memnew(CollisionShape3D);
		static_body->add_child(collision_shape, false, INTERNAL_MODE_BACK);
	}

	if (collision_shape) {
		collision_shape->set_shape(shape);
	}
}

} // namespace godot
