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
	int variant = 1; // tag number: "_ma2" (or the old "_v2") -> 2
	char height = 'm'; // tag height: 'l', 'm' or 'h'
	bool group_a = true; // tag group is a (other groups are not used yet)
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
				mc::TileTag tag = mc::tile_tag(child->get_name());
				variant = tag.number;
				height = tag.height;
				group_a = tag.group == 'a';
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

// Add one loaded mesh to a level: the first mesh of a hash becomes its tile, later ones its
// variants (in number order); a second mesh with the same number is skipped. Returns true for an
// extra variant.
static bool add_loaded(
		MCTile *tiles,
		std::vector<uint8_t> &loaded_hashes,
		int *last_variant,
		const LoadMeta &meta
) {
	uint8_t h = meta.hash;
	MCTile &config = tiles[h];
	bool extra = false;
	if (config.mesh.is_null()) {
		config.mesh = meta.mi->get_mesh();
		config.turn = Transform3D(); // Identity for original GLB nodes
		config.source_hash = h;
		loaded_hashes.push_back(h);
	} else if (meta.variant == last_variant[h]) {
		UtilityFunctions::push_warning(
				"MCTileLibrary: two meshes for ", mc::hash_to_binary(h), " ", String::chr(meta.height), "a",
				meta.variant, "; skipping ", meta.child->get_name()
		);
		return false;
	} else {
		extra = true;
	}
	config.variant_meshes.push_back(meta.mi->get_mesh());
	last_variant[h] = meta.variant;
	return extra;
}

// Instantiate the .glb, read every node named like a tile (hash + tag, or the old _vN) into its
// level (M a: tiles[], L a: ground, H a: roof), variants in number order, and list the modelled
// hashes of each level in load order.
void MCTileLibrary::load_tiles() {
	for (int i = 0; i < 256; i++) {
		tiles[i] = MCTile();
		ground.tiles[i] = MCTile();
		roof.tiles[i] = MCTile();
	}
	loaded_hashes.clear();
	ground.loaded_hashes.clear();
	roof.loaded_hashes.clear();

	Ref<PackedScene> scene = ResourceLoader::get_singleton()->load(tile_set_path);
	if (scene.is_null()) {
		UtilityFunctions::print("MCTileLibrary: Failed to load mesh library at ", tile_set_path);
		return;
	}

	Node *inst = scene->instantiate();
	if (!inst) {
		return;
	}

	// 1. Collect all candidates (group a only: other groups are not placed yet)
	std::vector<LoadMeta> candidates;
	int other_groups = 0;
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
			if (meta.is_valid && !meta.group_a) {
				other_groups++;
			} else if (meta.is_valid) {
				candidates.push_back(meta);
			} else {
				UtilityFunctions::print("MCTileLibrary: Skipping mesh with invalid name: ", child->get_name());
			}
		}
	}

	if (other_groups > 0) {
		UtilityFunctions::print("MCTileLibrary: ", other_groups, " tiles of groups b..z left out (only group a is used).");
	}

	// 2. Sort by bit-density (number of active corners)
	std::sort(candidates.begin(), candidates.end());

	// 3. Each mesh into its level, in that order
	int variant_count = 0;
	int last_variant[3][256] = {};
	for (const LoadMeta &meta : candidates) {
		bool extra = false;
		if (meta.height == 'l') {
			extra = add_loaded(ground.tiles, ground.loaded_hashes, last_variant[0], meta);
		} else if (meta.height == 'h') {
			extra = add_loaded(roof.tiles, roof.loaded_hashes, last_variant[2], meta);
		} else {
			extra = add_loaded(tiles, loaded_hashes, last_variant[1], meta);
		}
		variant_count += extra ? 1 : 0;
	}
	UtilityFunctions::print(
			"MCTileLibrary: ", static_cast<int64_t>(loaded_hashes.size()), " M tiles, ",
			static_cast<int64_t>(ground.loaded_hashes.size()), " L tiles, ", static_cast<int64_t>(roof.loaded_hashes.size()),
			" H tiles, ", variant_count, " extra variants."
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

// The tile of a level for a cell hash: 'l' ground, 'h' roof, anything else the main (M a) tiles.
const MCTile &MCTileLibrary::get_level_tile(
		char p_height,
		uint8_t p_hash
) const {
	if (p_height == 'l') {
		return ground.tiles[p_hash];
	}
	if (p_height == 'h') {
		return roof.tiles[p_hash];
	}
	return tiles[p_hash];
}

// The level's tile, or the M a tile when the level has none for this hash.
const MCTile &MCTileLibrary::get_tile_for_level(
		char p_height,
		uint8_t p_hash
) const {
	const MCTile &tile = get_level_tile(p_height, p_hash);
	return tile.mesh.is_valid() ? tile : tiles[p_hash];
}

// Does the set have any tile of this level?
bool MCTileLibrary::has_level(char p_height) const {
	if (p_height == 'l') {
		return !ground.loaded_hashes.empty();
	}
	if (p_height == 'h') {
		return !roof.loaded_hashes.empty();
	}
	return !loaded_hashes.empty();
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
