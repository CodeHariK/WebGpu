// Levels of a block of cells: assign_levels / plan_levels (rules in mc_level_plan.h).
#include "marching_cubes/core/mc_level_plan.h"
#include <algorithm>
#include <random>

namespace godot::mc {

namespace {

// Corner i of a cell (x, y, z), as mc_corners.h: c0 (x, y, z+1) ... c3 (x, y, z), c4..c7 one up.
constexpr int OFFSETS[8][3] = {
	{ 0, 0, 1 }, { 1, 0, 1 }, { 1, 0, 0 }, { 0, 0, 0 }, { 0, 1, 1 }, { 1, 1, 1 }, { 1, 1, 0 }, { 0, 1, 0 },
};
constexpr int TOP_BITS[4] = { 4, 5, 6, 7 };
constexpr int SIDES[4][3] = { { 1, 0, 0 }, { -1, 0, 0 }, { 0, 0, 1 }, { 0, 0, -1 } };
constexpr uint8_t UNSET = 2;
constexpr int SOLVE_BUDGET = 20000;

struct Cell {
	int x = 0;
	int y = 0;
	int z = 0;
};

Cell step(
		const Cell &p_cell,
		const int p_dir[3]
) {
	return { p_cell.x + p_dir[0], p_cell.y + p_dir[1], p_cell.z + p_dir[2] };
}

// A corner grid (0, 1 or UNSET while planning) and the cell queries the rules need.
struct Layout {
	CellBlock block;
	std::vector<uint8_t> grid;

	explicit Layout(const CellBlock &p_block) :
			block(p_block), grid(static_cast<size_t>(p_block.corner_count()), UNSET) {}

	// Grid index of corner p_bit of a cell.
	int key(
			const Cell &p_cell,
			int p_bit
	) const {
		return block.corner_index(
				p_cell.x + OFFSETS[p_bit][0], p_cell.y + OFFSETS[p_bit][1], p_cell.z + OFFSETS[p_bit][2]
		);
	}

	uint8_t hash(const Cell &p_cell) const {
		uint8_t h = 0;
		for (int bit = 0; bit < 8; bit++) {
			if (grid[static_cast<size_t>(key(p_cell, bit))] == 1) {
				h |= static_cast<uint8_t>(1 << bit);
			}
		}
		return h;
	}

	bool inside(const Cell &p_cell) const {
		return p_cell.x >= 0 && p_cell.y >= 0 && p_cell.z >= 0 && p_cell.x < block.x && p_cell.y < block.y &&
				p_cell.z < block.z;
	}

	size_t cell(const Cell &p_cell) const { return static_cast<size_t>(block.cell_index(p_cell.x, p_cell.y, p_cell.z)); }

	// Something solid, nothing solid on top.
	bool roof(const Cell &p_cell) const {
		uint8_t h = hash(p_cell);
		return h != 0 && (h & 0xF0) == 0;
	}

	// A solid corner on the wall two side neighbours share.
	bool joined(
			const Cell &p_a,
			const Cell &p_b
	) const {
		for (int i = 0; i < 8; i++) {
			int k = key(p_a, i);
			if (grid[static_cast<size_t>(k)] != 1) {
				continue;
			}
			for (int j = 0; j < 8; j++) {
				if (key(p_b, j) == k) {
					return true;
				}
			}
		}
		return false;
	}
};

// Call p_fn(cell) for every cell with y >= p_from_y.
template <typename F>
void for_cells(
		const CellBlock &p_block,
		int p_from_y,
		F p_fn
) {
	for (int z = 0; z < p_block.z; z++) {
		for (int y = p_from_y; y < p_block.y; y++) {
			for (int x = 0; x < p_block.x; x++) {
				p_fn(Cell{ x, y, z });
			}
		}
	}
}

// Corners on the 4 side faces and the top face are empty; the floor solid with p_solid_floor
// (then the floor's side edges too, as MCGrid's boundary), else random.
void set_faces(
		Layout &p_layout,
		bool p_solid_floor
) {
	const CellBlock &b = p_layout.block;
	for (int z = 0; z <= b.z; z++) {
		for (int y = 0; y <= b.y; y++) {
			for (int x = 0; x <= b.x; x++) {
				uint8_t &corner = p_layout.grid[static_cast<size_t>(b.corner_index(x, y, z))];
				if (y == 0 && p_solid_floor) {
					corner = 1;
				} else if (x == 0 || x == b.x || z == 0 || z == b.z || y == b.y) {
					corner = 0;
				}
			}
		}
	}
}

// Backtracking: fill the unset corners of the cells (random order) so every cell's hash is ok.
// On failure their free corners are left unset (filled at random later).
bool solve_cells(
		Layout &p_layout,
		const std::vector<Cell> &p_cells,
		size_t p_index,
		const std::array<bool, 256> &p_ok,
		std::mt19937 &p_rng,
		int &p_budget
) {
	if (p_index == p_cells.size()) {
		return true;
	}
	if (--p_budget < 0) {
		return false;
	}
	const Cell &cell = p_cells[p_index];
	std::vector<int> free;
	for (int bit = 0; bit < 8; bit++) {
		int k = p_layout.key(cell, bit);
		if (p_layout.grid[static_cast<size_t>(k)] == UNSET && std::find(free.begin(), free.end(), k) == free.end()) {
			free.push_back(k);
		}
	}
	std::vector<int> options(static_cast<size_t>(1) << free.size());
	for (size_t i = 0; i < options.size(); i++) {
		options[i] = static_cast<int>(i);
	}
	std::shuffle(options.begin(), options.end(), p_rng);
	for (int option : options) {
		for (size_t b = 0; b < free.size(); b++) {
			p_layout.grid[static_cast<size_t>(free[b])] = static_cast<uint8_t>((option >> b) & 1);
		}
		if (p_ok[p_layout.hash(cell)] && solve_cells(p_layout, p_cells, p_index + 1, p_ok, p_rng, p_budget)) {
			return true;
		}
	}
	for (int k : free) {
		p_layout.grid[static_cast<size_t>(k)] = UNSET;
	}
	return false;
}

// Every bottom cell an L shape (or empty / full).
bool solve_bottom(
		Layout &p_layout,
		const std::array<bool, 256> &p_l_shapes,
		std::mt19937 &p_rng
) {
	std::array<bool, 256> ok = p_l_shapes;
	ok[0] = true;
	ok[255] = true;
	std::vector<Cell> bottom;
	for (int z = 0; z < p_layout.block.z; z++) {
		for (int x = 0; x < p_layout.block.x; x++) {
			bottom.push_back({ x, 0, z });
		}
	}
	int budget = SOLVE_BUDGET;
	return solve_cells(p_layout, bottom, 0, ok, p_rng, budget);
}

void fill_random(
		Layout &p_layout,
		std::mt19937 &p_rng
) {
	for (uint8_t &corner : p_layout.grid) {
		if (corner == UNSET) {
			corner = static_cast<uint8_t>(p_rng() & 1u);
		}
	}
}

// Every roof cell above the bottom layer makes the side neighbours joined to it roofs too, by
// clearing their top corners. Repeats until nothing changes.
void spread_roofs(Layout &p_layout) {
	bool changed = true;
	while (changed) {
		changed = false;
		for_cells(p_layout.block, 1, [&](const Cell &cell) {
			if (!p_layout.roof(cell)) {
				return;
			}
			for (const auto &dir : SIDES) {
				Cell other = step(cell, dir);
				if (!p_layout.inside(other) || p_layout.roof(other) || !p_layout.joined(cell, other)) {
					continue;
				}
				for (int bit : TOP_BITS) {
					p_layout.grid[static_cast<size_t>(p_layout.key(other, bit))] = 0;
				}
				changed = true;
			}
		});
	}
}

// Roof cells above the bottom layer that get H; a region still joined to a non-H cell stays M
// (a cell in p_roof_m counts as non-H, so its whole region is M).
std::vector<uint8_t> find_h_cells(
		const Layout &p_layout,
		const std::vector<uint8_t> *p_roof_m
) {
	std::vector<uint8_t> h_cells(static_cast<size_t>(p_layout.block.cell_count()), 0);
	bool use_m = p_roof_m && p_roof_m->size() == h_cells.size();
	for_cells(p_layout.block, 1, [&](const Cell &cell) {
		size_t i = p_layout.cell(cell);
		h_cells[i] = p_layout.roof(cell) && !(use_m && (*p_roof_m)[i]);
	});
	bool changed = true;
	while (changed) {
		changed = false;
		for_cells(p_layout.block, 1, [&](const Cell &cell) {
			if (!h_cells[p_layout.cell(cell)]) {
				return;
			}
			for (const auto &dir : SIDES) {
				Cell other = step(cell, dir);
				if (p_layout.inside(other) && !h_cells[p_layout.cell(other)] && p_layout.joined(cell, other)) {
					h_cells[p_layout.cell(cell)] = 0;
					changed = true;
					return;
				}
			}
		});
	}
	return h_cells;
}

// Levels of every cell of a finished layout.
std::vector<uint8_t> levels_of(
		const Layout &p_layout,
		bool p_have_l,
		bool p_have_h,
		const std::vector<uint8_t> *p_roof_m = nullptr
) {
	std::vector<uint8_t> h_cells;
	if (p_have_h) {
		h_cells = find_h_cells(p_layout, p_roof_m);
	}
	std::vector<uint8_t> levels(static_cast<size_t>(p_layout.block.cell_count()), LEVEL_M);
	for_cells(p_layout.block, 0, [&](const Cell &cell) {
		size_t i = p_layout.cell(cell);
		if (cell.y == 0) {
			levels[i] = p_have_l ? LEVEL_L : LEVEL_M;
		} else if (p_have_h && h_cells[i]) {
			levels[i] = LEVEL_H;
		}
	});
	return levels;
}

// Each H roof region (cells joined side by side) becomes M with p_chance, when M has a tile for
// every cell of it. Returns the cells chosen M (one entry per cell).
std::vector<uint8_t> choose_m_roofs(
		const Layout &p_layout,
		const std::vector<uint8_t> &p_levels,
		const std::array<bool, 256> &p_m_shapes,
		float p_chance,
		std::mt19937 &p_rng
) {
	std::vector<uint8_t> roof_m(p_levels.size(), 0);
	std::vector<uint8_t> seen(p_levels.size(), 0);
	std::uniform_real_distribution<float> coin(0.0f, 1.0f);
	for_cells(p_layout.block, 1, [&](const Cell &start) {
		size_t s = p_layout.cell(start);
		if (seen[s] || p_levels[s] != LEVEL_H) {
			return;
		}
		std::vector<Cell> region = { start };
		seen[s] = 1;
		for (size_t r = 0; r < region.size(); r++) {
			for (const auto &dir : SIDES) {
				Cell other = step(region[r], dir);
				if (!p_layout.inside(other)) {
					continue;
				}
				size_t o = p_layout.cell(other);
				if (!seen[o] && p_levels[o] == LEVEL_H && p_layout.joined(region[r], other)) {
					seen[o] = 1;
					region.push_back(other);
				}
			}
		}
		bool m_draws_all = std::all_of(region.begin(), region.end(), [&](const Cell &c) { return p_m_shapes[p_layout.hash(c)]; });
		if (m_draws_all && coin(p_rng) < p_chance) {
			for (const Cell &c : region) {
				roof_m[p_layout.cell(c)] = 1;
			}
		}
	});
	return roof_m;
}

// Nothing above an H: every cell above a column's lowest H cell loses its top corners (so the
// column is empty above the roof). Returns how many corners were cleared.
int clear_above_h(
		Layout &p_layout,
		const std::vector<uint8_t> &p_levels
) {
	const CellBlock &b = p_layout.block;
	int cleared = 0;
	for (int z = 0; z < b.z; z++) {
		for (int x = 0; x < b.x; x++) {
			bool above_h = false;
			for (int y = 0; y < b.y; y++) {
				Cell cell = { x, y, z };
				if (above_h) {
					for (int bit : TOP_BITS) {
						uint8_t &corner = p_layout.grid[static_cast<size_t>(p_layout.key(cell, bit))];
						cleared += corner == 1 ? 1 : 0;
						corner = 0;
					}
				} else if (p_levels[p_layout.cell(cell)] == LEVEL_H && p_layout.hash(cell) != 0) {
					above_h = true;
				}
			}
		}
	}
	return cleared;
}

// Cells breaking the column order: a level lower than one below it, or anything above an H.
int count_column_breaks(const LevelPlan &p_plan) {
	const CellBlock &b = p_plan.block;
	int breaks = 0;
	for (int z = 0; z < b.z; z++) {
		for (int x = 0; x < b.x; x++) {
			int top = -1;
			for (int y = 0; y < b.y; y++) {
				size_t i = static_cast<size_t>(b.cell_index(x, y, z));
				if (p_plan.hashes[i] == 0) {
					continue;
				}
				int level = p_plan.levels[i];
				if (top == LEVEL_H || level < top) {
					breaks++;
				}
				top = std::max(top, level);
			}
		}
	}
	return breaks;
}

bool has_any(const std::array<bool, 256> &p_shapes) {
	return std::any_of(p_shapes.begin(), p_shapes.end(), [](bool b) { return b; });
}

// One random layout and its levels.
LevelPlan plan_once(
		const LevelPlanInput &p_input,
		std::mt19937 &p_rng
) {
	bool have_l = has_any(p_input.shapes[LEVEL_L]);
	bool have_h = has_any(p_input.shapes[LEVEL_H]);
	Layout layout(p_input.block);
	set_faces(layout, p_input.solid_floor);
	LevelPlan plan;
	plan.block = p_input.block;
	if (have_l) {
		plan.bottom_solved = solve_bottom(layout, p_input.shapes[LEVEL_L], p_rng);
	}
	fill_random(layout, p_rng);
	if (have_h) {
		// Roofs spread to joined neighbours; the column above each H is emptied; repeat until
		// nothing changes (clearing only removes solid corners, so it settles).
		const int max_passes = 16;
		for (int pass = 0; pass < max_passes; pass++) {
			spread_roofs(layout);
			if (clear_above_h(layout, levels_of(layout, have_l, have_h)) == 0) {
				break;
			}
		}
	}
	plan.levels = levels_of(layout, have_l, have_h);
	if (have_h) { // a column top can be M too: some roof regions become M
		plan.roof_m = choose_m_roofs(layout, plan.levels, p_input.shapes[LEVEL_M], p_input.roof_m_chance, p_rng);
		plan.levels = levels_of(layout, have_l, have_h, &plan.roof_m);
	}
	plan.hashes.resize(plan.levels.size());
	for_cells(plan.block, 0, [&](const Cell &cell) {
		size_t i = layout.cell(cell);
		uint8_t h = layout.hash(cell);
		plan.hashes[i] = h;
		if (h != 0 && h != 255 && !p_input.shapes[plan.levels[i]][h]) {
			plan.gaps++;
		}
	});
	plan.column_breaks = count_column_breaks(plan);
	plan.corners = std::move(layout.grid);
	return plan;
}

} // namespace

std::vector<uint8_t> assign_levels(
		const CellBlock &p_block,
		const std::vector<uint8_t> &p_corners,
		bool p_have_l,
		bool p_have_h,
		const std::vector<uint8_t> *p_roof_m
) {
	Layout layout(p_block);
	layout.grid = p_corners;
	return levels_of(layout, p_have_l, p_have_h, p_roof_m);
}

std::vector<uint8_t> choose_roof_m(
		const CellBlock &p_block,
		const std::vector<uint8_t> &p_corners,
		bool p_have_l,
		bool p_have_h,
		const std::array<bool, 256> &p_m_shapes,
		float p_chance,
		uint32_t p_seed
) {
	if (!p_have_h) {
		return {};
	}
	Layout layout(p_block);
	layout.grid = p_corners;
	std::mt19937 rng(p_seed);
	return choose_m_roofs(layout, levels_of(layout, p_have_l, p_have_h), p_m_shapes, p_chance, rng);
}

LevelPlan plan_levels(const LevelPlanInput &p_input) {
	LevelPlan best;
	bool have_best = false;
	int tries = std::max(1, p_input.tries);
	for (int attempt = 0; attempt < tries; attempt++) {
		std::mt19937 rng(p_input.seed * 7919u + static_cast<uint32_t>(attempt));
		LevelPlan plan = plan_once(p_input, rng);
		bool better = !have_best || plan.column_breaks < best.column_breaks ||
				(plan.column_breaks == best.column_breaks && plan.gaps < best.gaps);
		if (better) {
			best = std::move(plan);
			have_best = true;
		}
		if (best.column_breaks == 0 && best.gaps == 0) {
			break;
		}
	}
	return best;
}

} // namespace godot::mc
