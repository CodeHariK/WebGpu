// MCGrid: marching squares mode — each layer of corners is its own sheet.
//
// The corners are stored and edited exactly as in corner mode (one corner per click, undo, save);
// only what a cell draws changes. A cell between corner layers y and y + 1 looks at its bottom four
// corners only (layer y) and draws them as a straight-sided piece one layer high: the hash with its
// top four corners set to the same as the bottom four (squares_hash). Those 16 "pillar" hashes are
// 6 shapes up to Y turns, 5 to model (empty draws nothing):
//
//   corner        1 corner      00010001
//   edge          2 side by side 00110011
//   diagonal      2 opposite    01010101   (one shape: the two corners joined)
//   inner corner  3 corners     01110111
//   full          4 corners     11111111
//
// The tile library fills the other turns as for any set (mc_library_fill.cpp). The pieces are
// drawn half a cell lower than other tiles (_tile_offset): the sheet of corner layer y spans
// y - 0.5 .. y + 0.5, centred on its corners, so its cap is at y + 0.5 like the heightmap tiles'
// ground (and the corner's hover cube top). Each piece carries its own top cap, always drawn
// (hidden inside the solid where the layer above covers it), so a layer never depends on the one
// above. Layers stand on their own, so overhangs and floating
// sheets are fine. Tile set: res://assets/Marching/Squares.glb (blender/mc_tiles/scripts/
// squares_tiles.py); scene: scene/marching/squares.tscn.
#include "marching_cubes/library/mc_library.h"
#include "marching_cubes/terrain/mc_grid.h"
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

// Squares mode on / off: every cell's drawn hash changes, so a live grid is rebuilt. Heightmap
// mode goes off first (the corners already match: nothing to convert).
void MCGrid::set_squares_mode(bool p_on) {
	if (squares_mode == p_on) {
		return;
	}
	if (p_on && heightmap_mode) {
		set_heightmap_mode(false);
	}
	squares_mode = p_on;
	clear_history(); // the edits were recorded in the other mode
	if (!chunks.empty()) {
		refresh_grid();
	}
}

// One of the three modes: heightmap and squares are never on together.
void MCGrid::set_terrain_mode(int p_mode) {
	switch (p_mode) {
		case TERRAIN_HEIGHTMAP:
			set_heightmap_mode(true); // turns squares mode off
			break;
		case TERRAIN_SQUARES:
			set_squares_mode(true); // turns heightmap mode off
			break;
		default:
			set_heightmap_mode(false);
			set_squares_mode(false);
			break;
	}
}

int MCGrid::get_terrain_mode() const {
	if (heightmap_mode) {
		return TERRAIN_HEIGHTMAP;
	}
	return squares_mode ? TERRAIN_SQUARES : TERRAIN_CORNERS;
}

const char *MCGrid::terrain_mode_name(int p_mode) {
	switch (p_mode) {
		case TERRAIN_HEIGHTMAP:
			return "heightmap";
		case TERRAIN_SQUARES:
			return "squares";
		default:
			return "corners";
	}
}

// The heightmap island's tiers written as corners: each column solid up to its height. The outer
// columns stay at the floor (the corner shell keeps the sides empty). Turns squares mode on.
String MCGrid::randomize_squares(int p_seed) {
	if (!tile_library || chunks.empty()) {
		return "no tile library / grid";
	}
	if (!squares_mode) {
		set_squares_mode(true);
	}
	std::vector<int> heights = _island_heights(p_seed, true);
	Vector2i columns = _column_counts();
	int counts[256] = {};
	for (int z = 0; z < columns.y; z++) {
		for (int x = 0; x < columns.x; x++) {
			int height = heights[static_cast<size_t>(_column_index(x, z))];
			_write_column(x, z, height); // records the corners (undo)
			counts[height]++;
		}
	}
	decor_seed = p_seed;
	refresh_grid();

	String summary = "squares island " + String::num_int64(p_seed) + ": heights";
	for (int h = 0; h <= _max_column_height(); h++) {
		if (counts[h] > 0) {
			summary += " " + String::num_int64(h) + "x" + String::num_int64(counts[h]);
		}
	}
	UtilityFunctions::print("MCGrid Randomize squares: ", summary);
	return summary;
}

} // namespace godot
