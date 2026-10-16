// Levels (L, M, H) of a block of cells, with the same rules as the Blender add-on's playground
// (mc_tile_tools/playground.py):
//   L  bottom layer only (M there when the set has no L tiles)
//   H  a roof cell above the bottom layer (top of its column: nothing solid on top); a roof region
//      joined to a non-H cell (solid corner on a shared side wall) is M instead, so H only sits
//      beside H
//   M  every other cell above the bottom layer. A column top can be M too: plan_levels makes each
//      roof region M or H at random (M only when M has a tile for every cell of it); the regions
//      chosen M are kept in roof_m, so assign_levels gives them M again after edits
//   up a column: L, then M, then H, nothing above an H
//
// assign_levels: the levels of corners that already exist (MCGrid after any edit).
// plan_levels:   random corners that follow the rules (MCGrid "Randomize"). Corners on the 4 side
//                faces and the top face are empty; the floor is random (or solid with solid_floor).
//                The bottom layer is solved (backtracking) so every bottom cell is an L shape, roof
//                cells spread to the side neighbours they are joined to (their top corners
//                cleared), and the best of many random layouts is kept: the column order kept,
//                then the fewest cells whose level has no tile.
//
// Plain data, no nodes. Corner order and axes as mc_corners.h (Godot: +Y up, +Z near).
#ifndef MC_LEVEL_PLAN_H
#define MC_LEVEL_PLAN_H

#include <array>
#include <cstdint>
#include <vector>

namespace godot::mc {

// Which shapes each level can draw: shapes[level][hash], level 0 = L, 1 = M, 2 = H.
using LevelShapes = std::array<std::array<bool, 256>, 3>;

inline constexpr char LEVEL_LETTERS[3] = { 'l', 'm', 'h' };
inline constexpr uint8_t LEVEL_L = 0;
inline constexpr uint8_t LEVEL_M = 1;
inline constexpr uint8_t LEVEL_H = 2;

// Cells per axis; corners are one more per axis. Corner and cell arrays: x fastest, then y, then z.
struct CellBlock {
	int x = 1;
	int y = 1;
	int z = 1;

	int cell_count() const { return x * y * z; }
	int corner_count() const { return (x + 1) * (y + 1) * (z + 1); }
	int cell_index(
			int p_x,
			int p_y,
			int p_z
	) const {
		return p_x + x * (p_y + y * p_z);
	}
	int corner_index(
			int p_x,
			int p_y,
			int p_z
	) const {
		return p_x + (x + 1) * (p_y + (y + 1) * p_z);
	}
};

struct LevelPlanInput {
	CellBlock block;
	uint32_t seed = 1;
	int tries = 300; // random layouts planned; the best one is kept
	bool solid_floor = false; // floor corners (y = 0) solid instead of random
	float roof_m_chance = 0.5f; // chance that a roof region is M rather than H
	LevelShapes shapes = {};
};

struct LevelPlan {
	CellBlock block;
	std::vector<uint8_t> corners; // 1 = solid
	std::vector<uint8_t> hashes; // per cell
	std::vector<uint8_t> levels; // per cell: LEVEL_L, LEVEL_M or LEVEL_H
	std::vector<uint8_t> roof_m; // per cell: 1 = its roof region was chosen M
	int gaps = 0; // cells (not empty / full) whose level has no tile for their hash
	int column_breaks = 0; // cells breaking L -> M -> H up a column (0 unless no layout keeps it)
	bool bottom_solved = true; // false when no all-L bottom layer was found
};

// Levels of existing corners (corner_count() entries, 1 = solid); one entry per cell. p_roof_m
// (optional, one entry per cell): a roof region with any of these cells is M, not H.
std::vector<uint8_t> assign_levels(
		const CellBlock &p_block,
		const std::vector<uint8_t> &p_corners,
		bool p_have_l,
		bool p_have_h,
		const std::vector<uint8_t> *p_roof_m = nullptr
);

// The roof_m list for existing corners: each H roof region becomes M with p_chance (seeded), when
// M (p_m_shapes) has a tile for every cell of it. Pass it to assign_levels.
std::vector<uint8_t> choose_roof_m(
		const CellBlock &p_block,
		const std::vector<uint8_t> &p_corners,
		bool p_have_l,
		bool p_have_h,
		const std::array<bool, 256> &p_m_shapes,
		float p_chance,
		uint32_t p_seed
);

// Plan a block: the best of p_input.tries random layouts.
LevelPlan plan_levels(const LevelPlanInput &p_input);

} // namespace godot::mc

#endif // MC_LEVEL_PLAN_H
