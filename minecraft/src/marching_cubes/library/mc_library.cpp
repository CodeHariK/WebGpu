// MCNode: load the base tile meshes and look tiles up by hash.
#include "marching_cubes/library/mc_library.h"
#include "marching_cubes/core/mc_hash.h"
#include "marching_cubes/core/mc_variant.h"
#include <algorithm>
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
#include <vector>

namespace godot {

void MCNode::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_mesh_library_path", "p_path"), &MCNode::set_mesh_library_path);
	ClassDB::bind_method(D_METHOD("get_mesh_library_path"), &MCNode::get_mesh_library_path);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "mesh_library_path"), "set_mesh_library_path", "get_mesh_library_path");

	ClassDB::bind_method(D_METHOD("load_mesh_library"), &MCNode::load_mesh_library);
	ClassDB::bind_method(D_METHOD("initialize_library"), &MCNode::initialize_library);
	ClassDB::bind_method(D_METHOD("display_library"), &MCNode::display_library);
	ClassDB::bind_method(D_METHOD("generate_variants"), &MCNode::fill_y_rotations);
	ClassDB::bind_method(D_METHOD("get_variant_counts"), &MCNode::get_variant_counts);
}

MCNode::MCNode() = default;

MCNode::~MCNode() {}

void MCNode::_ready() {
	if (Engine::get_singleton()->is_editor_hint()) {
		return;
	}
	initialize_library();
	display_library();
}

void MCNode::initialize_library() {
	if (is_initialized) {
		return;
	}

	load_mesh_library();
	int generated = fill_y_rotations();
	UtilityFunctions::print(
			"MCNode: ", static_cast<int64_t>(base_mesh_order.size()), " tiles loaded, ", generated,
			" filled by turning about Y."
	);
	report_missing();

	is_initialized = true;
}

void MCNode::set_mesh_library_path(const String &p_path) { mesh_library_path = p_path; }

String MCNode::get_mesh_library_path() const { return mesh_library_path; }

struct LoadMeta {
	Node *child = nullptr;
	MeshInstance3D *mi = nullptr;
	int ones_count = 0;
	uint8_t hash = 0;
	int variant = 1; // "_v2" suffix -> 2
	bool is_valid = true;

	LoadMeta(
			Node *p_child,
			MeshInstance3D *p_mi
	) :
			child(p_child),
			mi(p_mi) {
		if (child) {
			String p_bin = child->get_name().substr(0, 8);
			if (p_bin.length() < 8) {
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
			if (is_valid) {
				hash = mc::binary_to_hash(p_bin);
				variant = mc::variant_number(child->get_name());
			}
		} else {
			is_valid = false;
		}
	}

	// Fewest corners first, then lowest hash, then variant: a fixed order, so gap filling is
	// repeatable and variants end up in number order.
	bool operator<(const LoadMeta &p_other) const {
		if (ones_count != p_other.ones_count) {
			return ones_count < p_other.ones_count;
		}
		if (hash != p_other.hash) {
			return hash < p_other.hash;
		}
		return variant < p_other.variant;
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
			LoadMeta meta(child, mi);
			if (meta.is_valid) {
				candidates.push_back(meta);
			} else {
				UtilityFunctions::print("MCNode: Skipping mesh with invalid name: ", child->get_name());
			}
		}
	}

	// 2. Sort by bit-density (number of active corners)
	std::sort(candidates.begin(), candidates.end());

	// 3. Populate Library and Master Order
	base_mesh_order.clear();
	int variant_count = 0;
	int last_variant[256] = {};
	for (const LoadMeta &meta : candidates) {
		uint8_t h = meta.hash;
		MeshConfig &config = mesh_library[h];
		if (config.mesh.is_null()) {
			config.mesh = meta.mi->get_mesh();
			config.transform = Transform3D(); // Identity for original GLB nodes
			config.source_mesh = h;
			base_mesh_order.push_back(h);
		} else if (meta.variant == last_variant[h]) {
			UtilityFunctions::push_warning(
					"MCNode: two meshes for ", mc::hash_to_binary(h), " variant ", meta.variant, "; skipping ",
					meta.child->get_name()
			);
			continue;
		} else {
			variant_count++;
		}
		config.variants.push_back(meta.mi->get_mesh());
		last_variant[h] = meta.variant;
	}
	UtilityFunctions::print(
			"MCNode: ", static_cast<int64_t>(base_mesh_order.size()), " tiles, ", variant_count, " extra variants."
	);

	int loaded_count = 0;
	for (int i = 0; i < 256; i++) {
		if (mesh_library[i].mesh.is_valid()) {
			loaded_count++;
		}
	}

	UtilityFunctions::print("MCNode: Finished loading library. Total items: ", loaded_count);
	inst->queue_free();
}

MeshConfig MCNode::get_mesh_config(uint8_t p_hash) const { return mesh_library[p_hash]; }

Ref<Mesh> MeshConfig::mesh_for_cell(const Vector3i &p_cell) const {
	if (variants.size() <= 1) {
		return mesh;
	}
	return variants[static_cast<size_t>(mc::variant_index(p_cell, static_cast<int>(variants.size())))];
}

Dictionary MCNode::get_variant_counts() const {
	Dictionary counts;
	for (int i = 0; i < 256; i++) {
		if (!mesh_library[i].mesh.is_valid()) {
			continue;
		}
		String base_hash_str = mc::hash_to_binary(mesh_library[i].source_mesh);
		if (!counts.has(base_hash_str)) {
			counts[base_hash_str] = 0;
		}
		int current = counts[base_hash_str];
		counts[base_hash_str] = current + 1;
	}
	return counts;
}

} // namespace godot
