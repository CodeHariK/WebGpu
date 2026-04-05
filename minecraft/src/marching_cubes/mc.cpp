#include "marching_cubes/mc.h"
#include <algorithm>
#include <godot_cpp/classes/canvas_layer.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/classes/input_event_mouse_button.hpp>
#include <godot_cpp/classes/label3d.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/packed_scene.hpp>
#include <godot_cpp/classes/panel.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/templates/hash_map.hpp>
#include <godot_cpp/variant/callable.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <vector>

using namespace godot;

void MCNode::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_mesh_library_path", "p_path"), &MCNode::set_mesh_library_path);
	ClassDB::bind_method(D_METHOD("get_mesh_library_path"), &MCNode::get_mesh_library_path);

	ClassDB::bind_method(D_METHOD("set_regenerate_mesh", "p_regenerate"), &MCNode::set_regenerate_mesh);
	ClassDB::bind_method(D_METHOD("get_regenerate_mesh"), &MCNode::get_regenerate_mesh);

	ClassDB::bind_method(D_METHOD("load_mesh_library"), &MCNode::load_mesh_library);
	ClassDB::bind_method(D_METHOD("display_library"), &MCNode::display_library);
	ClassDB::bind_method(D_METHOD("generate_variants"), &MCNode::generate_variants);
	ClassDB::bind_method(D_METHOD("_on_toggle_ui"), &MCNode::_on_toggle_ui);
	ClassDB::bind_method(D_METHOD("_on_gui_input", "p_event"), &MCNode::_on_gui_input);
	ClassDB::bind_method(D_METHOD("_on_show_help"), &MCNode::_on_show_help);

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "mesh_library_path"), "set_mesh_library_path", "get_mesh_library_path");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "regenerate_mesh"), "set_regenerate_mesh", "get_regenerate_mesh");
}

MCNode::MCNode() {
}

MCNode::~MCNode() {
}

void MCNode::_ready() {
	load_mesh_library();
	generate_variants();
	// Finally update UI
	if (!Engine::get_singleton()->is_editor_hint()) {
		ui_manager = CUI::create_on_new_layer(this);
		if (ui_manager) {
			setup_ui();
			UtilityFunctions::print("MCNode: UI initialized via CUI factory.");
		}
	}
	update_ui_counts();
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
		generate_variants();
		display_library();
	}
	// Flag is always false in inspector after click
	regenerate_mesh = false;

	if (!Engine::get_singleton()->is_editor_hint()) {
		if (!ui_manager) {
			ui_manager = memnew(CUI);
			add_child(ui_manager);
			setup_ui();
		}
	}
	notify_property_list_changed();
}

bool MCNode::get_regenerate_mesh() const {
	return regenerate_mesh;
}

struct LoadMeta {
	Node *child = nullptr;
	MeshInstance3D *mi = nullptr;
	int ones_count = 0;

	LoadMeta(Node *p_child, MeshInstance3D *p_mi) :
			child(p_child), mi(p_mi) {
		if (child) {
			String p_bin = child->get_name();
			for (int i = 0; i < p_bin.length(); i++) {
				if (p_bin[i] == '1')
					ones_count++;
			}
		}
	}

	bool operator<(const LoadMeta &p_other) const {
		return ones_count < p_other.ones_count;
	}
};

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

	// 1. Collect all candidates
	std::vector<LoadMeta> candidates;
	TypedArray<Node> children = inst->get_children();
	for (int i = 0; i < children.size(); i++) {
		Node *child = Object::cast_to<Node>(children[i]);
		MeshInstance3D *mi = Object::cast_to<MeshInstance3D>(child);

		if (!mi) {
			for (int j = 0; j < child->get_child_count(); j++) {
				mi = Object::cast_to<MeshInstance3D>(child->get_child(j));
				if (mi)
					break;
			}
		}

		if (mi && mi->get_mesh().is_valid()) {
			candidates.push_back(LoadMeta(child, mi));
		}
	}

	// 2. Sort by bit-density (number of active corners)
	std::sort(candidates.begin(), candidates.end());

	// 3. Populate Library and Master Order
	base_mesh_order.clear();
	for (const LoadMeta &meta : candidates) {
		MeshConfig config;
		config.mesh = meta.mi->get_mesh();
		config.transform = Transform3D(); // Identity for original GLB nodes
		config.source_mesh = meta.child->get_name();
		mesh_library[meta.child->get_name()] = config;
		base_mesh_order.push_back(meta.child->get_name());
		UtilityFunctions::print("MCNode: Loaded mesh: ", meta.child->get_name(), " (Corners: ", meta.ones_count, ")");
	}

	UtilityFunctions::print("MCNode: Finished loading library. Total items: ", mesh_library.size());
	inst->queue_free();
}

void MCNode::generate_variants() {
	// 1 Corner
	uint8_t base_h = 0b10000000;
	if (mesh_library.has("10000000")) {
		MeshConfig base_conf = mesh_library["10000000"];
		Transform3D base_t = base_conf.transform;

		apply_8_rotations(base_h, base_t, "1Corner");
	}

	// 2 Corners
	uint8_t base_h_edge = 0b10010000;
	if (mesh_library.has("10010000")) {
		MeshConfig base_conf = mesh_library["10010000"];
		Transform3D base_t = base_conf.transform;

		apply_12_rotations(base_h_edge, base_t, "Edge");
	}
	uint8_t base_h_diag = 0b10100000;
	if (mesh_library.has("10100000")) {
		MeshConfig base_conf = mesh_library["10100000"];
		Transform3D base_t = base_conf.transform;

		apply_12_rotations(base_h_diag, base_t, "Diag");
	}
	uint8_t base_h_opp = 0b10000010;
	if (mesh_library.has("10000010")) {
		MeshConfig base_conf = mesh_library["10000010"];
		Transform3D base_t = base_conf.transform;

		apply_4_rotations(base_h_opp, base_t, "Opp");
	}

	// 3 Corners
	uint8_t base_h_v = 0b11100000;
	if (mesh_library.has("11100000")) {
		MeshConfig base_conf = mesh_library["11100000"];
		Transform3D base_t = base_conf.transform;

		apply_24_rotations(base_h_v, base_t, "V");
	}
	uint8_t base_h_l = 0b10100010;
	if (mesh_library.has("10100010")) {
		MeshConfig base_conf = mesh_library["10100010"];
		Transform3D base_t = base_conf.transform;

		apply_24_rotations(base_h_l, base_t, "L");
	}
	uint8_t base_h_tri = 0b10100001;
	if (mesh_library.has("10100001")) {
		MeshConfig base_conf = mesh_library["10100001"];
		Transform3D base_t = base_conf.transform;

		apply_8_rotations(base_h_tri, base_t, "Tri");
	}

	// 4 Corners
	uint8_t base_h_slab = 0b11110000;
	if (mesh_library.has("11110000")) {
		MeshConfig base_conf = mesh_library["11110000"];
		Transform3D base_t = base_conf.transform;

		apply_6_rotations(base_h_slab, base_t, "Slab");
	}
	uint8_t base_h_e1 = 0b11100001;
	if (mesh_library.has("11100001")) {
		MeshConfig base_conf = mesh_library["11100001"];
		Transform3D base_t = base_conf.transform;

		apply_24_rotations(base_h_e1, base_t, "E1");
	}
	uint8_t base_h_tetra = 0b10100101;
	if (mesh_library.has("10100101")) {
		MeshConfig base_conf = mesh_library["10100101"];
		Transform3D base_t = base_conf.transform;

		apply_transform_sequence(base_h_tetra, base_t, { RX90 }, "Tetra Rx90");
	}
	uint8_t base_h_e4 = 0b11100100;
	if (mesh_library.has("11100100")) {
		MeshConfig base_conf = mesh_library["11100100"];
		Transform3D base_t = base_conf.transform;

		apply_8_rotations(base_h_e4, base_t, "E4");
	}
	uint8_t base_h_e8 = 0b11101000;
	if (mesh_library.has("11101000")) {
		MeshConfig base_conf = mesh_library["11101000"];
		Transform3D base_t = base_conf.transform;

		apply_12_rotations(base_h_e8, base_t, "E8");

		// Add mirror variants
		apply_12_mirror_rotations(base_h_e8, base_t, "E8 Sx");
	}
	uint8_t base_h_opp_edges = 0b10101010;
	if (mesh_library.has("10101010")) {
		MeshConfig base_conf = mesh_library["10101010"];
		Transform3D base_t = base_conf.transform;

		apply_6_rotations(base_h_opp_edges, base_t, "OppEdges");
	}

	// 5 Corners
	uint8_t base_h_v_inv = 0b11110010;
	if (mesh_library.has("11110010")) {
		MeshConfig base_conf = mesh_library["11110010"];
		Transform3D base_t = base_conf.transform;

		apply_24_rotations(base_h_v_inv, base_t, "InvV");
	}
	uint8_t base_h_ea = 0b11101010;
	if (mesh_library.has("11101010")) {
		MeshConfig base_conf = mesh_library["11101010"];
		Transform3D base_t = base_conf.transform;

		apply_24_rotations(base_h_ea, base_t, "InvV2");
	}
	uint8_t base_h_da = 0b11011010;
	if (mesh_library.has("11011010")) {
		MeshConfig base_conf = mesh_library["11011010"];
		Transform3D base_t = base_conf.transform;

		apply_8_rotations(base_h_da, base_t, "InvTri");
	}

	// 6 Corners
	uint8_t base_h_edge_inv = 0b11110110;
	if (mesh_library.has("11110110")) {
		MeshConfig base_conf = mesh_library["11110110"];
		Transform3D base_t = base_conf.transform;

		apply_12_rotations(base_h_edge_inv, base_t, "InvEdge");
	}
	uint8_t base_h_inv_diag = 0b11111010;
	if (mesh_library.has("11111010")) {
		MeshConfig base_conf = mesh_library["11111010"];
		Transform3D base_t = base_conf.transform;

		apply_12_rotations(base_h_inv_diag, base_t, "InvDiag");
	}
	uint8_t base_h_inv_opp = 0b11010111;
	if (mesh_library.has("11010111")) {
		MeshConfig base_conf = mesh_library["11010111"];
		Transform3D base_t = base_conf.transform;

		apply_4_rotations(base_h_inv_opp, base_t, "InvOpp");
	}

	// 7 Corners
	uint8_t base_h_inv = 0b11111110;
	if (mesh_library.has("11111110")) {
		MeshConfig base_conf = mesh_library["11111110"];
		Transform3D base_t = base_conf.transform;

		apply_8_rotations(base_h_inv, base_t, "7Corner");
	}
}

void MCNode::apply_4_rotations(uint8_t p_base_hash, const Transform3D &p_base_t, const String &p_prefix) {
	apply_transform_sequence(p_base_hash, p_base_t, { RY90 }, p_prefix + String(" Ry90"));
	apply_transform_sequence(p_base_hash, p_base_t, { RY180 }, p_prefix + String(" Ry180"));
	apply_transform_sequence(p_base_hash, p_base_t, { RY270 }, p_prefix + String(" Ry270"));
}

void MCNode::apply_6_rotations(uint8_t p_base_hash, const Transform3D &p_base_t, const String &p_prefix) {
	apply_transform_sequence(p_base_hash, p_base_t, { RX90 }, p_prefix + String(" Rx90"));
	apply_transform_sequence(p_base_hash, p_base_t, { RX180 }, p_prefix + String(" Rx180"));
	apply_transform_sequence(p_base_hash, p_base_t, { RX270 }, p_prefix + String(" Rx270"));
	apply_transform_sequence(p_base_hash, p_base_t, { RX90, RY90 }, p_prefix + String(" Rx90 Ry90"));
	apply_transform_sequence(p_base_hash, p_base_t, { RX90, RY270 }, p_prefix + String(" Rx90 Ry270"));
}

void MCNode::apply_8_rotations(uint8_t p_base_hash, const Transform3D &p_base_t, const String &p_prefix) {
	apply_transform_sequence(p_base_hash, p_base_t, { RY90 }, p_prefix + String(" Ry90"));
	apply_transform_sequence(p_base_hash, p_base_t, { RY180 }, p_prefix + String(" Ry180"));
	apply_transform_sequence(p_base_hash, p_base_t, { RY270 }, p_prefix + String(" Ry270"));
	apply_transform_sequence(p_base_hash, p_base_t, { RX270 }, p_prefix + String(" Rx270"));
	apply_transform_sequence(p_base_hash, p_base_t, { RX270, RY90 }, p_prefix + String(" Rx270 Ry90"));
	apply_transform_sequence(p_base_hash, p_base_t, { RX270, RY180 }, p_prefix + String(" Rx270 Ry180"));
	apply_transform_sequence(p_base_hash, p_base_t, { RX270, RY270 }, p_prefix + String(" Rx270 Ry270"));

	apply_transform_sequence(p_base_hash, p_base_t, { RX180 }, p_prefix + String(" Rx180"));
	apply_transform_sequence(p_base_hash, p_base_t, { RX180, RY90 }, p_prefix + String(" Rx180 Ry90"));
	apply_transform_sequence(p_base_hash, p_base_t, { RX180, RY180 }, p_prefix + String(" Rx180 Ry180"));
	apply_transform_sequence(p_base_hash, p_base_t, { RX180, RY270 }, p_prefix + String(" Rx180 Ry270"));
}

void MCNode::apply_12_rotations(uint8_t p_base_hash, const Transform3D &p_base_t, const String &p_prefix) {
	apply_transform_sequence(p_base_hash, p_base_t, { RY90 }, p_prefix + String(" Ry90"));
	apply_transform_sequence(p_base_hash, p_base_t, { RY180 }, p_prefix + String(" Ry180"));
	apply_transform_sequence(p_base_hash, p_base_t, { RY270 }, p_prefix + String(" Ry270"));

	apply_transform_sequence(p_base_hash, p_base_t, { RX180 }, p_prefix + String(" Rx180"));
	apply_transform_sequence(p_base_hash, p_base_t, { RX180, RY90 }, p_prefix + String(" Rx180 Ry90"));
	apply_transform_sequence(p_base_hash, p_base_t, { RX180, RY180 }, p_prefix + String(" Rx180 Ry180"));
	apply_transform_sequence(p_base_hash, p_base_t, { RX180, RY270 }, p_prefix + String(" Rx180 Ry270"));

	apply_transform_sequence(p_base_hash, p_base_t, { RX90 }, p_prefix + String(" Rx90"));
	apply_transform_sequence(p_base_hash, p_base_t, { RX90, RY90 }, p_prefix + String(" Rx90 Ry90"));
	apply_transform_sequence(p_base_hash, p_base_t, { RX90, RY180 }, p_prefix + String(" Rx90 Ry180"));
	apply_transform_sequence(p_base_hash, p_base_t, { RX90, RY270 }, p_prefix + String(" Rx90 Ry270"));

	apply_transform_sequence(p_base_hash, p_base_t, { RZ90 }, p_prefix + String(" Rz90"));
	apply_transform_sequence(p_base_hash, p_base_t, { RZ90, RY90 }, p_prefix + String(" Rz90 Ry90"));
	apply_transform_sequence(p_base_hash, p_base_t, { RZ90, RY180 }, p_prefix + String(" Rz90 Ry180"));
	apply_transform_sequence(p_base_hash, p_base_t, { RZ90, RY270 }, p_prefix + String(" Rz90 Ry270"));
}

void MCNode::apply_12_mirror_rotations(uint8_t p_base_hash, const Transform3D &p_base_t, const String &p_prefix) {
	// uint8_t h = scale_x(p_base_hash);
	// Transform3D t = p_base_t.scaled_local(Vector3(-1, 1, 1));
	// apply_transform_sequence(h, t, {}, p_prefix);
	// apply_12_rotations(h, t, p_prefix);

	apply_transform_sequence(p_base_hash, p_base_t, { SX }, p_prefix);
	apply_transform_sequence(p_base_hash, p_base_t, { SX, RY90 }, p_prefix + String(" Ry90"));
	apply_transform_sequence(p_base_hash, p_base_t, { SX, RY180 }, p_prefix + String(" Ry180"));
	apply_transform_sequence(p_base_hash, p_base_t, { SX, RY270 }, p_prefix + String(" Ry270"));

	apply_transform_sequence(p_base_hash, p_base_t, { SX, RX180 }, p_prefix + String(" Rx180"));
	apply_transform_sequence(p_base_hash, p_base_t, { SX, RX180, RY90 }, p_prefix + String(" Rx180 Ry90"));
	apply_transform_sequence(p_base_hash, p_base_t, { SX, RX180, RY180 }, p_prefix + String(" Rx180 Ry180"));
	apply_transform_sequence(p_base_hash, p_base_t, { SX, RX180, RY270 }, p_prefix + String(" Rx180 Ry270"));

	apply_transform_sequence(p_base_hash, p_base_t, { SX, RX90 }, p_prefix + String(" Rx90"));
	apply_transform_sequence(p_base_hash, p_base_t, { SX, RX90, RY90 }, p_prefix + String(" Rx90 Ry90"));
	apply_transform_sequence(p_base_hash, p_base_t, { SX, RX90, RY180 }, p_prefix + String(" Rx90 Ry180"));
	apply_transform_sequence(p_base_hash, p_base_t, { SX, RX90, RY270 }, p_prefix + String(" Rx90 Ry270"));

	apply_transform_sequence(p_base_hash, p_base_t, { SX, RZ90 }, p_prefix + String(" Rz90"));
	apply_transform_sequence(p_base_hash, p_base_t, { SX, RZ90, RY90 }, p_prefix + String(" Rz90 Ry90"));
	apply_transform_sequence(p_base_hash, p_base_t, { SX, RZ90, RY180 }, p_prefix + String(" Rz90 Ry180"));
	apply_transform_sequence(p_base_hash, p_base_t, { SX, RZ90, RY270 }, p_prefix + String(" Rz90 Ry270"));
}

void MCNode::apply_24_rotations(uint8_t p_base_hash, const Transform3D &p_base_t, const String &p_prefix) {
	apply_transform_sequence(p_base_hash, p_base_t, { RY90 }, p_prefix + String(" Ry90"));
	apply_transform_sequence(p_base_hash, p_base_t, { RY180 }, p_prefix + String(" Ry180"));
	apply_transform_sequence(p_base_hash, p_base_t, { RY270 }, p_prefix + String(" Ry270"));

	apply_transform_sequence(p_base_hash, p_base_t, { RX180 }, p_prefix + String(" Rx180"));
	apply_transform_sequence(p_base_hash, p_base_t, { RX180, RY90 }, p_prefix + String(" Rx180 Ry90"));
	apply_transform_sequence(p_base_hash, p_base_t, { RX180, RY180 }, p_prefix + String(" Rx180 Ry180"));
	apply_transform_sequence(p_base_hash, p_base_t, { RX180, RY270 }, p_prefix + String(" Rx180 Ry270"));

	apply_transform_sequence(p_base_hash, p_base_t, { RX90 }, p_prefix + String(" Rx90"));
	apply_transform_sequence(p_base_hash, p_base_t, { RX90, RZ90 }, p_prefix + String(" Rx90 Rz90"));
	apply_transform_sequence(p_base_hash, p_base_t, { RX90, RZ180 }, p_prefix + String(" Rx90 Rz180"));
	apply_transform_sequence(p_base_hash, p_base_t, { RX90, RZ270 }, p_prefix + String(" Rx90 Rz270"));

	apply_transform_sequence(p_base_hash, p_base_t, { RX270 }, p_prefix + String(" Rx270"));
	apply_transform_sequence(p_base_hash, p_base_t, { RX270, RZ90 }, p_prefix + String(" Rx270 Rz90"));
	apply_transform_sequence(p_base_hash, p_base_t, { RX270, RZ180 }, p_prefix + String(" Rx270 Rz180"));
	apply_transform_sequence(p_base_hash, p_base_t, { RX270, RZ270 }, p_prefix + String(" Rx270 Rz270"));

	apply_transform_sequence(p_base_hash, p_base_t, { RZ90 }, p_prefix + String(" RZ90"));
	apply_transform_sequence(p_base_hash, p_base_t, { RZ90, RX90 }, p_prefix + String(" RZ90 Rx90"));
	apply_transform_sequence(p_base_hash, p_base_t, { RZ90, RX180 }, p_prefix + String(" RZ90 Rx180"));
	apply_transform_sequence(p_base_hash, p_base_t, { RZ90, RX270 }, p_prefix + String(" RZ90 Rx270"));

	apply_transform_sequence(p_base_hash, p_base_t, { RZ270 }, p_prefix + String(" RZ270"));
	apply_transform_sequence(p_base_hash, p_base_t, { RZ270, RX90 }, p_prefix + String(" RZ270 Rx90"));
	apply_transform_sequence(p_base_hash, p_base_t, { RZ270, RX180 }, p_prefix + String(" RZ270 Rx180"));
	apply_transform_sequence(p_base_hash, p_base_t, { RZ270, RX270 }, p_prefix + String(" RZ270 Rx270"));
}

void MCNode::apply_transform_sequence(uint8_t p_base_hash, const Transform3D &p_base_t, const std::vector<MCTransform> &p_sequence, const String &p_name) {
	uint8_t h = p_base_hash;
	Transform3D t = p_base_t;

	float p90 = Math_PI / 2.0;
	float m90 = -Math_PI / 2.0;

	for (MCTransform op : p_sequence) {
		switch (op) {
			case RX90:
				h = rotate_x(h);
				t = t.rotated(Vector3(1, 0, 0), p90);
				break;
			case RX180:
				h = rotate_x(rotate_x(h));
				t = t.rotated(Vector3(1, 0, 0), Math_PI);
				break;
			case RX270:
				h = rotate_x(rotate_x(rotate_x(h)));
				t = t.rotated(Vector3(1, 0, 0), m90);
				break;

			case RY90:
				h = rotate_y(h);
				t = t.rotated(Vector3(0, 1, 0), p90);
				break;
			case RY180:
				h = rotate_y(rotate_y(h));
				t = t.rotated(Vector3(0, 1, 0), Math_PI);
				break;
			case RY270:
				h = rotate_y(rotate_y(rotate_y(h)));
				t = t.rotated(Vector3(0, 1, 0), m90);
				break;

			case RZ90:
				h = rotate_z(h);
				t = t.rotated(Vector3(0, 0, 1), p90);
				break;
			case RZ180:
				h = rotate_z(rotate_z(h));
				t = t.rotated(Vector3(0, 0, 1), Math_PI);
				break;
			case RZ270:
				h = rotate_z(rotate_z(rotate_z(h)));
				t = t.rotated(Vector3(0, 0, 1), m90);
				break;

			case SX:
				h = scale_x(h);
				t = t.scaled_local(Vector3(-1, 1, 1));
				break;
		}
	}

	String h_str = hash_to_binary(h);

	if (h_str == "11010100") {
		UtilityFunctions::print("\n:::::: ", p_base_hash, "\n", h, "\n", t, "\n");
	}

	if (!mesh_library.has(h_str)) {
		MeshConfig config;
		config.mesh = mesh_library[hash_to_binary(p_base_hash)].mesh;
		config.transform = t;
		config.source_mesh = hash_to_binary(p_base_hash);
		mesh_library[h_str] = config;
		UtilityFunctions::print("Generated Variant ", p_name, ": ", h_str);
	} else {
		UtilityFunctions::print("Rejected Variant ", p_name, ": ", h_str);
	}
}

void MCNode::display_library() {
	for (int i = get_child_count() - 1; i >= 0; i--) {
		Node *child = get_child(i);
		child->queue_free();
		remove_child(child);
	}

	float spacing = 2.0f; // 1 unit mesh + 1 unit gap

	// 1. Arrange Rows (Base meshes along Z based on master sorted order)
	for (int row = 0; row < (int)base_mesh_order.size(); row++) {
		String base_name = base_mesh_order[row];

		// 2. Find all variants for this row
		std::vector<String> variants;
		for (const KeyValue<String, MeshConfig> &E : mesh_library) {
			if (E.value.source_mesh == base_name) {
				variants.push_back(E.key);
			}
		}

		for (int col = 0; col < (int)variants.size(); col++) {
			String variant_name = variants[col];
			const MeshConfig &conf = mesh_library[variant_name];

			MeshInstance3D *mi = memnew(MeshInstance3D);
			mi->set_mesh(conf.mesh);
			mi->set_name(variant_name);

			Transform3D grid_t;
			grid_t.origin = Vector3(col * spacing, 0, row * spacing);
			mi->set_transform(grid_t * conf.transform);

			add_child(mi);
			if (Engine::get_singleton()->is_editor_hint()) {
				mi->set_owner(get_owner());
			}

			Label3D *label = memnew(Label3D);
			int bit_count = 0;
			for (int i = 0; i < variant_name.length(); i++) {
				if (variant_name[i] == '1')
					bit_count++;
			}
			label->set_text(String::num_int64(bit_count) + " : " + variant_name);
			label->set_position(grid_t.origin + Vector3(0, 1.5f, -0.2f));
			label->set_billboard_mode(BaseMaterial3D::BILLBOARD_ENABLED);
			label->set_font_size(24);
			add_child(label);
			if (Engine::get_singleton()->is_editor_hint()) {
				label->set_owner(get_owner());
			}
		}
	}
}

uint8_t MCNode::rotate_y(uint8_t p_hash) {
	uint8_t next = 0;
	if (is_c0(p_hash))
		activate_c1(next);
	if (is_c1(p_hash))
		activate_c2(next);
	if (is_c2(p_hash))
		activate_c3(next);
	if (is_c3(p_hash))
		activate_c0(next);
	if (is_c4(p_hash))
		activate_c5(next);
	if (is_c5(p_hash))
		activate_c6(next);
	if (is_c6(p_hash))
		activate_c7(next);
	if (is_c7(p_hash))
		activate_c4(next);
	return next;
}

uint8_t MCNode::rotate_x(uint8_t p_hash) {
	uint8_t next = 0;
	// Face x=0 (Corners 0,3,7,4) rotate CW: 0->3, 3->7, 7->4, 4->0
	if (is_c0(p_hash))
		activate_c3(next);
	if (is_c3(p_hash))
		activate_c7(next);
	if (is_c7(p_hash))
		activate_c4(next);
	if (is_c4(p_hash))
		activate_c0(next);

	// Face x=1 (Corners 1,2,6,5) rotate CW: 1->2, 2->6, 6->5, 5->1
	if (is_c1(p_hash))
		activate_c2(next);
	if (is_c2(p_hash))
		activate_c6(next);
	if (is_c6(p_hash))
		activate_c5(next);
	if (is_c5(p_hash))
		activate_c1(next);
	return next;
}

uint8_t MCNode::rotate_z(uint8_t p_hash) {
	uint8_t next = 0;
	// Face Z=0 (Corners 0,1,5,4)
	if (is_c0(p_hash))
		activate_c1(next);
	if (is_c1(p_hash))
		activate_c5(next);
	if (is_c5(p_hash))
		activate_c4(next);
	if (is_c4(p_hash))
		activate_c0(next);

	// Face Z=1 (Corners 3,2,6,7)
	if (is_c3(p_hash))
		activate_c2(next);
	if (is_c2(p_hash))
		activate_c6(next);
	if (is_c6(p_hash))
		activate_c7(next);
	if (is_c7(p_hash))
		activate_c3(next);
	return next;
}

uint8_t MCNode::scale_x(uint8_t p_hash) {
	uint8_t next = 0;
	if (is_c0(p_hash))
		activate_c1(next);
	if (is_c1(p_hash))
		activate_c0(next);
	if (is_c3(p_hash))
		activate_c2(next);
	if (is_c2(p_hash))
		activate_c3(next);
	if (is_c4(p_hash))
		activate_c5(next);
	if (is_c5(p_hash))
		activate_c4(next);
	if (is_c7(p_hash))
		activate_c6(next);
	if (is_c6(p_hash))
		activate_c7(next);
	return next;
}

String MCNode::hash_to_binary(uint8_t p_hash) {
	String s = "";
	for (int i = 7; i >= 0; i--) {
		s += (p_hash & (1 << i)) ? "1" : "0";
	}
	return s;
}
