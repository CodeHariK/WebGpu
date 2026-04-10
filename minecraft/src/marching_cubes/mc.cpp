#include "marching_cubes/mc.h"
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/classes/input_event_mouse_button.hpp>
#include <godot_cpp/classes/label3d.hpp>
#include <godot_cpp/classes/mesh.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/packed_scene.hpp>
#include <godot_cpp/classes/panel.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/templates/hash_map.hpp>

#include "cui/cui.h"

namespace godot {

void MCNode::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_mesh_library_path", "p_path"), &MCNode::set_mesh_library_path);
	ClassDB::bind_method(D_METHOD("get_mesh_library_path"), &MCNode::get_mesh_library_path);

	ClassDB::bind_method(D_METHOD("load_mesh_library"), &MCNode::load_mesh_library);
	ClassDB::bind_method(D_METHOD("display_library"), &MCNode::display_library);
	ClassDB::bind_method(D_METHOD("generate_variants"), &MCNode::generate_variants);
	ClassDB::bind_method(D_METHOD("get_variant_counts"), &MCNode::get_variant_counts);
	ClassDB::bind_method(D_METHOD("print_library_hashes"), &MCNode::print_library_hashes);

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "mesh_library_path"), "set_mesh_library_path", "get_mesh_library_path");
	ClassDB::bind_method(D_METHOD("set_use_full_library", "p_use"), &MCNode::set_use_full_library);
	ClassDB::bind_method(D_METHOD("get_use_full_library"), &MCNode::get_use_full_library);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "use_full_library"), "set_use_full_library", "get_use_full_library");
}

MCNode::MCNode() = default;

MCNode::~MCNode() {
}

void MCNode::_ready() {
	if (Engine::get_singleton()->is_editor_hint()) {
		return;
	}
	load_mesh_library();
	if (use_full_library) {
		validate_full_library();
	} else {
		generate_variants();
	}
	display_library();
}

void MCNode::set_use_full_library(bool p_use) {
	use_full_library = p_use;
}

bool MCNode::get_use_full_library() const {
	return use_full_library;
}

void MCNode::set_mesh_library_path(const String &p_path) {
	mesh_library_path = p_path;
}

String MCNode::get_mesh_library_path() const {
	return mesh_library_path;
}

struct LoadMeta {
	Node *child = nullptr;
	MeshInstance3D *mi = nullptr;
	int ones_count = 0;

	LoadMeta(Node *p_child, MeshInstance3D *p_mi) :
			child(p_child), mi(p_mi) {
		if (child) {
			String p_bin = child->get_name().substr(0, 8);
			for (int i = 0; i < p_bin.length(); i++) {
				if (p_bin[i] == '1') {
					ones_count++;
				}
			}
		}
	}

	bool operator<(const LoadMeta &p_other) const {
		return ones_count < p_other.ones_count;
	}
};

void MCNode::load_mesh_library() {
	for (int i = 0; i < 256; i++) {
		mesh_library[i] = MeshConfig();
	}

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
				if (mi) {
					break;
				}
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

		String truncated_name = meta.child->get_name().substr(0, 8);
		uint8_t h = binary_to_hash(truncated_name);
		config.source_mesh = h;
		mesh_library[h] = config;
		base_mesh_order.push_back(h);
		UtilityFunctions::print("MCNode: Loaded mesh: ", truncated_name, " (Hash: ", h, ", Corners: ", meta.ones_count, ")");
	}

	int loaded_count = 0;
	for (int i = 0; i < 256; i++) {
		if (mesh_library[i].mesh.is_valid()) {
			loaded_count++;
		}
	}

	UtilityFunctions::print("MCNode: Finished loading library. Total items: ", loaded_count);
	inst->queue_free();
}

void MCNode::generate_variants() {
	// 1 Corner - 8 total (2 base meshes x 4 Y-rotations)
	for (uint8_t h : { 0b00000001, 0b00010000 }) {
		if (mesh_library[h].mesh.is_valid()) {
			apply_4_axis_rotations(h, h, mesh_library[h].transform, Vector3::AXIS_Y, "1Corner", true);
		}
	}

	// 2 Corners (Edge) - 12 total (3 base meshes x 4 Y-rotations)
	for (uint8_t h : { 0b00001001, 0b10010000, 0b00010001 }) {
		if (mesh_library[h].mesh.is_valid()) {
			apply_4_axis_rotations(h, h, mesh_library[h].transform, Vector3::AXIS_Y, "Corner2Edge", true);
		}
	}
	for (uint8_t h : { 0b00000101, 0b10100000, 0b00011000, 0b10000001 }) {
		if (mesh_library[h].mesh.is_valid()) {
			apply_4_axis_rotations(h, h, mesh_library[h].transform, Vector3::AXIS_Y, "Corner2Diag", true);
		}
	}
	for (uint8_t h : { 0b01000001 }) {
		if (mesh_library[h].mesh.is_valid()) {
			apply_4_axis_rotations(h, h, mesh_library[h].transform, Vector3::AXIS_Y, "Corner2Opp", false);
		}
	}

	// 3 Corners
	uint8_t base_h_v = 0b00000111;
	if (mesh_library[base_h_v].mesh.is_valid()) {
		MeshConfig base_conf = mesh_library[base_h_v];
		Transform3D base_t = base_conf.transform;

		apply_24_rotations(base_h_v, base_t, "V");
	}
	uint8_t base_h_l = 0b01000101;
	if (mesh_library[base_h_l].mesh.is_valid()) {
		MeshConfig base_conf = mesh_library[base_h_l];
		Transform3D base_t = base_conf.transform;

		apply_24_rotations(base_h_l, base_t, "L");
	}
	uint8_t base_h_tri = 0b10000101;
	if (mesh_library[base_h_tri].mesh.is_valid()) {
		MeshConfig base_conf = mesh_library[base_h_tri];
		Transform3D base_t = base_conf.transform;

		apply_8_rotations(base_h_tri, base_t, "Tri");
	}

	// 4 Corners
	uint8_t base_h_slab = 0b00001111;
	if (mesh_library[base_h_slab].mesh.is_valid()) {
		MeshConfig base_conf = mesh_library[base_h_slab];
		Transform3D base_t = base_conf.transform;

		apply_6_rotations(base_h_slab, base_t, "Slab");
	}
	uint8_t base_h_e1 = 0b10000111;
	if (mesh_library[base_h_e1].mesh.is_valid()) {
		MeshConfig base_conf = mesh_library[base_h_e1];
		Transform3D base_t = base_conf.transform;

		apply_24_rotations(base_h_e1, base_t, "E1");
	}
	uint8_t base_h_tetra = 0b10100101;
	if (mesh_library[base_h_tetra].mesh.is_valid()) {
		MeshConfig base_conf = mesh_library[base_h_tetra];
		Transform3D base_t = base_conf.transform;

		apply_transform_sequence(base_h_tetra, base_h_tetra, base_t, { RX90 }, "Tetra Rx90");
	}
	uint8_t base_h_e4 = 0b00100111;
	if (mesh_library[base_h_e4].mesh.is_valid()) {
		MeshConfig base_conf = mesh_library[base_h_e4];
		Transform3D base_t = base_conf.transform;

		apply_8_rotations(base_h_e4, base_t, "E4");
	}
	uint8_t base_h_e8 = 0b00010111;
	if (mesh_library[base_h_e8].mesh.is_valid()) {
		MeshConfig base_conf = mesh_library[base_h_e8];
		Transform3D base_t = base_conf.transform;

		apply_12_rotations(base_h_e8, base_h_e8, base_t, "E8");

		// Add mirror variants
		apply_12_mirror_rotations(base_h_e8, base_t, "E8 Sx");
	}
	uint8_t base_h_opp_edges = 0b01010101;
	if (mesh_library[base_h_opp_edges].mesh.is_valid()) {
		MeshConfig base_conf = mesh_library[base_h_opp_edges];
		Transform3D base_t = base_conf.transform;

		apply_6_rotations(base_h_opp_edges, base_t, "OppEdges");
	}

	// 5 Corners
	uint8_t base_h_v_inv = 0b01001111;
	if (mesh_library[base_h_v_inv].mesh.is_valid()) {
		MeshConfig base_conf = mesh_library[base_h_v_inv];
		Transform3D base_t = base_conf.transform;

		apply_24_rotations(base_h_v_inv, base_t, "InvV");
	}
	uint8_t base_h_ea = 0b01010111;
	if (mesh_library[base_h_ea].mesh.is_valid()) {
		MeshConfig base_conf = mesh_library[base_h_ea];
		Transform3D base_t = base_conf.transform;

		apply_24_rotations(base_h_ea, base_t, "InvV2");
	}
	uint8_t base_h_da = 0b01011011;
	if (mesh_library[base_h_da].mesh.is_valid()) {
		MeshConfig base_conf = mesh_library[base_h_da];
		Transform3D base_t = base_conf.transform;

		apply_8_rotations(base_h_da, base_t, "InvTri");
	}

	// 6 Corners
	for (uint8_t h : { 0b11001111, 0b11110110, 0b11101110 }) {
		if (mesh_library[h].mesh.is_valid()) {
			apply_4_axis_rotations(h, h, mesh_library[h].transform, Vector3::AXIS_Y, "Corner6Edge", true);
		}
	}
	for (uint8_t h : { 0b01011111, 0b11111010, 0b11100111, 0b01111110 }) {
		if (mesh_library[h].mesh.is_valid()) {
			apply_4_axis_rotations(h, h, mesh_library[h].transform, Vector3::AXIS_Y, "Corner6Diag", true);
		}
	}
	for (uint8_t h : { 0b11101011 }) {
		if (mesh_library[h].mesh.is_valid()) {
			apply_4_axis_rotations(h, h, mesh_library[h].transform, Vector3::AXIS_Y, "InvOpp", false);
		}
	}

	// 7 Corners - 8 total
	for (uint8_t h : { 0b11101111, 0b11111110 }) {
		if (mesh_library[h].mesh.is_valid()) {
			apply_4_axis_rotations(h, h, mesh_library[h].transform, Vector3::AXIS_Y, "7Corner", true);
		}
	}
}

void MCNode::validate_full_library() {
	UtilityFunctions::print("MCNode: Validating full 256-mesh library...");
	int missing_count = 0;
	for (int i = 0; i < 256; i++) {
		if (!mesh_library[i].mesh.is_valid()) {
			UtilityFunctions::print("MCNode: Missing mesh for hash: ", hash_to_binary((uint8_t)i));
			missing_count++;
		}
	}

	if (missing_count == 0) {
		UtilityFunctions::print("MCNode: Full library validation successful (all 256 meshes present).");
	} else {
		UtilityFunctions::print("MCNode: Full library validation failed. Missing ", missing_count, " meshes.");
	}
}

void MCNode::apply_4_axis_rotations(uint8_t p_base_hash, uint8_t p_variant_hash, const Transform3D &p_base_t, Vector3::Axis p_axis, const String &p_prefix, bool p_include_base) {
	if (p_include_base) {
		apply_transform_sequence(p_base_hash, p_variant_hash, p_base_t, {}, p_prefix);
	}

	MCTransform r90, r180, r270;
	String axis_name;

	switch (p_axis) {
		case Vector3::AXIS_X:
			r90 = RX90;
			r180 = RX180;
			r270 = RX270;
			axis_name = " Rx";
			break;
		case Vector3::AXIS_Y:
			r90 = RY90;
			r180 = RY180;
			r270 = RY270;
			axis_name = " Ry";
			break;
		case Vector3::AXIS_Z:
			r90 = RZ90;
			r180 = RZ180;
			r270 = RZ270;
			axis_name = " Rz";
			break;
	}

	apply_transform_sequence(p_base_hash, p_variant_hash, p_base_t, { r90 }, p_prefix + axis_name + "90");
	apply_transform_sequence(p_base_hash, p_variant_hash, p_base_t, { r180 }, p_prefix + axis_name + "180");
	apply_transform_sequence(p_base_hash, p_variant_hash, p_base_t, { r270 }, p_prefix + axis_name + "270");
}

void MCNode::apply_6_rotations(uint8_t p_base_hash, const Transform3D &p_base_t, const String &p_prefix) {
	apply_4_axis_rotations(p_base_hash, p_base_hash, p_base_t, Vector3::AXIS_X, p_prefix, false);

	uint8_t h90 = p_base_hash;
	Transform3D t90 = p_base_t;
	apply_mct_transform(h90, t90, RX90);
	apply_transform_sequence(p_base_hash, h90, t90, { RY90 }, p_prefix + String(" Rx90 Ry90"));
	apply_transform_sequence(p_base_hash, h90, t90, { RY270 }, p_prefix + String(" Rx90 Ry270"));
}

void MCNode::apply_8_rotations(uint8_t p_base_hash, const Transform3D &p_base_t, const String &p_prefix) {
	apply_4_axis_rotations(p_base_hash, p_base_hash, p_base_t, Vector3::AXIS_Y, p_prefix, false);

	for (MCTransform base_rot : { RX270, RX180 }) {
		uint8_t h = p_base_hash;
		Transform3D t = p_base_t;
		apply_mct_transform(h, t, base_rot);
		String name = (base_rot == RX270) ? " Rx270" : " Rx180";
		apply_4_axis_rotations(p_base_hash, h, t, Vector3::AXIS_Y, p_prefix + name, true);
	}
}

void MCNode::apply_12_rotations(uint8_t p_base_hash, uint8_t p_variant_hash, const Transform3D &p_base_t, const String &p_prefix) {
	apply_4_axis_rotations(p_base_hash, p_variant_hash, p_base_t, Vector3::AXIS_Y, p_prefix, false);

	for (MCTransform axis_rot : { RX180, RX90, RZ90 }) {
		uint8_t h = p_variant_hash;
		Transform3D t = p_base_t;
		apply_mct_transform(h, t, axis_rot);
		String name = (axis_rot == RX180) ? " Rx180" : (axis_rot == RX90 ? " Rx90" : " Rz90");
		apply_4_axis_rotations(p_base_hash, h, t, Vector3::AXIS_Y, p_prefix + name, true);
	}
}

void MCNode::apply_12_mirror_rotations(uint8_t p_base_hash, const Transform3D &p_base_t, const String &p_prefix) {
	uint8_t h = p_base_hash;
	Transform3D t = p_base_t;
	apply_mct_transform(h, t, SX);
	apply_transform_sequence(p_base_hash, h, t, {}, p_prefix);
	apply_12_rotations(p_base_hash, h, t, p_prefix);
}

void MCNode::apply_24_rotations(uint8_t p_base_hash, const Transform3D &p_base_t, const String &p_prefix) {
	apply_4_axis_rotations(p_base_hash, p_base_hash, p_base_t, Vector3::AXIS_Y, p_prefix, false);

	// Rx180 -> Ry rotations
	{
		uint8_t h = p_base_hash;
		Transform3D t = p_base_t;
		apply_mct_transform(h, t, RX180);
		apply_4_axis_rotations(p_base_hash, h, t, Vector3::AXIS_Y, p_prefix + String(" Rx180"), true);
	}

	// Rx90/270 -> Rz rotations
	for (MCTransform rx : { RX90, RX270 }) {
		uint8_t h = p_base_hash;
		Transform3D t = p_base_t;
		apply_mct_transform(h, t, rx);
		apply_4_axis_rotations(p_base_hash, h, t, Vector3::AXIS_Z, p_prefix + String(rx == RX90 ? " Rx90" : " Rx270"), true);
	}

	// Rz90/270 -> Rx rotations
	for (MCTransform rz : { RZ90, RZ270 }) {
		uint8_t h = p_base_hash;
		Transform3D t = p_base_t;
		apply_mct_transform(h, t, rz);
		apply_4_axis_rotations(p_base_hash, h, t, Vector3::AXIS_X, p_prefix + String(rz == RZ90 ? " Rz90" : " Rz270"), true);
	}
}

void MCNode::apply_mct_transform(uint8_t &p_hash, Transform3D &p_transform, MCTransform p_op) {
	float p90 = Math_PI / 2.0;
	float m90 = -Math_PI / 2.0;

	switch (p_op) {
		case RX90:
			p_hash = rotate_x(p_hash);
			p_transform = p_transform.rotated(Vector3(1, 0, 0), p90);
			break;
		case RX180:
			p_hash = rotate_x(rotate_x(p_hash));
			p_transform = p_transform.rotated(Vector3(1, 0, 0), Math_PI);
			break;
		case RX270:
			p_hash = rotate_x(rotate_x(rotate_x(p_hash)));
			p_transform = p_transform.rotated(Vector3(1, 0, 0), m90);
			break;

		case RY90:
			p_hash = rotate_y(p_hash);
			p_transform = p_transform.rotated(Vector3(0, 1, 0), p90);
			break;
		case RY180:
			p_hash = rotate_y(rotate_y(p_hash));
			p_transform = p_transform.rotated(Vector3(0, 1, 0), Math_PI);
			break;
		case RY270:
			p_hash = rotate_y(rotate_y(rotate_y(p_hash)));
			p_transform = p_transform.rotated(Vector3(0, 1, 0), m90);
			break;

		case RZ90:
			p_hash = rotate_z(p_hash);
			p_transform = p_transform.rotated(Vector3(0, 0, 1), p90);
			break;
		case RZ180:
			p_hash = rotate_z(rotate_z(p_hash));
			p_transform = p_transform.rotated(Vector3(0, 0, 1), Math_PI);
			break;
		case RZ270:
			p_hash = rotate_z(rotate_z(rotate_z(p_hash)));
			p_transform = p_transform.rotated(Vector3(0, 0, 1), m90);
			break;

		case SX:
			p_hash = scale_x(p_hash);
			p_transform = p_transform.scaled_local(Vector3(-1, 1, 1));
			break;
	}
}

void MCNode::apply_transform_sequence(uint8_t p_base_hash, uint8_t p_variant_hash, const Transform3D &p_base_t, const std::vector<MCTransform> &p_sequence, const String &p_name) {
	uint8_t h = p_variant_hash;
	Transform3D t = p_base_t;

	for (MCTransform op : p_sequence) {
		apply_mct_transform(h, t, op);
	}

	String h_str = hash_to_binary(h);

	if (!mesh_library[h].mesh.is_valid()) {
		MeshConfig config;
		config.mesh = mesh_library[p_base_hash].mesh;
		config.transform = t;
		config.source_mesh = p_base_hash;
		mesh_library[h] = config;
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
		uint8_t base_h = base_mesh_order[row];

		// 2. Find all variants for this row
		std::vector<uint8_t> variants;
		if (mesh_library[base_h].mesh.is_valid()) {
			variants.push_back(base_h);
		}

		for (int i = 0; i < 256; i++) {
			if (i == base_h) {
				continue;
			}
			if (mesh_library[i].mesh.is_valid() && mesh_library[i].source_mesh == base_h) {
				variants.push_back((uint8_t)i);
			}
		}

		for (int col = 0; col < (int)variants.size(); col++) {
			uint8_t variant_h = variants[col];
			const MeshConfig &conf = mesh_library[variant_h];

			MeshInstance3D *mi = memnew(MeshInstance3D);
			mi->set_mesh(conf.mesh);
			String h_str = hash_to_binary(variant_h);
			mi->set_name(h_str);

			Transform3D grid_t;
			grid_t.origin = Vector3(col * spacing, 0, row * spacing);
			mi->set_transform(grid_t * conf.transform);

			add_child(mi);
			if (Engine::get_singleton()->is_editor_hint()) {
				mi->set_owner(get_owner());
			}

			Label3D *label = memnew(Label3D);
			int bit_count = 0;
			for (int i = 0; i < 8; i++) {
				if ((variant_h >> i) & 1) {
					bit_count++;
				}
			}
			label->set_text(String::num_int64(bit_count) + " : " + h_str);
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
	static const uint8_t swap_table[8] = { C1, C2, C3, C0, C5, C6, C7, C4 };
	uint8_t next = 0;
	for (int i = 7; i >= 0; i--) {
		if ((p_hash >> i) & 1) {
			next |= (1 << swap_table[i]);
		}
	}
	return next;
}

uint8_t MCNode::rotate_x(uint8_t p_hash) {
	static const uint8_t swap_table[8] = { C3, C2, C6, C7, C0, C1, C5, C4 };
	uint8_t next = 0;
	for (int i = 7; i >= 0; i--) {
		if ((p_hash >> i) & 1) {
			next |= (1 << swap_table[i]);
		}
	}
	return next;
}

uint8_t MCNode::rotate_z(uint8_t p_hash) {
	static const uint8_t swap_table[8] = { C1, C5, C6, C2, C0, C4, C7, C3 };
	uint8_t next = 0;
	for (int i = 7; i >= 0; i--) {
		if ((p_hash >> i) & 1) {
			next |= (1 << swap_table[i]);
		}
	}
	return next;
}

uint8_t MCNode::scale_x(uint8_t p_hash) {
	static const uint8_t swap_table[8] = { C1, C0, C3, C2, C5, C4, C7, C6 };
	uint8_t next = 0;
	for (int i = 7; i >= 0; i--) {
		if ((p_hash >> i) & 1) {
			next |= (1 << swap_table[i]);
		}
	}
	return next;
}

uint8_t MCNode::binary_to_hash(const String &p_bin) {
	uint8_t h = 0;
	for (int i = 0; i < 8; i++) {
		if (p_bin[7 - i] == '1') {
			h |= (1 << i);
		}
	}
	return h;
}

String MCNode::hash_to_binary(uint8_t p_hash) {
	String s = "";
	for (int i = 7; i >= 0; i--) {
		s += (p_hash & (1 << i)) ? "1" : "0";
	}
	return s;
}

MeshConfig MCNode::get_mesh_config(uint8_t p_hash) const {
	return mesh_library[p_hash];
}

Dictionary MCNode::get_variant_counts() const {
	Dictionary counts;
	for (int i = 0; i < 256; i++) {
		if (!mesh_library[i].mesh.is_valid()) {
			continue;
		}
		String base_hash_str = hash_to_binary(mesh_library[i].source_mesh);
		if (!counts.has(base_hash_str)) {
			counts[base_hash_str] = 0;
		}
		int current = counts[base_hash_str];
		counts[base_hash_str] = current + 1;
	}
	return counts;
}

void MCNode::print_library_hashes() const {
	UtilityFunctions::print("MCNode: --- Marching Cubes Config Table ---");
	for (uint8_t base_h : base_mesh_order) {
		String base_h_str = hash_to_binary(base_h);
		UtilityFunctions::print("Base Hash: ", base_h_str);
		for (int i = 0; i < 256; i++) {
			if (mesh_library[i].mesh.is_valid() && mesh_library[i].source_mesh == base_h && i != base_h) {
				UtilityFunctions::print("  ", hash_to_binary((uint8_t)i));
			}
		}
	}
	int total_count = 0;
	for (int i = 0; i < 256; i++) {
		if (mesh_library[i].mesh.is_valid()) {
			total_count++;
		}
	}
	UtilityFunctions::print("MCNode: Total table size: ", total_count);
}

} // namespace godot
