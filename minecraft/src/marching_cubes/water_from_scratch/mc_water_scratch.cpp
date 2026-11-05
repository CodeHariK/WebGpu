// MCWaterScratch (see mc_water_scratch.h).
#include "marching_cubes/water_from_scratch/mc_water_scratch.h"
#include "marching_cubes/grid/mc_grid.h"
#include "marching_cubes/water_from_scratch/wfs_flood.h"
#include <godot_cpp/classes/time.hpp>
#include <algorithm>

namespace godot {

void MCWaterScratch::_bind_methods() {
	ClassDB::bind_method(D_METHOD("load_grid", "grid"), &MCWaterScratch::load_grid);
	ClassDB::bind_method(D_METHOD("run_flood", "grid", "runs"), &MCWaterScratch::run_flood, DEFVAL(20));
}

void MCWaterScratch::load_grid(Object *p_grid) {
	MCGrid *grid = Object::cast_to<MCGrid>(p_grid);
	if (!grid) {
		return;
	}
	Vector2i counts = grid->get_column_counts();
	columns.width = counts.x;
	columns.depth = counts.y;
	columns.top = grid->get_max_column_height();
	columns.sea_level = grid->get_sea_level();
	columns.heights.assign(static_cast<size_t>(counts.x * counts.y), 0);
	columns.levels.assign(static_cast<size_t>(counts.x * counts.y), 0);
	for (int z = 0; z < counts.y; z++) {
		for (int x = 0; x < counts.x; x++) {
			size_t i = static_cast<size_t>(columns.index(x, z));
			columns.heights[i] = grid->get_column_height(Vector2i(x, z));
			columns.levels[i] = grid->get_water_level(Vector2i(x, z));
		}
	}
}

Dictionary MCWaterScratch::run_flood(
		Object *p_grid,
		int p_runs
) {
	Dictionary out;
	WfsFloodResult result;
	uint64_t best = UINT64_MAX;
	for (int r = 0; r < std::max(p_runs, 1); r++) { // best of p_runs: the first run warms the caches
		uint64_t started = Time::get_singleton()->get_ticks_usec();
		result = wfs_settle(columns);
		best = std::min(best, Time::get_singleton()->get_ticks_usec() - started);
	}
	int wet = 0;
	for (size_t i = 0; i < result.levels.size(); i++) {
		wet += result.levels[i] > columns.heights[i] ? 1 : 0;
	}
	// compare with the grid's own water levels (its settle_water ran on load)
	int matches = 0;
	int differs = 0;
	MCGrid *grid = Object::cast_to<MCGrid>(p_grid);
	for (int z = 0; grid && z < columns.depth; z++) {
		for (int x = 0; x < columns.width; x++) {
			int mine = result.levels[static_cast<size_t>(columns.index(x, z))];
			(mine == grid->get_water_level(Vector2i(x, z)) ? matches : differs)++;
		}
	}
	out["usec"] = static_cast<int64_t>(best);
	out["columns"] = columns.width * columns.depth;
	out["sources"] = result.sources;
	out["pours"] = result.pours;
	out["reached"] = result.reached;
	out["wet"] = wet;
	out["matches"] = matches;
	out["differs"] = differs;
	return out;
}

} // namespace godot
