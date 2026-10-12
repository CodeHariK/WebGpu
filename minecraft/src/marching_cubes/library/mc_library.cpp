// MCTileLibrary: load the base tile meshes and look tiles up by hash.
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

// Inspector property (tile_set_path) and the load / fill / showcase methods.
void MCTileLibrary::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_tile_set_path", "p_path"), &MCTileLibrary::set_tile_set_path);
	ClassDB::bind_method(D_METHOD("get_tile_set_path"), &MCTileLibrary::get_tile_set_path);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "tile_set_path"), "set_tile_set_path", "get_tile_set_path");

	ClassDB::bind_method(D_METHOD("load_tiles"), &MCTileLibrary::load_tiles);
	ClassDB::bind_method(D_METHOD("initialize_library"), &MCTileLibrary::initialize_library);
	ClassDB::bind_method(D_METHOD("build_showcase"), &MCTileLibrary::build_showcase);
	ClassDB::bind_method(D_METHOD("fill_by_y_turns"), &MCTileLibrary::fill_by_y_turns);
	ClassDB::bind_method(D_METHOD("get_hashes_per_tile"), &MCTileLibrary::get_hashes_per_tile);
}

MCTileLibrary::MCTileLibrary() = default;

MCTileLibrary::~MCTileLibrary() {}

// Outside the editor: load the tiles and build the showcase.
void MCTileLibrary::_ready() {
	if (Engine::get_singleton()->is_editor_hint()) {
		return;
	}
	initialize_library();
	build_showcase();
}

// Load, fill by Y turns, report what is missing. Only the first call does anything.
void MCTileLibrary::initialize_library() {
	if (is_initialized) {
		return;
	}

	load_tiles();
	int generated = fill_by_y_turns();
	UtilityFunctions::print(
			"MCTileLibrary: ", static_cast<int64_t>(loaded_hashes.size()), " tiles loaded, ", generated,
			" filled by turning about Y."
	);
	report_missing();

	is_initialized = true;
}

// The tile-set .glb to load.
void MCTileLibrary::set_tile_set_path(const String &p_path) { tile_set_path = p_path; }

// The tile-set .glb to load.
String MCTileLibrary::get_tile_set_path() const { return tile_set_path; }

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

// Instantiate the .glb, read every node named like a tile (hash + optional _vN) into tiles[],
// variants in number order, and list the modelled hashes in loaded_hashes.
void MCTileLibrary::load_tiles() {
	for (int i = 0; i < 256; i++) {
		tiles[i] = MCTile();
	}

	Ref<PackedScene> scene = ResourceLoader::get_singleton()->load(tile_set_path);
	if (scene.is_null()) {
		UtilityFunctions::print("MCTileLibrary: Failed to load mesh library at ", tile_set_path);
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
				UtilityFunctions::print("MCTileLibrary: Skipping mesh with invalid name: ", child->get_name());
			}
		}
	}

	// 2. Sort by bit-density (number of active corners)
	std::sort(candidates.begin(), candidates.end());

	// 3. Populate Library and Master Order
	loaded_hashes.clear();
	int variant_count = 0;
	int last_variant[256] = {};
	for (const LoadMeta &meta : candidates) {
		uint8_t h = meta.hash;
		MCTile &config = tiles[h];
		if (config.mesh.is_null()) {
			config.mesh = meta.mi->get_mesh();
			config.turn = Transform3D(); // Identity for original GLB nodes
			config.source_hash = h;
			loaded_hashes.push_back(h);
		} else if (meta.variant == last_variant[h]) {
			UtilityFunctions::push_warning(
					"MCTileLibrary: two meshes for ", mc::hash_to_binary(h), " variant ", meta.variant, "; skipping ",
					meta.child->get_name()
			);
			continue;
		} else {
			variant_count++;
		}
		config.variant_meshes.push_back(meta.mi->get_mesh());
		last_variant[h] = meta.variant;
	}
	UtilityFunctions::print(
			"MCTileLibrary: ", static_cast<int64_t>(loaded_hashes.size()), " tiles, ", variant_count, " extra variants."
	);

	int loaded_count = 0;
	for (int i = 0; i < 256; i++) {
		if (tiles[i].mesh.is_valid()) {
			loaded_count++;
		}
	}

	UtilityFunctions::print("MCTileLibrary: Finished loading library. Total items: ", loaded_count);
	inst->queue_free();
}

// One of the variants, chosen from the cell position (stable across rebuilds).
Ref<Mesh> MCTile::mesh_for_cell(const Vector3i &p_cell) const {
	if (variant_meshes.size() <= 1) {
		return mesh;
	}
	return variant_meshes[static_cast<size_t>(mc::variant_index(p_cell, static_cast<int>(variant_meshes.size())))];
}

// For the stats panel: how many hashes each modelled tile draws.
Dictionary MCTileLibrary::get_hashes_per_tile() const {
	Dictionary counts;
	for (int i = 0; i < 256; i++) {
		if (!tiles[i].mesh.is_valid()) {
			continue;
		}
		String base_hash_str = mc::hash_to_binary(tiles[i].source_hash);
		if (!counts.has(base_hash_str)) {
			counts[base_hash_str] = 0;
		}
		int current = counts[base_hash_str];
		counts[base_hash_str] = current + 1;
	}
	return counts;
}

} // namespace godot
