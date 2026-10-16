// MCTileLibrary: the tile library. Loads the tile meshes from a .glb (each named by its 8-bit corner
// pattern and tag, e.g. "00001111_ma1"; core/mc_variant.h) and fills the other hashes by turning
// them about Y. Any tile set works (68 tiles, a heightmap set, a complete set): no mode setting is
// needed. Tiles are kept per level (tag height, group a): M a is tiles[], L a and H a are the
// ground / roof tables; MCGrid draws each cell from its level (core/mc_level_plan.h). Other groups
// are not used yet.
//   mc_library.cpp           loading + lookup
//   mc_library_fill.cpp      filling the gaps by Y turns, reporting what is still missing
//   mc_library_showcase.cpp  build_showcase(): every tile laid out in a grid, for checking
#ifndef MC_LIBRARY_H
#define MC_LIBRARY_H

#include <cstdint>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/templates/hash_map.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/transform3d.hpp>
#include <vector>

namespace godot {
class Mesh;
class PackedScene;
class InputEvent;
class VBoxContainer;
class Panel;
class AcceptDialog;
class CUI;

// Everything needed to draw one cell hash: the tile's mesh (and its variants) and the Y turn that
// makes a modelled tile fit this hash. tiles[hash] in MCTileLibrary; mesh is null when the set has
// no tile for the hash (that cell draws nothing).
struct MCTile {
	Ref<Mesh> mesh; // the first variant; null = no tile for this hash
	std::vector<Ref<Mesh>> variant_meshes; // every modelled variant, by number (variant_meshes[0] == mesh)
	Transform3D turn; // Y turn applied to the modelled tile (identity for tiles loaded as they are)
	uint8_t source_hash = 0; // hash of the modelled tile this entry was made from

	// The mesh a cell at p_cell (grid coordinates) uses: one of the variants, picked from the
	// cell position (see core/mc_variant.h).
	Ref<Mesh> mesh_for_cell(const Vector3i &p_cell) const;
};

// The tiles of one level: tiles[hash] and the modelled hashes in load order.
struct MCTileLevel {
	MCTile tiles[256];
	std::vector<uint8_t> loaded_hashes; // fewest corners first, then hash
};

// The tile library: tiles[hash] for all 256 cell hashes, loaded from one tile-set .glb.
class MCTileLibrary : public Node3D {
	GDCLASS(MCTileLibrary,
			Node3D)

private:
	String tile_set_path = "res://assets/MarchingCubes.glb";
	bool is_initialized = false;
	MCTile tiles[256]; // M a: the main level

	// Hashes of the modelled M a tiles in load order (fewest corners first, then hash).
	std::vector<uint8_t> loaded_hashes;

	MCTileLevel ground; // L a ("_la1"): bottom layer only
	MCTileLevel roof; // H a ("_ha1"): roof cells (column tops)

protected:
	// Inspector property and the methods scripts call.
	static void _bind_methods();

public:
	MCTileLibrary();
	~MCTileLibrary();

	// Outside the editor: load the tiles and lay them out as a showcase.
	void _ready() override;

	// The tile-set .glb (e.g. res://assets/Marching/Cube.glb), exported by the Blender add-on.
	void set_tile_set_path(const String &p_path);
	// The tile-set .glb path.
	String get_tile_set_path() const;

	// Load the tile set and fill every hash it can reach by Y turns. Runs once; later calls do nothing.
	void initialize_library();

	// Read every tile (and variant) from the .glb into tiles[], replacing what was there.
	void load_tiles();
	// Debug: one row per modelled tile with every hash it fills, labelled, as child nodes.
	void build_showcase();
	// Fill empty entries of every level by turning each loaded tile about Y; returns how many M a
	// entries were filled.
	int fill_by_y_turns();
	// Print which drawable hashes still have no tile.
	void report_missing() const;

	// The tile for a cell hash (mesh is null if the set has none).
	const MCTile &get_tile(uint8_t p_hash) const { return tiles[p_hash]; }
	// The tile of a level for a cell hash: 'l' ground, 'm' main (get_tile), 'h' roof.
	const MCTile &get_level_tile(
			char p_height,
			uint8_t p_hash
	) const;
	// The tile a cell of this level draws: the level's tile, or the M a tile when the level has none
	// for this hash (a hand-made shape), so nothing is left as a hole.
	const MCTile &get_tile_for_level(
			char p_height,
			uint8_t p_hash
	) const;
	// Does the set have any tile of this level ('l', 'm', 'h')?
	bool has_level(char p_height) const;
	// {modelled tile hash as "00001111": how many hashes it draws}, for the stats panel.
	Dictionary get_hashes_per_tile() const;
	// Hashes of the modelled tiles, in load order.
	std::vector<uint8_t> get_loaded_hashes() const { return loaded_hashes; }
};

} // namespace godot

#endif // MC_LIBRARY_H
