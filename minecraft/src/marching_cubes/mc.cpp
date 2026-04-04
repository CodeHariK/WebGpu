#include "marching_cubes/mc.h"
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/packed_scene.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/label3d.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/classes/engine.hpp>

using namespace godot;

void MCNode::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_mesh_library_path", "p_path"), &MCNode::set_mesh_library_path);
	ClassDB::bind_method(D_METHOD("get_mesh_library_path"), &MCNode::get_mesh_library_path);
	
	ClassDB::bind_method(D_METHOD("set_regenerate_mesh", "p_regenerate"), &MCNode::set_regenerate_mesh);
	ClassDB::bind_method(D_METHOD("get_regenerate_mesh"), &MCNode::get_regenerate_mesh);

	ClassDB::bind_method(D_METHOD("load_mesh_library"), &MCNode::load_mesh_library);
	ClassDB::bind_method(D_METHOD("display_library"), &MCNode::display_library);
	ClassDB::bind_method(D_METHOD("generate_corner0_variants"), &MCNode::generate_corner0_variants);

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "mesh_library_path"), "set_mesh_library_path", "get_mesh_library_path");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "regenerate_mesh"), "set_regenerate_mesh", "get_regenerate_mesh");
}

MCNode::MCNode() {
}

MCNode::~MCNode() {
}

void MCNode::_ready() {
	load_mesh_library();
	generate_corner0_variants();
	display_library();
}

void MCNode::set_mesh_library_path(const String &p_path) {
	mesh_library_path = p_path;
}

String MCNode::get_mesh_library_path() const {
	return mesh_library_path;
}

void MCNode::set_regenerate_mesh(bool p_regenerate) {
	if (p_regenerate) {
		UtilityFunctions::print("MCNode: Regenerating meshes...");
		load_mesh_library();
		generate_corner0_variants();
		display_library();
	}
	// Flag is always false in inspector after click
	regenerate_mesh = false;
	notify_property_list_changed();
}

bool MCNode::get_regenerate_mesh() const {
	return regenerate_mesh;
}

void MCNode::load_mesh_library() {
	mesh_library.clear();

	Ref<PackedScene> scene = ResourceLoader::get_singleton()->load(mesh_library_path);
	if (scene.is_null()) {
		UtilityFunctions::print("MCNode: Failed to load mesh library at ", mesh_library_path);
		return;
	}

	Node *inst = scene->instantiate();
	if (!inst) {
		return;
	}

	TypedArray<Node> children = inst->get_children();
	for (int i = 0; i < children.size(); i++) {
		Node *child = Object::cast_to<Node>(children[i]);
		MeshInstance3D *mi = Object::cast_to<MeshInstance3D>(child);
		
		if (!mi) {
			for (int j = 0; j < child->get_child_count(); j++) {
				mi = Object::cast_to<MeshInstance3D>(child->get_child(j));
				if (mi) break;
			}
		}

		if (mi) {
			Ref<Mesh> mesh = mi->get_mesh();
			if (mesh.is_valid()) {
				MeshConfig config;
				config.mesh = mesh;
				config.transform = Transform3D(); // Identity for original GLB nodes
				mesh_library[child->get_name()] = config;
			}
		}
	}

	inst->queue_free();
}

void MCNode::generate_corner0_variants() {
	uint8_t base_h = 0b10000000;
	if (!mesh_library.has("10000000")) {
		return;
	}
	
	MeshConfig base_conf = mesh_library["10000000"];
	Transform3D base_t = base_conf.transform;

	// Defined sequences from user request
	apply_transform_sequence(base_h, base_t, { RY90 }, "Ry90");
	apply_transform_sequence(base_h, base_t, { RY180 }, "Ry180");
	apply_transform_sequence(base_h, base_t, { RY270 }, "Ry270");
	apply_transform_sequence(base_h, base_t, { RX90 }, "Rx90");
	apply_transform_sequence(base_h, base_t, { RX90, RY90 }, "Rx90 Ry90");
	apply_transform_sequence(base_h, base_t, { RX90, RY180 }, "Rx90 Ry180");
	apply_transform_sequence(base_h, base_t, { RX90, RY270 }, "Rx90 Ry270");
}

void MCNode::apply_transform_sequence(uint8_t p_base_hash, const Transform3D &p_base_t, const std::vector<MCTransform> &p_sequence, const String &p_name) {
	uint8_t h = p_base_hash;
	Transform3D t = p_base_t;

	float p90 = Math_PI / 2.0;
	float m90 = -Math_PI / 2.0;

	for (MCTransform op : p_sequence) {
		switch (op) {
			case RX90: h = rotate_x(h); t = t.rotated(Vector3(1, 0, 0), p90); break;
			case RX180: h = rotate_x(rotate_x(h)); t = t.rotated(Vector3(1, 0, 0), Math_PI); break;
			case RX270: h = rotate_x(rotate_x(rotate_x(h))); t = t.rotated(Vector3(1, 0, 0), m90); break;
			
			case RY90: h = rotate_y(h); t = t.rotated(Vector3(0, 1, 0), m90); break;
			case RY180: h = rotate_y(rotate_y(h)); t = t.rotated(Vector3(0, 1, 0), Math_PI); break;
			case RY270: h = rotate_y(rotate_y(rotate_y(h))); t = t.rotated(Vector3(0, 1, 0), p90); break;
			
			case RZ90: h = rotate_z(h); t = t.rotated(Vector3(0, 0, 1), p90); break;
			case RZ180: h = rotate_z(rotate_z(h)); t = t.rotated(Vector3(0, 0, 1), Math_PI); break;
			case RZ270: h = rotate_z(rotate_z(rotate_z(h))); t = t.rotated(Vector3(0, 0, 1), m90); break;
			
			case SX: h = scale_x(h); t = t.scaled_local(Vector3(-1, 1, 1)); break;
		}
	}

	String h_str = hash_to_binary(h);
	if (!mesh_library.has(h_str)) {
		MeshConfig config;
		config.mesh = mesh_library[hash_to_binary(p_base_hash)].mesh;
		config.transform = t;
		mesh_library[h_str] = config;
		UtilityFunctions::print("Generated Variant ", p_name, ": ", h_str);
	}
}

void MCNode::display_library() {
	for (int i = get_child_count() - 1; i >= 0; i--) {
		Node *child = get_child(i);
		child->queue_free();
		remove_child(child);
	}

	int i = 0;
	int grid_size = 5;
	float spacing = 3.0f;

	for (const KeyValue<String, MeshConfig> &E : mesh_library) {
		MeshInstance3D *mi = memnew(MeshInstance3D);
		mi->set_mesh(E.value.mesh);
		mi->set_name(E.key);
		
		int x = i % grid_size;
		int z = i / grid_size;
		
		// Position in grid + the internal rotation for that variant
		Transform3D grid_t;
		grid_t.origin = Vector3(x * spacing, 0, z * spacing);
		mi->set_transform(grid_t * E.value.transform);
		
		add_child(mi);
		if (Engine::get_singleton()->is_editor_hint()) {
			mi->set_owner(get_owner());
		}

		Label3D *label = memnew(Label3D);
		label->set_text(E.key);
		label->set_position(grid_t.origin + Vector3(0, 1.5f, 0));
		label->set_billboard_mode(BaseMaterial3D::BILLBOARD_ENABLED);
		add_child(label);
		if (Engine::get_singleton()->is_editor_hint()) {
			label->set_owner(get_owner());
		}

		i++;
	}
}

uint8_t MCNode::rotate_y(uint8_t p_hash) {
	uint8_t next = 0;
	if (is_c0(p_hash)) next |= (1 << 6);
	if (is_c1(p_hash)) next |= (1 << 5);
	if (is_c2(p_hash)) next |= (1 << 4);
	if (is_c3(p_hash)) next |= (1 << 7);
	if (is_c4(p_hash)) next |= (1 << 2);
	if (is_c5(p_hash)) next |= (1 << 1);
	if (is_c6(p_hash)) next |= (1 << 0);
	if (is_c7(p_hash)) next |= (1 << 3);
	return next;
}

uint8_t MCNode::rotate_x(uint8_t p_hash) {
	uint8_t next = 0;
	if (is_c0(p_hash)) next |= (1 << 3);
	if (is_c3(p_hash)) next |= (1 << 7);
	if (is_c4(p_hash)) next |= (1 << 0);
	if (is_c7(p_hash)) next |= (1 << 4);
	if (is_c1(p_hash)) next |= (1 << 2);
	if (is_c2(p_hash)) next |= (1 << 6);
	if (is_c5(p_hash)) next |= (1 << 1);
	if (is_c6(p_hash)) next |= (1 << 5);
	return next;
}

uint8_t MCNode::rotate_z(uint8_t p_hash) {
	uint8_t next = 0;
	if (is_c0(p_hash)) next |= (1 << 6);
	if (is_c1(p_hash)) next |= (1 << 2);
	if (is_c2(p_hash)) next |= (1 << 3);
	if (is_c3(p_hash)) next |= (1 << 7);
	if (is_c4(p_hash)) next |= (1 << 5);
	if (is_c5(p_hash)) next |= (1 << 1);
	if (is_c6(p_hash)) next |= (1 << 0);
	if (is_c7(p_hash)) next |= (1 << 4);
	return next;
}

uint8_t MCNode::scale_x(uint8_t p_hash) {
	uint8_t next = 0;
	if (is_c0(p_hash)) next |= (1 << 6);
	if (is_c1(p_hash)) next |= (1 << 7);
	if (is_c3(p_hash)) next |= (1 << 5);
	if (is_c2(p_hash)) next |= (1 << 4);
	if (is_c4(p_hash)) next |= (1 << 2);
	if (is_c5(p_hash)) next |= (1 << 3);
	if (is_c7(p_hash)) next |= (1 << 1);
	if (is_c6(p_hash)) next |= (1 << 0);
	return next;
}

String MCNode::hash_to_binary(uint8_t p_hash) {
	String s = "";
	for (int i = 7; i >= 0; i--) {
		s += (p_hash & (1 << i)) ? "1" : "0";
	}
	return s;
}
