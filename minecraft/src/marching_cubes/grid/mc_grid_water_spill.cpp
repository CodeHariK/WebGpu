// MCGrid: diagonal spills, for the water mesh only (water_spill_reach, Water tab slider).
//
// The pour (fill_water) runs left / right / front / back only, so a dry column that touches water
// only diagonally stays dry and shows as a gap at the water's edge. A spill wets it anyway, at mesh
// build time: where a wet column has a dry diagonal neighbour whose ground is not higher, and one
// of the two columns in between is lower than the wet column's ground (a low corner the water runs
// past), the water spills onto that neighbour. From there it spreads on over dry ground that is not
// higher, left / right / front / back, up to water_spill_reach columns in all (1: only the column it
// spills onto; 0: no spills). Each spilled column stands one layer deep (level = ground + 1).
//
// Spills are never stored: _rebuild_water swaps the spilled levels in while it builds the mesh and
// then puts the stored ones back, so pours, settle_water, saves and undo never see them (stored
// spill water would become a source and flood its whole tier on the next settle).
#include "marching_cubes/grid/mc_grid.h"
#include <algorithm>
#include <vector>

namespace godot {

namespace {
const Vector2i SPILL_STEPS[4] = { Vector2i(1, 0), Vector2i(-1, 0), Vector2i(0, 1), Vector2i(0, -1) };
const Vector2i SPILL_DIAGONALS[4] = { Vector2i(1, 1), Vector2i(-1, 1), Vector2i(1, -1), Vector2i(-1, -1) };
} // namespace

void MCGrid::set_water_spill_reach(int p_reach) {
	water_spill_reach = std::clamp(p_reach, 0, 8);
	if (!chunks.empty() && water_levels.size() == column_heights.size()) {
		_queue_water();
	}
}

// The water levels with the spills added (see the top of this file).
std::vector<uint8_t> MCGrid::_spilled_levels() const {
	std::vector<uint8_t> levels = water_levels;
	if (water_spill_reach <= 0 || levels.size() != column_heights.size()) {
		return levels;
	}
	Vector2i columns = _column_counts();
	auto inside = [&](const Vector2i &c) { return c.x >= 0 && c.y >= 0 && c.x < columns.x && c.y < columns.y; };
	auto ground = [&](const Vector2i &c) { return static_cast<int>(column_heights[static_cast<size_t>(_column_index(c.x, c.y))]); };
	auto wet = [&](const Vector2i &c) {
		size_t i = static_cast<size_t>(_column_index(c.x, c.y));
		return levels[i] > column_heights[i];
	};
	int top = _max_column_height();
	// the spill landings: dry diagonal neighbours of stored water, past a low corner
	std::vector<std::pair<Vector2i, int>> queue; // column, steps taken (1 = the landing)
	for (int z = 0; z < columns.y; z++) {
		for (int x = 0; x < columns.x; x++) {
			Vector2i c(x, z);
			if (!_is_wet(_column_index(x, z))) {
				continue;
			}
			int g = ground(c);
			for (const Vector2i &d : SPILL_DIAGONALS) {
				Vector2i n = c + d;
				if (!inside(n) || wet(n) || ground(n) > g) {
					continue;
				}
				if (ground(Vector2i(x + d.x, z)) < g || ground(Vector2i(x, z + d.y)) < g) {
					levels[static_cast<size_t>(_column_index(n.x, n.y))] = static_cast<uint8_t>(std::min(ground(n) + 1, top));
					queue.push_back({ n, 1 });
				}
			}
		}
	}
	// spread on from the landings, up to water_spill_reach columns in all
	for (size_t head = 0; head < queue.size(); head++) {
		auto [c, steps] = queue[head];
		if (steps >= water_spill_reach) {
			continue;
		}
		for (const Vector2i &s : SPILL_STEPS) {
			Vector2i n = c + s;
			if (!inside(n) || wet(n) || ground(n) > ground(c)) {
				continue;
			}
			levels[static_cast<size_t>(_column_index(n.x, n.y))] = static_cast<uint8_t>(std::min(ground(n) + 1, top));
			queue.push_back({ n, steps + 1 });
		}
	}
	return levels;
}

} // namespace godot
