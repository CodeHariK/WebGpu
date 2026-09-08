/**
 * @file tree.cpp
 * @brief FoliageTree: mesh hookup, prop materials, leaf/fruit MultiMeshes, trunk collider.
 */
#include "tree.h"
#include "prop_material.h"
#include <godot_cpp/classes/cylinder_shape3d.hpp>
#include <godot_cpp/classes/multi_mesh.hpp>
#include <godot_cpp/classes/quad_mesh.hpp>
#include <godot_cpp/classes/sphere_mesh.hpp>
#include <godot_cpp/core/class_db.hpp>

namespace godot {

void FoliageTree::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_tree_mesh", "mesh"), &FoliageTree::set_tree_mesh);
	ClassDB::bind_method(D_METHOD("get_tree_mesh"), &FoliageTree::get_tree_mesh);
	ADD_PROPERTY(
			PropertyInfo(Variant::OBJECT, "tree_mesh", PROPERTY_HINT_RESOURCE_TYPE, "FoliageTreeMesh"), "set_tree_mesh",
			"get_tree_mesh"
	);
	ClassDB::bind_method(D_METHOD("set_leaf_mesh", "mesh"), &FoliageTree::set_leaf_mesh);
	ClassDB::bind_method(D_METHOD("get_leaf_mesh"), &FoliageTree::get_leaf_mesh);
	ADD_PROPERTY(
			PropertyInfo(Variant::OBJECT, "leaf_mesh", PROPERTY_HINT_RESOURCE_TYPE, "Mesh"), "set_leaf_mesh",
			"get_leaf_mesh"
	);
	ClassDB::bind_method(D_METHOD("set_fruit_mesh", "mesh"), &FoliageTree::set_fruit_mesh);
	ClassDB::bind_method(D_METHOD("get_fruit_mesh"), &FoliageTree::get_fruit_mesh);
	ADD_PROPERTY(
			PropertyInfo(Variant::OBJECT, "fruit_mesh", PROPERTY_HINT_RESOURCE_TYPE, "Mesh"), "set_fruit_mesh",
			"get_fruit_mesh"
	);
	ClassDB::bind_method(D_METHOD("set_trunk_material", "material"), &FoliageTree::set_trunk_material);
	ClassDB::bind_method(D_METHOD("get_trunk_material"), &FoliageTree::get_trunk_material);
	ADD_PROPERTY(
			PropertyInfo(Variant::OBJECT, "trunk_material", PROPERTY_HINT_RESOURCE_TYPE, "Material"),
			"set_trunk_material", "get_trunk_material"
	);
	ClassDB::bind_method(D_METHOD("set_foliage_material", "material"), &FoliageTree::set_foliage_material);
	ClassDB::bind_method(D_METHOD("get_foliage_material"), &FoliageTree::get_foliage_material);
	ADD_PROPERTY(
			PropertyInfo(Variant::OBJECT, "foliage_material", PROPERTY_HINT_RESOURCE_TYPE, "Material"),
			"set_foliage_material", "get_foliage_material"
	);
	ClassDB::bind_method(D_METHOD("set_glow", "value"), &FoliageTree::set_glow);
	ClassDB::bind_method(D_METHOD("get_glow"), &FoliageTree::get_glow);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "glow", PROPERTY_HINT_RANGE, "0,4,0.05"), "set_glow", "get_glow");
	ClassDB::bind_method(D_METHOD("set_transmission", "value"), &FoliageTree::set_transmission);
	ClassDB::bind_method(D_METHOD("get_transmission"), &FoliageTree::get_transmission);
	ADD_PROPERTY(
			PropertyInfo(Variant::FLOAT, "transmission", PROPERTY_HINT_RANGE, "0,2,0.05"), "set_transmission",
			"get_transmission"
	);
	ClassDB::bind_method(D_METHOD("set_collision_enabled", "enabled"), &FoliageTree::set_collision_enabled);
	ClassDB::bind_method(D_METHOD("get_collision_enabled"), &FoliageTree::get_collision_enabled);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "collision_enabled"), "set_collision_enabled", "get_collision_enabled");
	ClassDB::bind_method(D_METHOD("set_seed_from_position", "enabled"), &FoliageTree::set_seed_from_position);
	ClassDB::bind_method(D_METHOD("get_seed_from_position"), &FoliageTree::get_seed_from_position);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "seed_from_position"), "set_seed_from_position", "get_seed_from_position");
	ClassDB::bind_method(D_METHOD("_on_tree_mesh_changed"), &FoliageTree::_on_tree_mesh_changed);
}

FoliageTree::FoliageTree() {}
FoliageTree::~FoliageTree() {}

void FoliageTree::_validate_property(PropertyInfo &p_property) const {
	if (p_property.name == StringName("mesh") || p_property.name == StringName("material_override")) {
		p_property.usage &= ~PROPERTY_USAGE_STORAGE;
	}
}

void FoliageTree::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY:
			if (tree_mesh.is_null()) {
				set_tree_mesh(memnew(FoliageTreeMesh));
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

/// See Rock::_reseed_from_position — same rule: private mesh copy, seed from the 0.5 m-quantized position.
void FoliageTree::_reseed_from_position() {
	if (!seed_from_position || tree_mesh.is_null() || !is_inside_tree()) {
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
	if (tree_mesh->get_reference_count() > 1) {
		set_tree_mesh(tree_mesh->duplicate());
	}
	tree_mesh->set_seed(seed);
}

void FoliageTree::set_seed_from_position(bool p_enabled) {
	seed_from_position = p_enabled;
	_applied_seed = 0x7fffffff;
	set_notify_transform(p_enabled);
	_reseed_from_position();
}

void FoliageTree::set_tree_mesh(const Ref<FoliageTreeMesh> &p_mesh) {
	const Callable cb(this, "_on_tree_mesh_changed");
	if (tree_mesh.is_valid() && tree_mesh->is_connected("changed", cb)) {
		tree_mesh->disconnect("changed", cb);
	}
	tree_mesh = p_mesh;
	if (tree_mesh.is_valid()) {
		tree_mesh->connect("changed", cb);
	}
	_apply_mesh();
}

void FoliageTree::set_leaf_mesh(const Ref<Mesh> &p_mesh) {
	leaf_mesh = p_mesh;
	_rebuild_instances();
}
void FoliageTree::set_fruit_mesh(const Ref<Mesh> &p_mesh) {
	fruit_mesh = p_mesh;
	_rebuild_instances();
}
void FoliageTree::set_trunk_material(const Ref<Material> &p_material) {
	trunk_material = p_material;
	_apply_materials();
}
void FoliageTree::set_foliage_material(const Ref<Material> &p_material) {
	foliage_material = p_material;
	_apply_materials();
}
void FoliageTree::set_glow(float p_glow) {
	glow = MAX(0.0f, p_glow);
	_apply_materials();
}
void FoliageTree::set_transmission(float p_amount) {
	transmission = MAX(0.0f, p_amount);
	_apply_materials();
}
void FoliageTree::set_collision_enabled(bool p_enabled) {
	if (collision_enabled != p_enabled) {
		collision_enabled = p_enabled;
		_update_collision();
	}
}

void FoliageTree::_on_tree_mesh_changed() {
	_rebuild_instances();
	_update_collision();
}

void FoliageTree::_apply_mesh() {
	set_mesh(tree_mesh);
	_apply_materials();
	_rebuild_instances();
	_update_collision();
}

// ---------------------------------------------------------------------------------------------
// Materials
// ---------------------------------------------------------------------------------------------

void FoliageTree::_apply_materials() {
	if (_trunk_prop.is_null()) {
		_trunk_prop = make_prop_material(false);
	}
	if (_foliage_prop.is_null()) {
		_foliage_prop = make_prop_material(true); // Leaves read from both sides
	}
	for (const Ref<ShaderMaterial> &m : { _trunk_prop, _foliage_prop }) {
		m->set_shader_parameter("glow", glow);
		m->set_shader_parameter("transmission", transmission);
	}
	set_material_override(trunk_material.is_valid() ? trunk_material : Ref<Material>(_trunk_prop));
	const Ref<Material> foliage = foliage_material.is_valid() ? foliage_material : Ref<Material>(_foliage_prop);
	if (_leaves) {
		_leaves->set_material_override(foliage);
	}
	if (_fruit) {
		_fruit->set_material_override(trunk_material.is_valid() ? trunk_material : Ref<Material>(_trunk_prop));
	}
}

// ---------------------------------------------------------------------------------------------
// Instances (internal children, never saved)
// ---------------------------------------------------------------------------------------------

Ref<Mesh> FoliageTree::_placeholder_leaf() {
	Ref<QuadMesh> q;
	q.instantiate();
	q->set_size(Vector2(1.0f, 1.0f));
	return q;
}

Ref<Mesh> FoliageTree::_placeholder_fruit() {
	Ref<SphereMesh> s;
	s.instantiate();
	s->set_radius(1.0f);
	s->set_height(2.0f);
	s->set_radial_segments(6);
	s->set_rings(4);
	return s;
}

MultiMeshInstance3D *FoliageTree::_ensure_mm(
		MultiMeshInstance3D *&p_slot,
		const char *p_name
) {
	if (!p_slot) {
		p_slot = memnew(MultiMeshInstance3D);
		p_slot->set_name(p_name);
		add_child(p_slot, false, INTERNAL_MODE_BACK);
	}
	return p_slot;
}

void FoliageTree::_rebuild_instances() {
	if (tree_mesh.is_null()) {
		return;
	}
	FoliageTreeMesh::Scatter leaves, fruit;
	tree_mesh->compute_leaves(leaves);
	tree_mesh->compute_fruit(fruit);

	auto fill = [&](MultiMeshInstance3D *mmi, const Ref<Mesh> &mesh, const FoliageTreeMesh::Scatter &sc) {
		Ref<MultiMesh> mm = mmi->get_multimesh();
		if (mm.is_null()) {
			mm.instantiate();
			mmi->set_multimesh(mm);
		}
		mm->set_instance_count(0); // Required before changing format / colours on an existing MultiMesh
		mm->set_transform_format(MultiMesh::TRANSFORM_3D);
		mm->set_use_colors(true);
		mm->set_mesh(mesh);
		mm->set_instance_count((int)sc.xforms.size());
		for (int i = 0; i < (int)sc.xforms.size(); ++i) {
			mm->set_instance_transform(i, sc.xforms[i]);
			mm->set_instance_color(i, i < sc.colors.size() ? sc.colors[i] : Color(1, 1, 1));
		}
	};

	MultiMeshInstance3D *lm = _ensure_mm(_leaves, "Leaves");
	fill(lm, leaf_mesh.is_valid() ? leaf_mesh : _placeholder_leaf(), leaves);
	MultiMeshInstance3D *fm = _ensure_mm(_fruit, "Fruit");
	fill(fm, fruit_mesh.is_valid() ? fruit_mesh : _placeholder_fruit(), fruit);
	_apply_materials();
}

// ---------------------------------------------------------------------------------------------
// Collision
// ---------------------------------------------------------------------------------------------

void FoliageTree::_update_collision() {
	if (!collision_enabled || tree_mesh.is_null()) {
		if (static_body) {
			static_body->queue_free();
			static_body = nullptr;
			collision_shape = nullptr;
		}
		return;
	}
	Ref<Shape3D> shape = tree_mesh->create_collision_shape();
	if (shape.is_null()) {
		return;
	}
	if (!static_body) {
		static_body = memnew(StaticBody3D);
		static_body->set_name("TrunkBody");
		collision_shape = memnew(CollisionShape3D);
		static_body->add_child(collision_shape, false, INTERNAL_MODE_BACK);
		add_child(static_body, false, INTERNAL_MODE_BACK);
	}
	collision_shape->set_shape(shape);
	collision_shape->set_position(Vector3(0, tree_mesh->get_trunk_height() * 0.5f, 0));
}

} // namespace godot
