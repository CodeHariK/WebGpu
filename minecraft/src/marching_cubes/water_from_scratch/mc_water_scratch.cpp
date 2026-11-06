// MCWaterScratch (see mc_water_scratch.h).
#include "marching_cubes/water_from_scratch/mc_water_scratch.h"
#include "marching_cubes/grid/mc_grid.h"
#include "marching_cubes/water_from_scratch/wfs_flood.h"
#include "marching_cubes/water_from_scratch/wfs_flow.h"
#include "marching_cubes/water_from_scratch/wfs_held.h"
#include "marching_cubes/water_from_scratch/wfs_spill.h"
#include <godot_cpp/classes/time.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/packed_color_array.hpp>
#include <godot_cpp/variant/packed_float32_array.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>
#include <cmath>
#include <algorithm>

namespace godot {

void MCWaterScratch::_bind_methods() {
	ClassDB::bind_method(D_METHOD("load_grid", "grid"), &MCWaterScratch::load_grid);
	ClassDB::bind_method(D_METHOD("run_flood", "grid", "runs"), &MCWaterScratch::run_flood, DEFVAL(20));
	ClassDB::bind_method(D_METHOD("run_step2", "grid", "runs"), &MCWaterScratch::run_step2, DEFVAL(20));
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
	drop = grid->get_water_drop();
	spill_reach = grid->get_water_spill_reach();
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

namespace {
// The best (smallest) time of p_runs calls of p_step, in microseconds.
template <typename Step>
int64_t best_usec(
		int p_runs,
		Step p_step
) {
	uint64_t best = UINT64_MAX;
	for (int r = 0; r < std::max(p_runs, 1); r++) {
		uint64_t started = Time::get_singleton()->get_ticks_usec();
		p_step();
		best = std::min(best, Time::get_singleton()->get_ticks_usec() - started);
	}
	return static_cast<int64_t>(best);
}
} // namespace

Dictionary MCWaterScratch::run_step2(
		Object *p_grid,
		int p_runs
) {
	Dictionary out;
	// the input: step 1's levels
	WfsColumns settled = columns;
	settled.levels = wfs_settle(columns).levels;
	// 2a spills, 2b held / stream, 2c flow; each one's output is the next one's input
	std::vector<int> spilled_levels;
	out["spill_usec"] = best_usec(p_runs, [&]() { spilled_levels = wfs_spill(settled, spill_reach); });
	WfsColumns spilled = settled;
	spilled.levels = spilled_levels;
	WfsSurfaces surfaces;
	out["held_usec"] = best_usec(p_runs, [&]() { surfaces = wfs_surfaces(spilled, drop); });
	std::vector<WfsFlow> flow;
	out["flow_usec"] = best_usec(p_runs, [&]() { flow = wfs_flow(spilled, surfaces.surface); });
	// what it found
	int spilled_count = 0;
	int flowing = 0;
	int landings = 0;
	for (size_t i = 0; i < flow.size(); i++) {
		spilled_count += spilled.is_wet(static_cast<int>(i)) && !settled.is_wet(static_cast<int>(i)) ? 1 : 0;
		flowing += flow[i].flows;
		landings += flow[i].landing;
	}
	out["spilled"] = spilled_count;
	out["held"] = surfaces.held_count;
	out["streams"] = surfaces.stream_count;
	out["flowing"] = flowing;
	out["landings"] = landings;
	// against the grid's own step 2
	MCGrid *grid = Object::cast_to<MCGrid>(p_grid);
	if (!grid) {
		return out;
	}
	Dictionary reference = grid->get_water_columns_debug();
	PackedInt32Array ref_levels = reference.get("levels", PackedInt32Array());
	PackedByteArray ref_held = reference.get("held", PackedByteArray());
	PackedFloat32Array ref_surface = reference.get("surface", PackedFloat32Array());
	PackedColorArray ref_flow = reference.get("flow", PackedColorArray());
	if (static_cast<size_t>(ref_levels.size()) != flow.size()) {
		out["error"] = "no reference";
		return out;
	}
	int levels_differ = 0;
	int held_differ = 0;
	int surface_differ = 0;
	int flow_differ = 0;
	for (size_t i = 0; i < flow.size(); i++) {
		int64_t r = static_cast<int64_t>(i);
		levels_differ += spilled.levels[i] != ref_levels[r] ? 1 : 0;
		bool wet = spilled.is_wet(static_cast<int>(i));
		held_differ += wet && surfaces.held[i] != ref_held[r] ? 1 : 0;
		surface_differ += std::abs(surfaces.surface[i] - ref_surface[r]) > 1e-4f ? 1 : 0;
		const Color &c = ref_flow[r];
		bool same_flow = flow[i].flows == (c.b > 0.5f ? 1 : 0) && flow[i].landing == (c.a > 0.5f ? 1 : 0) &&
				std::abs(flow[i].x - c.r) < 1e-3f && std::abs(flow[i].z - c.g) < 1e-3f;
		flow_differ += same_flow ? 0 : 1;
	}
	out["levels_differ"] = levels_differ;
	out["held_differ"] = held_differ;
	out["surface_differ"] = surface_differ;
	out["flow_differ"] = flow_differ;
	return out;
}

} // namespace godot
