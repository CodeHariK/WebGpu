// MCGrid: heightmap mode — Animal Crossing / Link's Awakening style terrain on the same grid.
//
// In heightmap mode every corner column (x, z) has one height h: its corners y < h are solid and
// the ones above are empty, so the terrain can never overhang. The heights are the source of truth
// for editing; the chunks' corner bits are kept in step (they are what the chunk builder, the
// levels, save / load and the debug cubes read), so everything else in MCGrid works unchanged.
//
//   set_corner (any caller: the editor's clicks, scripts) becomes a column edit:
//     fill a corner  -> its column rises to just above it  (left click on a top face: one tier up;
//                       on a side face: the neighbour column up to that height)
//     empty a corner -> its column drops to just below it   (right click: cut down to the clicked one)
//   Switching the mode on (or loading a save in it) flattens any overhang: each column keeps the
//   solid run up from its floor.
//   randomize_heights: an island from noise — tiers rising toward the middle, the rim at the floor.
//
// Outer columns (x or z on the grid's edge) are locked to the floor, like the corner mode's shell
// (`lock_rim`); with lock_rim off they are editable and the terrain ends open at the grid's edge,
// like MCHeightmap's.
//   load_heights: MCHeightmap's data (one byte per column, solid up to and including that corner)
//   → the grid, with MCHeightmap's M-roof roll, so its terrain rebuilds cell for cell.
#include "marching_cubes/grid/mc_grid.h"
#include "marching_cubes/library/mc_library.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <godot_cpp/classes/fast_noise_lite.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

namespace {
const float ISLAND_EDGE = 0.92f; // share of the half-width where the island has sunk to the floor
const float NOISE_SHARE = 0.45f; // how much noise bends the tiers (0: a perfect cone of tiers)
const float ROOF_M_CHANCE = 0.5f; // roof regions drawn with M tiles (as MCHeightmap did)
} // namespace

// Corner columns per side (one more than the cells).
Vector2i MCGrid::_column_counts() const {
	Vector3i corners = _dims().max_corner();
	return Vector2i(corners.x + 1, corners.z + 1);
}

// Index of column (x, z) in column_heights; -1 outside the grid.
int MCGrid::_column_index(
		int p_x,
		int p_z
) const {
	Vector2i columns = _column_counts();
	if (p_x < 0 || p_z < 0 || p_x >= columns.x || p_z >= columns.y) {
		return -1;
	}
	return p_x + (p_z * columns.x);
}

// The lowest a column may go (the floor), and the highest (the top layer stays empty).
int MCGrid::_min_column_height() const { return solid_floor ? 1 : 0; }

int MCGrid::_max_column_height() const { return _dims().max_corner().y; }

// Outer columns are locked to the floor (unless lock_rim is off).
bool MCGrid::_is_edge_column(
		int p_x,
		int p_z
) const {
	if (!lock_rim) {
		return false;
	}
	Vector2i columns = _column_counts();
	return p_x == 0 || p_z == 0 || p_x == columns.x - 1 || p_z == columns.y - 1;
}

// Heightmap mode on / off. Turning it on flattens overhangs on a live grid and rebuilds it.
void MCGrid::set_heightmap_mode(bool p_on) {
	if (heightmap_mode == p_on) {
		return;
	}
	heightmap_mode = p_on;
	if (heightmap_mode && !chunks.empty()) {
		_heights_from_corners();
		refresh_grid();
	}
}

// Every column's height from its corners: the solid run up from the floor; corners above it that
// were solid (overhangs, floating bits) are emptied so the corners match the heights.
void MCGrid::_heights_from_corners() {
	Vector2i columns = _column_counts();
	int top = _max_column_height();
	column_heights.assign(static_cast<size_t>(columns.x) * columns.y, 0);
	for (int z = 0; z < columns.y; z++) {
		for (int x = 0; x < columns.x; x++) {
			int height = 0;
			while (height < top && is_corner_active(Vector3i(x, height, z))) {
				height++;
			}
			if (_is_edge_column(x, z)) {
				height = _min_column_height();
			}
			height = std::clamp(height, _min_column_height(), top);
			column_heights[static_cast<size_t>(_column_index(x, z))] = static_cast<uint8_t>(height);
			_write_column(x, z, height);
		}
	}
}

// Write column (x, z)'s corners for height p_height (no checks, no rebuild).
void MCGrid::_write_column(
		int p_x,
		int p_z,
		int p_height
) {
	int top = _max_column_height();
	for (int y = 0; y <= top; y++) {
		_write_corner(Vector3i(p_x, y, p_z), y < p_height);
	}
}

// Height of column (x, z): its corners y < height are solid. -1 outside the grid or when not in
// heightmap mode.
int MCGrid::get_column_height(const Vector2i &p_column) const {
	int index = _column_index(p_column.x, p_column.y);
	if (!heightmap_mode || index < 0 || static_cast<size_t>(index) >= column_heights.size()) {
		return -1;
	}
	return column_heights[static_cast<size_t>(index)];
}

// Set column (x, z) to p_height (clamped between the floor and the empty top layer; outer columns
// are locked). Marks the cells that changed dirty and queues one rebuild.
void MCGrid::set_column_height(
		const Vector2i &p_column,
		int p_height
) {
	int index = _column_index(p_column.x, p_column.y);
	if (!heightmap_mode || index < 0 || static_cast<size_t>(index) >= column_heights.size()) {
		return;
	}
	if (_is_edge_column(p_column.x, p_column.y)) {
		return; // the rim stays at the floor
	}
	int old_height = column_heights[static_cast<size_t>(index)];
	int new_height = std::clamp(p_height, _min_column_height(), _max_column_height());
	if (new_height == old_height) {
		return;
	}
	column_heights[static_cast<size_t>(index)] = static_cast<uint8_t>(new_height);
	int low = std::min(old_height, new_height); // corners low .. high - 1 flipped
	int high = std::max(old_height, new_height);
	for (int y = low; y < high; y++) {
		_write_corner(Vector3i(p_column.x, y, p_column.y), y < new_height);
	}
	// A corner touches the cells below and above it, in the 4 cell columns around the corner column.
	for (int cy = low - 1; cy < high; cy++) {
		for (int cz = p_column.y - 1; cz <= p_column.y; cz++) {
			for (int cx = p_column.x - 1; cx <= p_column.x; cx++) {
				_mark_cell_dirty(cx, cy, cz);
			}
		}
	}
	_queue_rebuild();
	// The debug cubes of the changed corners and of their neighbours (which may now be exposed).
	for (int y = low - 1; y <= high; y++) {
		_update_debug_corner(p_column.x, y, p_column.y);
		_update_debug_corner(p_column.x - 1, y, p_column.y);
		_update_debug_corner(p_column.x + 1, y, p_column.y);
		_update_debug_corner(p_column.x, y, p_column.y - 1);
		_update_debug_corner(p_column.x, y, p_column.y + 1);
	}
}

// set_corner in heightmap mode: fill -> the column rises to just above the corner; empty -> the
// column drops to just below it (never lower, never higher than asked).
void MCGrid::_set_corner_as_height(
		const Vector3i &p_grid_pos,
		bool p_active
) {
	int current = get_column_height(Vector2i(p_grid_pos.x, p_grid_pos.z));
	if (current < 0) {
		return;
	}
	if (p_active) {
		set_column_height(Vector2i(p_grid_pos.x, p_grid_pos.z), std::max(current, p_grid_pos.y + 1));
	} else {
		set_column_height(Vector2i(p_grid_pos.x, p_grid_pos.z), std::min(current, p_grid_pos.y));
	}
}

// Outer columns locked to the floor (default) or editable (the terrain ends open at the edge).
void MCGrid::set_lock_rim(bool p_lock) {
	lock_rim = p_lock;
	if (heightmap_mode && !chunks.empty()) {
		_heights_from_corners();
		refresh_grid();
	}
}

// MCHeightmap's heightmap_data (p_size columns, x fastest; a column is solid up to and including
// its value) into the grid: the grid's corner columns must be exactly p_size (rim open) or
// p_size + 2 (rim locked, the data inside it). M roofs are rolled with MCHeightmap's seed (FNV-1a of
// the bytes), so the same data draws the same tiles. Returns "" or what was wrong.
String MCGrid::load_heights(
		const PackedByteArray &p_data,
		const Vector2i &p_size
) {
	if (!tile_library || chunks.empty()) {
		return "no tile library / grid";
	}
	if (p_size.x < 1 || p_size.y < 1 || p_data.size() < p_size.x * p_size.y) {
		return "heights: need size.x * size.y bytes";
	}
	Vector2i columns = _column_counts();
	int offset = lock_rim ? 1 : 0;
	if (columns != p_size + Vector2i(offset * 2, offset * 2)) {
		return "heights " + String::num_int64(p_size.x) + "x" + String::num_int64(p_size.y) + " don't fit " +
				String::num_int64(columns.x) + "x" + String::num_int64(columns.y) + " corner columns" +
				(lock_rim ? " (rim locked: needs 2 more)" : "");
	}
	if (!heightmap_mode) {
		heightmap_mode = true;
	}
	const uint8_t *data = p_data.ptr();
	int clipped = 0;
	for (int z = 0; z < columns.y; z++) {
		for (int x = 0; x < columns.x; x++) {
			int height = _min_column_height();
			int dx = x - offset;
			int dz = z - offset;
			if (dx >= 0 && dz >= 0 && dx < p_size.x && dz < p_size.y) {
				int wanted = data[dx + (dz * p_size.x)] + 1; // solid up to and including the value
				height = std::clamp(wanted, _min_column_height(), _max_column_height());
				clipped += height != wanted ? 1 : 0;
			}
			column_heights[static_cast<size_t>(_column_index(x, z))] = static_cast<uint8_t>(height);
			_write_column(x, z, height);
		}
	}
	uint32_t seed = 2166136261u; // FNV-1a of the heights, as MCHeightmap seeds its M roofs
	for (int i = 0; i < p_data.size(); i++) {
		seed = (seed ^ data[i]) * 16777619u;
	}
	_roll_roof_m(seed);
	refresh_grid();
	return clipped > 0 ? "clipped " + String::num_int64(clipped) + " columns to the grid's height" : String();
}

// The M roofs for the current corners (MCHeightmap's rule: each H roof region M with ROOF_M_CHANCE
// when M has a tile for all of it), seeded.
void MCGrid::_roll_roof_m(uint32_t p_seed) {
	mc::CellBlock block = _cell_block();
	std::vector<uint8_t> corners(static_cast<size_t>(block.corner_count()));
	for (int z = 0; z <= block.z; z++) {
		for (int y = 0; y <= block.y; y++) {
			for (int x = 0; x <= block.x; x++) {
				corners[static_cast<size_t>(block.corner_index(x, y, z))] = is_corner_active(Vector3i(x, y, z)) ? 1 : 0;
			}
		}
	}
	std::array<bool, 256> m_shapes = {};
	for (int h = 1; h < 255; h++) {
		m_shapes[static_cast<size_t>(h)] = tile_library->get_tile(static_cast<uint8_t>(h)).mesh.is_valid();
	}
	roof_m = mc::choose_roof_m(
			block, corners, tile_library->has_level('l'), tile_library->has_level('h'), m_shapes, ROOF_M_CHANCE,
			p_seed
	);
}

// An island from noise for p_seed: tiers rising toward the middle (bent by noise), the rim at the
// floor; then the M roofs are rolled and the whole grid rebuilt. Returns a summary.
String MCGrid::randomize_heights(int p_seed) {
	if (!tile_library || chunks.empty()) {
		return "no tile library / grid";
	}
	if (!heightmap_mode) {
		set_heightmap_mode(true);
	}
	Ref<FastNoiseLite> noise;
	noise.instantiate();
	noise->set_seed(p_seed);
	noise->set_frequency(0.12f);
	noise->set_fractal_octaves(3);

	Vector2i columns = _column_counts();
	int floor_height = _min_column_height();
	int tiers = _max_column_height() - floor_height; // heights above the floor we can use
	Vector2 middle(static_cast<float>(columns.x - 1) * 0.5f, static_cast<float>(columns.y - 1) * 0.5f);
	int counts[256] = {};
	for (int z = 0; z < columns.y; z++) {
		for (int x = 0; x < columns.x; x++) {
			int height = floor_height;
			if (!_is_edge_column(x, z)) {
				Vector2 offset = (Vector2(static_cast<float>(x), static_cast<float>(z)) - middle) / middle;
				float rise = std::clamp(1.0f - offset.length() / ISLAND_EDGE, 0.0f, 1.0f); // 1 middle .. 0 rim
				float bend = noise->get_noise_2d(static_cast<float>(x), static_cast<float>(z)) * NOISE_SHARE;
				float shaped = std::clamp(rise + bend * rise, 0.0f, 1.0f);
				height = floor_height + static_cast<int>(std::floor(shaped * static_cast<float>(tiers) + 0.5f));
				height = std::clamp(height, floor_height, _max_column_height());
			}
			column_heights[static_cast<size_t>(_column_index(x, z))] = static_cast<uint8_t>(height);
			_write_column(x, z, height);
			counts[height]++;
		}
	}

	_roll_roof_m(static_cast<uint32_t>(p_seed)); // seeded, so a seed always gives the same island
	decor_seed = p_seed;
	refresh_grid();

	String summary = "island " + String::num_int64(p_seed) + ": heights";
	for (int h = 0; h <= _max_column_height(); h++) {
		if (counts[h] > 0) {
			summary += " " + String::num_int64(h) + "x" + String::num_int64(counts[h]);
		}
	}
	UtilityFunctions::print("MCGrid Randomize heights: ", summary);
	return summary;
}

} // namespace godot
