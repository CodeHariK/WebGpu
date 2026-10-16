// MCGrid: each cell's level (L / M / H) and Randomize, with the rules of core/mc_level_plan.h.
//
// Levels are worked out from the corners, so hand edits follow the rules too: after every edit
// (flush_dirty_chunks) and every full rebuild (refresh_grid) the levels are recomputed and the
// chunks of cells whose level changed are rebuilt. Each cell draws the tile of its level from
// the library (MCTileLibrary::get_level_tile); a set with no L / H tiles is all M, as before.
#include "marching_cubes/grid/mc_grid.h"
#include "marching_cubes/library/mc_library.h"
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

// The grid's cells as a level block.
mc::CellBlock MCGrid::_cell_block() const {
	Vector3i cells = _dims().max_corner();
	return mc::CellBlock{ cells.x, cells.y, cells.z };
}

// The level of a cell; M outside the grid or before the first update.
uint8_t MCGrid::_level_at(const Vector3i &p_cell) const {
	mc::CellBlock block = _cell_block();
	if (p_cell.x < 0 || p_cell.y < 0 || p_cell.z < 0 || p_cell.x >= block.x || p_cell.y >= block.y ||
		p_cell.z >= block.z || cell_levels.size() != static_cast<size_t>(block.cell_count())) {
		return mc::LEVEL_M;
	}
	return cell_levels[static_cast<size_t>(block.cell_index(p_cell.x, p_cell.y, p_cell.z))];
}

// The level of a cell as 'l', 'm' or 'h' ("" outside the grid).
String MCGrid::get_cell_level(const Vector3i &p_pos) const {
	if (!_dims().contains_corner(p_pos) || p_pos.x == _dims().max_corner().x || p_pos.y == _dims().max_corner().y ||
		p_pos.z == _dims().max_corner().z) {
		return String();
	}
	return String::chr(mc::LEVEL_LETTERS[_level_at(p_pos)]);
}

// Recompute the levels from the corners; mark the cells whose level changed for a rebuild.
void MCGrid::_update_levels() {
	mc::CellBlock block = _cell_block();
	std::vector<uint8_t> corners(static_cast<size_t>(block.corner_count()));
	for (int z = 0; z <= block.z; z++) {
		for (int y = 0; y <= block.y; y++) {
			for (int x = 0; x <= block.x; x++) {
				corners[static_cast<size_t>(block.corner_index(x, y, z))] = is_corner_active(Vector3i(x, y, z)) ? 1 : 0;
			}
		}
	}
	bool have_l = tile_library && tile_library->has_level('l');
	bool have_h = tile_library && tile_library->has_level('h');
	std::vector<uint8_t> levels = mc::assign_levels(block, corners, have_l, have_h, &roof_m);
	if (levels.size() == cell_levels.size()) {
		for (int z = 0; z < block.z; z++) {
			for (int y = 0; y < block.y; y++) {
				for (int x = 0; x < block.x; x++) {
					size_t i = static_cast<size_t>(block.cell_index(x, y, z));
					if (levels[i] != cell_levels[i]) {
						_mark_cell_dirty(x, y, z);
					}
				}
			}
		}
	}
	cell_levels = std::move(levels);
}

// Floor corners locked solid, or editable (Randomize makes them random).
void MCGrid::set_solid_floor(bool p_solid) { solid_floor = p_solid; }

// New random corners that follow the level rules, then a full rebuild.
String MCGrid::randomize_levels(
		int p_seed,
		int p_tries
) {
	if (!tile_library || chunks.empty()) {
		return "no tile library / grid";
	}
	mc::LevelPlanInput input;
	input.block = _cell_block();
	input.seed = static_cast<uint32_t>(p_seed);
	input.tries = p_tries;
	input.solid_floor = solid_floor;
	for (int level = 0; level < 3; level++) {
		for (int h = 1; h < 255; h++) {
			input.shapes[static_cast<size_t>(level)][static_cast<size_t>(h)] =
					tile_library->get_level_tile(mc::LEVEL_LETTERS[level], static_cast<uint8_t>(h)).mesh.is_valid();
		}
	}
	last_plan = mc::plan_levels(input);
	roof_m = last_plan.roof_m; // M roof regions stay M when levels are recomputed

	const mc::CellBlock &block = last_plan.block;
	for (int z = 0; z <= block.z; z++) {
		for (int y = 0; y <= block.y; y++) {
			for (int x = 0; x <= block.x; x++) {
				_write_corner(Vector3i(x, y, z), last_plan.corners[static_cast<size_t>(block.corner_index(x, y, z))] != 0);
			}
		}
	}
	refresh_grid(); // levels, every chunk mesh and debug cube

	int counts[3] = { 0, 0, 0 };
	int cells = 0;
	for (size_t i = 0; i < last_plan.hashes.size(); i++) {
		if (last_plan.hashes[i] != 0 && last_plan.hashes[i] != 255) {
			counts[last_plan.levels[i]]++;
			cells++;
		}
	}
	String summary = "seed " + String::num_int64(p_seed) + ": " + String::num_int64(cells) + " cells, L " +
			String::num_int64(counts[0]) + " M " + String::num_int64(counts[1]) + " H " + String::num_int64(counts[2]) +
			", " + String::num_int64(last_plan.gaps) + " missing";
	if (!last_plan.bottom_solved) {
		summary += " (no all-L bottom layer fits)";
	}
	UtilityFunctions::print("MCGrid Randomize: ", summary);
	return summary;
}

// The last Randomize as data (for scripts and tests).
Dictionary MCGrid::get_last_plan() const {
	PackedByteArray hashes;
	PackedByteArray levels;
	for (size_t i = 0; i < last_plan.hashes.size(); i++) {
		hashes.push_back(last_plan.hashes[i]);
		levels.push_back(last_plan.levels[i]);
	}
	Dictionary plan;
	plan["size"] = Vector3i(last_plan.block.x, last_plan.block.y, last_plan.block.z);
	plan["hashes"] = hashes;
	plan["levels"] = levels;
	plan["gaps"] = last_plan.gaps;
	plan["column_breaks"] = last_plan.column_breaks;
	plan["bottom_solved"] = last_plan.bottom_solved;
	return plan;
}

} // namespace godot
