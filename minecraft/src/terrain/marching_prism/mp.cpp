#include "mp.h"
#include <algorithm>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/label3d.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/packed_scene.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

void MPNode::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_mesh_library_path", "p_path"), &MPNode::set_mesh_library_path);
	ClassDB::bind_method(D_METHOD("get_mesh_library_path"), &MPNode::get_mesh_library_path);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "mesh_library_path"), "set_mesh_library_path", "get_mesh_library_path");

	ClassDB::bind_method(D_METHOD("load_mesh_library"), &MPNode::load_mesh_library);
	ClassDB::bind_method(D_METHOD("initialize_library"), &MPNode::initialize_library);
	ClassDB::bind_method(D_METHOD("display_library"), &MPNode::display_library);
	ClassDB::bind_method(D_METHOD("generate_variants"), &MPNode::generate_variants_by_120Y_rotation);
	ClassDB::bind_method(D_METHOD("get_variant_counts"), &MPNode::get_variant_counts);
}

MPNode::MPNode() = default;
MPNode::~MPNode() {}

void MPNode::_ready() {
	if (Engine::get_singleton()->is_editor_hint()) {
		return;
	}
	initialize_library();
	display_library();
}

void MPNode::initialize_library() {
	if (is_initialized)
		return;

	load_mesh_library();
	generate_variants_by_120Y_rotation();
	validate_full_library();

	is_initialized = true;
}

void MPNode::set_mesh_library_path(const String &p_path) {
	mesh_library_path = p_path;
}

String MPNode::get_mesh_library_path() const {
	return mesh_library_path;
}

// Meta struct to parse 6-bit binary strings (e.g., "000100")
struct LoadMeta {
	Node *child = nullptr;
	MeshInstance3D *mi = nullptr;
	int ones_count = 0;
	bool is_valid = true;

	LoadMeta(Node *p_child, MeshInstance3D *p_mi) : child(p_child), mi(p_mi) {
		if (child) {
			String p_bin = child->get_name().substr(0, 6); // 6 bits for Prisms
			if (p_bin.length() < 6) {
				is_valid = false;
				return;
			}
			for (int i = 0; i < p_bin.length(); i++) {
				if (p_bin[i] == '1') {
					ones_count++;
				} else if (p_bin[i] != '0') {
					is_valid = false;
				}
			}
		} else {
			is_valid = false;
		}
	}

	bool operator<(const LoadMeta &p_other) const {
		return ones_count < p_other.ones_count;
	}
};

void MPNode::load_mesh_library() {
	for (int i = 0; i < 64; i++) {
		mesh_library[i] = PrismMeshConfig();
	}

	Ref<PackedScene> scene = ResourceLoader::get_singleton()->load(mesh_library_path);
	if (scene.is_null()) {
		UtilityFunctions::print("MPNode: Failed to load mesh library at ", mesh_library_path);
		return;
	}

	Node *inst = scene->instantiate();
	if (!inst)
		return;

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
			LoadMeta meta(child, mi);
			if (meta.is_valid) {
				candidates.push_back(meta);
			}
		}
	}

	// Sort by bit-density (0 to 6 active corners)
	std::sort(candidates.begin(), candidates.end());

	base_mesh_order.clear();
	for (const LoadMeta &meta : candidates) {
		PrismMeshConfig config;
		config.mesh = meta.mi->get_mesh();
		config.transform = Transform3D(); // Identity

		String truncated_name = meta.child->get_name().substr(0, 6);
		uint8_t h = binary_to_hash(truncated_name);
		config.source_mesh = h;
		mesh_library[h] = config;
		base_mesh_order.push_back(h);

		UtilityFunctions::print("MPNode: Loaded base mesh: ", truncated_name, " (Hash: ", h, ")");
	}

	inst->queue_free();
}

void MPNode::generate_variants_by_120Y_rotation() {
	float rad_120 = Math_TAU / 3.0f; // Exactly 120 degrees

	for (int i = 0; i < 64; i++) {
		// If this index is a valid base mesh, generate its rotated variants
		if (mesh_library[i].mesh.is_valid() && mesh_library[i].source_mesh == i) {
			uint8_t h120 = rotate_y_120(i);
			uint8_t h240 = rotate_y_120(h120);

			Transform3D base_t = mesh_library[i].transform;

			// Generate 120-degree rotation if it doesn't exist
			if (!mesh_library[h120].mesh.is_valid()) {
				mesh_library[h120].mesh = mesh_library[i].mesh;
				mesh_library[h120].transform = base_t.rotated(Vector3(0, 1, 0), rad_120);
				mesh_library[h120].source_mesh = i;
				UtilityFunctions::print("Generated Variant (120deg): ", hash_to_binary(h120), " from ", hash_to_binary(i));
			}

			// Generate 240-degree rotation if it doesn't exist
			if (!mesh_library[h240].mesh.is_valid()) {
				mesh_library[h240].mesh = mesh_library[i].mesh;
				mesh_library[h240].transform = base_t.rotated(Vector3(0, 1, 0), rad_120 * 2.0f);
				mesh_library[h240].source_mesh = i;
				UtilityFunctions::print("Generated Variant (240deg): ", hash_to_binary(h240), " from ", hash_to_binary(i));
			}
		}
	}
}

void MPNode::validate_full_library() {
	int missing_count = 0;
	for (int i = 0; i < 64; i++) {
		if (!mesh_library[i].mesh.is_valid()) {
			UtilityFunctions::print("MPNode: ERROR - Missing mesh for hash: ", hash_to_binary((uint8_t)i));
			missing_count++;
		}
	}

	if (missing_count == 0) {
		UtilityFunctions::print("MPNode: SUCCESS - All 64 Prism configurations generated.");
	}
}

void MPNode::display_library() {
	for (int i = get_child_count() - 1; i >= 0; i--) {
		Node *child = get_child(i);
		child->queue_free();
		remove_child(child);
	}

	float spacing = 2.5f;

	for (int row = 0; row < (int)base_mesh_order.size(); row++) {
		uint8_t base_h = base_mesh_order[row];

		std::vector<uint8_t> variants;
		if (mesh_library[base_h].mesh.is_valid())
			variants.push_back(base_h);

		// Find variants generated by this base mesh
		for (int i = 0; i < 64; i++) {
			if (i == base_h)
				continue;
			if (mesh_library[i].mesh.is_valid() && mesh_library[i].source_mesh == base_h) {
				variants.push_back((uint8_t)i);
			}
		}

		for (int col = 0; col < (int)variants.size(); col++) {
			uint8_t variant_h = variants[col];
			const PrismMeshConfig &conf = mesh_library[variant_h];

			MeshInstance3D *mi = memnew(MeshInstance3D);
			mi->set_mesh(conf.mesh);
			String h_str = hash_to_binary(variant_h);
			mi->set_name(h_str);

			Transform3D grid_t;
			grid_t.origin = Vector3(col * spacing, 0, row * spacing);
			mi->set_transform(grid_t * conf.transform);

			add_child(mi);
			if (Engine::get_singleton()->is_editor_hint())
				mi->set_owner(get_owner());

			Label3D *label = memnew(Label3D);
			label->set_text(h_str);
			label->set_position(grid_t.origin + Vector3(0, 1.5f, -0.2f));
			label->set_billboard_mode(BaseMaterial3D::BILLBOARD_ENABLED);
			label->set_font_size(24);
			add_child(label);
			if (Engine::get_singleton()->is_editor_hint())
				label->set_owner(get_owner());
		}
	}
}

// ---------------------------------------------------------
// BITWISE HELPERS
// ---------------------------------------------------------

// Cycles corners 120 degrees on the Y Axis
uint8_t MPNode::rotate_y_120(uint8_t p_hash) {
	uint8_t bottom = p_hash & 7; // Bits 0, 1, 2
	uint8_t top = (p_hash >> 3) & 7; // Bits 3, 4, 5

	// Shift left by 1, wrap overflow back to bit 0
	uint8_t rot_bottom = ((bottom << 1) | (bottom >> 2)) & 7;
	uint8_t rot_top = ((top << 1) | (top >> 2)) & 7;

	return (rot_top << 3) | rot_bottom;
}

uint8_t MPNode::binary_to_hash(const String &p_bin) {
	uint8_t h = 0;
	// Reads 6 characters
	for (int i = 0; i < 6; i++) {
		if (p_bin[5 - i] == '1') {
			h |= (1 << i);
		}
	}
	return h;
}

String MPNode::hash_to_binary(uint8_t p_hash) {
	String s = "";
	for (int i = 5; i >= 0; i--) {
		s += (p_hash & (1 << i)) ? "1" : "0";
	}
	return s;
}

Dictionary MPNode::get_variant_counts() const {
	Dictionary counts;
	for (int i = 0; i < 64; i++) {
		if (!mesh_library[i].mesh.is_valid())
			continue;

		String base_hash_str = hash_to_binary(mesh_library[i].source_mesh);
		if (!counts.has(base_hash_str)) {
			counts[base_hash_str] = 0;
		}
		int current = counts[base_hash_str];
		counts[base_hash_str] = current + 1;
	}
	return counts;
}

} // namespace godot
