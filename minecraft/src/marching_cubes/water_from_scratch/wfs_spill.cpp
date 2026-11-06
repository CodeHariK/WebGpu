// Water from scratch, step 2a (see wfs_spill.h).
#include "marching_cubes/water_from_scratch/wfs_spill.h"
#include <algorithm>
#include <utility>

namespace godot {

namespace {
const int STEP_X[4] = { 1, -1, 0, 0 }; // the 4 neighbours
const int STEP_Z[4] = { 0, 0, 1, -1 };
const int DIAGONAL_X[4] = { 1, -1, 1, -1 }; // the 4 diagonal neighbours
const int DIAGONAL_Z[4] = { 1, 1, -1, -1 };
} // namespace

std::vector<int> wfs_spill(
		const WfsColumns &p_columns,
		int p_reach
) {
	std::vector<int> levels = p_columns.levels;
	if (p_reach <= 0) {
		return levels;
	}
	auto ground = [&](int p_x, int p_z) { return p_columns.heights[static_cast<size_t>(p_columns.index(p_x, p_z))]; };
	auto wet_now = [&](int p_x, int p_z) { // wet in the levels being built (stored or spilled)
		size_t i = static_cast<size_t>(p_columns.index(p_x, p_z));
		return levels[i] > p_columns.heights[i];
	};
	auto spill_onto = [&](int p_x, int p_z) {
		levels[static_cast<size_t>(p_columns.index(p_x, p_z))] = std::min(ground(p_x, p_z) + 1, p_columns.top);
	};
	struct Spilled {
		int x;
		int z;
		int steps; // 1: the landing
	};
	std::vector<Spilled> queue;
	// 1. the landings: dry diagonal neighbours of stored water, past a low corner
	for (int z = 0; z < p_columns.depth; z++) {
		for (int x = 0; x < p_columns.width; x++) {
			if (!p_columns.is_wet(p_columns.index(x, z))) {
				continue; // only stored water spills
			}
			int g = ground(x, z);
			for (int k = 0; k < 4; k++) {
				int nx = x + DIAGONAL_X[k];
				int nz = z + DIAGONAL_Z[k];
				if (!p_columns.inside(nx, nz) || wet_now(nx, nz) || ground(nx, nz) > g) {
					continue; // off the grid, already wet, or higher
				}
				if (ground(nx, z) < g || ground(x, nz) < g) { // a low corner between them
					spill_onto(nx, nz);
					queue.push_back({ nx, nz, 1 });
				}
			}
		}
	}
	// 2. spread on from the landings over dry ground not higher, up to p_reach columns in all
	for (size_t head = 0; head < queue.size(); head++) {
		Spilled c = queue[head];
		if (c.steps >= p_reach) {
			continue;
		}
		for (int k = 0; k < 4; k++) {
			int nx = c.x + STEP_X[k];
			int nz = c.z + STEP_Z[k];
			if (!p_columns.inside(nx, nz) || wet_now(nx, nz) || ground(nx, nz) > ground(c.x, c.z)) {
				continue;
			}
			spill_onto(nx, nz);
			queue.push_back({ nx, nz, c.steps + 1 });
		}
	}
	return levels;
}

} // namespace godot
