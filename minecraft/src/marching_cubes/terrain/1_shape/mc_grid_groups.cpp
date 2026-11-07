// MCGrid: which tile group (a, b, c … : the tag group letter, MCTileLibrary::get_group_letters)
// each cell draws.
//
// No painting: each connected solid region above the floor — solid corners with y >= 1 joined
// across cube edges (6 neighbours) — rolls one group from its first corner (scan order) and the
// grid's decor seed (Randomize sets it), so a whole island, building or rock is one style and the
// same grid always gets the same groups. The floor (y = 0) joins nothing: it is one solid sheet
// under everything, so through it the whole map would be one region. A cell takes the group of the
// region of its first solid corner above the floor (in corner order); a cell with only floor
// corners solid takes the floor's own group. Joining two regions with an edit makes them one
// region with one group; growing a region past its first corner (toward lower z, y, x) rolls it
// again.
// A group that has no tile for a shape draws group a's (MCTileLibrary::get_group_tile).
// Recomputed with the levels (_update_levels: after every edit and full rebuild); cells whose group
// changed are marked dirty.
#include "marching_cubes/terrain/mc_grid.h"
#include "marching_cubes/library/mc_library.h"
#include <cstdint>
#include <vector>

namespace godot {

namespace {
// The floor's roll key: corner index 0 (corner (0, 0, 0), on the floor), which no region above the
// floor can have as its first corner.
const uint32_t FLOOR_KEY = 0;

// Corner offsets of a cell, in hash bit order (core/mc_corners.h): 0..3 bottom, 4..7 top.
const int CELL_CORNERS[8][3] = { { 0, 0, 1 }, { 1, 0, 1 }, { 1, 0, 0 }, { 0, 0, 0 },
	{ 0, 1, 1 }, { 1, 1, 1 }, { 1, 1, 0 }, { 0, 1, 0 } };

// A stable mix of a region's first corner and the seed.
uint32_t region_roll(
		uint32_t p_corner_index,
		uint32_t p_seed
) {
	uint32_t h = p_corner_index * 2654435761u ^ (p_seed + 0x9e3779b9u + (p_corner_index << 6) + (p_corner_index >> 2));
	h ^= h >> 16;
	h *= 0x7feb352du;
	h ^= h >> 15;
	h *= 0x846ca68bu;
	h ^= h >> 16;
	return h;
}
} // namespace

// The group of a cell ('a' outside the grid or before the first update).
char MCGrid::_group_at(const Vector3i &p_cell) const {
	mc::CellBlock block = _cell_block();
	if (p_cell.x < 0 || p_cell.y < 0 || p_cell.z < 0 || p_cell.x >= block.x || p_cell.y >= block.y ||
		p_cell.z >= block.z || cell_groups.size() != static_cast<size_t>(block.cell_count())) {
		return 'a';
	}
	return static_cast<char>(cell_groups[static_cast<size_t>(block.cell_index(p_cell.x, p_cell.y, p_cell.z))]);
}

// The group letter of a cell ("" outside the grid).
String MCGrid::get_cell_group(const Vector3i &p_pos) const {
	mc::CellBlock block = _cell_block();
	if (p_pos.x < 0 || p_pos.y < 0 || p_pos.z < 0 || p_pos.x >= block.x || p_pos.y >= block.y || p_pos.z >= block.z) {
		return String();
	}
	return String::chr(_group_at(p_pos));
}

// Regions of the solid corners above the floor (flood fill), one group rolled per region (and one
// for the floor), then each cell's group.
void MCGrid::_update_groups(const std::vector<uint8_t> &p_corners) {
	mc::CellBlock block = _cell_block();
	std::vector<char> letters;
	if (tile_library) {
		letters = tile_library->get_group_letters();
	}
	std::vector<uint8_t> groups(static_cast<size_t>(block.cell_count()), 'a');
	if (letters.size() > 1) {
		// 1. Region of every solid corner above the floor: the index of its first corner (scan order);
		// -1 = empty or on the floor.
		std::vector<int32_t> region(p_corners.size(), -1);
		std::vector<int32_t> stack;
		for (int z = 0; z <= block.z; z++) {
			for (int y = 1; y <= block.y; y++) {
				for (int x = 0; x <= block.x; x++) {
					int32_t start = block.corner_index(x, y, z);
					if (!p_corners[static_cast<size_t>(start)] || region[static_cast<size_t>(start)] >= 0) {
						continue;
					}
					region[static_cast<size_t>(start)] = start;
					stack.push_back(start);
					while (!stack.empty()) {
						int32_t current = stack.back();
						stack.pop_back();
						int cx = current % (block.x + 1);
						int cy = (current / (block.x + 1)) % (block.y + 1);
						int cz = current / ((block.x + 1) * (block.y + 1));
						const int steps[6][3] = { { 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 } };
						for (const auto &step : steps) {
							int nx = cx + step[0];
							int ny = cy + step[1];
							int nz = cz + step[2];
							if (nx < 0 || ny < 1 || nz < 0 || nx > block.x || ny > block.y || nz > block.z) {
								continue; // outside the grid, or the floor
							}
							int32_t next = block.corner_index(nx, ny, nz);
							if (p_corners[static_cast<size_t>(next)] && region[static_cast<size_t>(next)] < 0) {
								region[static_cast<size_t>(next)] = start;
								stack.push_back(next);
							}
						}
					}
				}
			}
		}
		// 2. Each cell: the group of its first solid corner's region above the floor; else, when a
		// floor corner of it is solid, the floor's group.
		for (int z = 0; z < block.z; z++) {
			for (int y = 0; y < block.y; y++) {
				for (int x = 0; x < block.x; x++) {
					int64_t key = -1;
					bool floor_solid = false;
					for (const auto &offset : CELL_CORNERS) {
						int32_t corner = block.corner_index(x + offset[0], y + offset[1], z + offset[2]);
						if (region[static_cast<size_t>(corner)] >= 0) {
							key = region[static_cast<size_t>(corner)];
							break;
						}
						floor_solid = floor_solid || (y + offset[1] == 0 && p_corners[static_cast<size_t>(corner)]);
					}
					if (key < 0 && floor_solid) {
						key = FLOOR_KEY;
					}
					if (key < 0) {
						continue; // no solid corner: no tile, group a
					}
					uint32_t roll = region_roll(static_cast<uint32_t>(key), static_cast<uint32_t>(decor_seed));
					groups[static_cast<size_t>(block.cell_index(x, y, z))] =
							static_cast<uint8_t>(letters[roll % static_cast<uint32_t>(letters.size())]);
				}
			}
		}
	}
	if (groups.size() == cell_groups.size()) {
		for (int z = 0; z < block.z; z++) {
			for (int y = 0; y < block.y; y++) {
				for (int x = 0; x < block.x; x++) {
					size_t i = static_cast<size_t>(block.cell_index(x, y, z));
					if (groups[i] != cell_groups[i]) {
						_mark_cell_dirty(x, y, z);
					}
				}
			}
		}
	}
	cell_groups = std::move(groups);
}

} // namespace godot
