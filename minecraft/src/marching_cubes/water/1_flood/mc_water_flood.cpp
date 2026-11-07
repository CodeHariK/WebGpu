// Water step 1, flood: the stored water levels per column, and the tools that change them.
//
// MCGrid: the water layer (heightmap mode) — Animal Crossing style ponds, rivers and the sea, plan in
// docs/TerrainStorage.md §7.
//
// Water is not a volume: each corner column has a water level L (0 = dry), like its ground height.
// The column is wet when L is above its ground (L > height); the surface then sits water_drop below where
// ground of height L would be (y = L - 0.5 - water_drop), so banks one tier up stand just above it.
// Drawn on the same grid as the cliffs: a column's water is a 1 x 1 square centred on the column,
// and a cliff between two columns stands half way between them — so where two wet neighbours have
// different levels, the step is exactly where the cliff is, and a waterfall sheet fills it.
// Waterfalls are never stored: raise or lower a level and they appear / go by themselves.
//
//   set_water_level   one column
//   fill_water        pour water on a column: a flood fill that flows left / right / front / back
//                     onto ground that is not higher (same tier: spreads; lower: falls and carries on
//                     down there) and stands one layer deep wherever it reaches (level = ground + 1).
//                     Higher ground is a bank. A dug channel fills and runs down its steps as
//                     waterfalls into a basin or the sea. (The Water tool's left click.)
//   paint_water       waterscaping: the column's ground dug one tier and water at its old height
//   clear_water       dry the connected water (any level: a river with its falls), not the sea
//   sea_level         randomize_heights floods the sea from the grid's corner at this level (0: none):
//                     every connected column below it
// Held water and streams (_water_surfaces): water stands one layer deep (surface water_drop below the next
// tier) only where banks hold it in — every side a bank, the same held water, or a waterfall cut into
// the land (_fall_framed) onto lower water; the sea is held. Elsewhere (it ran out of a channel onto
// open ground, or a tier's open edge is beside it) it is a stream: a thin sheet STREAM above the ground
// that keeps running and pours over the edge. Same-level water joined to a stream drains through it,
// so it is a stream too. Derived when the mesh is built; only the levels are stored.
// Spills: where water stands beside dry ground lower than its surface, a fall sheet runs down onto it.
// Flow (_water_flow): water runs toward the nearest fall. A column next to lower water or a spill
// is an outlet (it points at the fall); a breadth-first walk from the outlets through same-level
// water points every other column of the body at the one it was reached from. A body with no
// outlet (a pond, the sea) is still. The column a fall lands in gets foam. The mesh carries this in
// its vertex COLOR (blended from the cell's 4 corners, so it turns smoothly) for water.gdshader:
// rg direction as 0..1, b flows, a landing foam.
// The mesh is built from the cells' bodies (step 3, mc_water_bodies.cpp: the banks and fall lines
// from the cliff tile meshes) as cached water tiles (step 4, mc_water_tiles.cpp), so its edges and
// falls follow the cliffs exactly. Steps 2 - 4 run in order in _rebuild_water (mc_water_build.cpp).
// One water mesh for the whole grid (surface and falls in one surface, one material), rebuilt at
// the end of a frame in which water or ground changed (flush_dirty_chunks) and on refresh_grid.
// Only the levels are saved (save v2).
#include "marching_cubes/terrain/mc_grid.h"
#include "marching_cubes/water/mc_water_steps.h"
#include <algorithm>
#include <functional>
#include <vector>

namespace godot {

using mc_water::STEPS;

// The water surface height of level p_level (world y in grid space).
float MCGrid::_surface_y(int p_level) const { return static_cast<float>(p_level) - 0.5f - water_drop; }

void MCGrid::set_water_drop(float p_drop) {
	water_drop = Math::clamp(p_drop, 0.0f, 0.95f);
	if (!chunks.empty() && water_levels.size() == column_heights.size()) {
		_queue_water();
	}
}

int MCGrid::get_water_level(const Vector2i &p_column) const {
	int index = _column_index(p_column.x, p_column.y);
	if (index < 0 || static_cast<size_t>(index) >= water_levels.size()) {
		return 0;
	}
	return water_levels[static_cast<size_t>(index)];
}

// Is column p_index wet (water above its ground)?
bool MCGrid::_is_wet(int p_index) const {
	return static_cast<size_t>(p_index) < water_levels.size() && static_cast<size_t>(p_index) < column_heights.size() &&
			water_levels[static_cast<size_t>(p_index)] > column_heights[static_cast<size_t>(p_index)];
}

bool MCGrid::_is_sea(size_t p_index) const {
	return sea_level > 0 && water_levels[p_index] == sea_level && column_heights[p_index] < sea_level;
}

void MCGrid::set_water_level(
		const Vector2i &p_column,
		int p_level
) {
	int index = _column_index(p_column.x, p_column.y);
	if (!heightmap_mode || index < 0 || static_cast<size_t>(index) >= water_levels.size()) {
		return;
	}
	_record_water(index);
	water_levels[static_cast<size_t>(index)] = static_cast<uint8_t>(std::clamp(p_level, 0, _max_column_height()));
	_queue_water();
}

// Pour water at p_column: it flows left / right / front / back wherever the ground beside it is not
// higher than the ground under it (same tier: it spreads, lower: it falls and carries on down
// there), and stands one layer deep everywhere it reaches (level = ground + 1). Higher ground is a
// bank. So it fills a dug channel, runs down its steps as waterfalls and stops in a basin or the sea.
int MCGrid::fill_water(
		const Vector2i &p_column,
		int p_level
) {
	int start = _column_index(p_column.x, p_column.y);
	if (!heightmap_mode || start < 0 || static_cast<size_t>(start) >= column_heights.size()) {
		return 0;
	}
	int top = _max_column_height();
	auto depth_one = [&](size_t p_index) { return std::min(static_cast<int>(column_heights[p_index]) + 1, top); };
	// the poured level (p_level, at most one layer deep; below the ground: nothing)
	int level = std::min(p_level, depth_one(static_cast<size_t>(start)));
	if (level <= column_heights[static_cast<size_t>(start)]) {
		return 0;
	}
	std::vector<uint8_t> seen(column_heights.size(), 0);
	_reset_pour_trace(); // the step buttons go through this pour
	std::vector<int> region = _flow_region(p_column, seen, &last_pour_trace);
	last_pour = region; // for the debug overlay's replay and step-by-step view
	for (int index : region) {
		size_t i = static_cast<size_t>(index);
		int wanted = index == start ? level : depth_one(i);
		if (water_levels[i] != wanted) {
			_record_water(index);
			water_levels[i] = static_cast<uint8_t>(wanted);
		}
	}
	_queue_water();
	return static_cast<int>(region.size());
}

// The flood fill every step 1 tool uses (breadth-first). The start column goes in a queue; each
// step takes the column at the front and looks at its 4 neighbours (+x, -x, +z, -z): off the grid
// -> skipped; already reached -> skipped; p_enter(this column, neighbour) false -> rejected;
// otherwise -> reached and added to the back of the queue. It ends when the queue is used up; the
// queue in order is the region (nearest first). Columns already in r_seen are skipped (and the
// reached ones added). r_trace (optional) records every step for the debug view.
std::vector<int> MCGrid::_flood(
		const Vector2i &p_column,
		const std::function<bool(size_t, size_t)> &p_enter,
		std::vector<uint8_t> &r_seen,
		std::vector<MCPourStep> *r_trace
) const {
	std::vector<int> region;
	int start = _column_index(p_column.x, p_column.y);
	if (start < 0 || r_seen[static_cast<size_t>(start)]) {
		return region;
	}
	Vector2i columns = _column_counts();
	std::vector<Vector2i> queue = { p_column };
	r_seen[static_cast<size_t>(start)] = 1;
	for (size_t head = 0; head < queue.size(); head++) {
		Vector2i c = queue[head];
		int index = _column_index(c.x, c.y);
		region.push_back(index);
		MCPourStep trace;
		trace.column = index;
		for (int k = 0; k < 4; k++) {
			Vector2i n = c + STEPS[k];
			if (n.x < 0 || n.y < 0 || n.x >= columns.x || n.y >= columns.y) {
				trace.result[k] = MCPourStep::OUTSIDE;
				continue;
			}
			size_t i = static_cast<size_t>(_column_index(n.x, n.y));
			if (r_seen[i]) {
				trace.result[k] = MCPourStep::SEEN;
			} else if (!p_enter(static_cast<size_t>(index), i)) {
				trace.result[k] = MCPourStep::HIGHER;
			} else {
				trace.result[k] = MCPourStep::QUEUED;
				r_seen[i] = 1;
				queue.push_back(n);
			}
		}
		if (r_trace) {
			trace.queue_size = static_cast<int>(queue.size());
			trace.head = static_cast<int>(head);
			r_trace->push_back(trace);
		}
	}
	return region;
}

// Where water poured at p_column runs: onto every neighbour whose ground is not higher (same tier:
// it spreads; lower: it falls and carries on down there). Higher ground is a bank.
std::vector<int> MCGrid::_flow_region(
		const Vector2i &p_column,
		std::vector<uint8_t> &r_seen,
		std::vector<MCPourStep> *r_trace
) const {
	auto not_higher = [&](size_t p_from, size_t p_to) { return column_heights[p_to] <= column_heights[p_from]; };
	return _flood(p_column, not_higher, r_seen, r_trace);
}

std::vector<int> MCGrid::_settle_sources() const {
	std::vector<int> sources;
	for (size_t i = 0; i < water_levels.size(); i++) {
		if (_is_wet(static_cast<int>(i)) && !_is_sea(i)) {
			sources.push_back(static_cast<int>(i));
		}
	}
	std::stable_sort(sources.begin(), sources.end(), [&](int p_a, int p_b) {
		return column_heights[static_cast<size_t>(p_a)] > column_heights[static_cast<size_t>(p_b)];
	});
	return sources;
}

// Let all water flow again (after a load or a terrain edit): every wet column, highest ground first,
// pours one layer deep wherever it can reach; a column already reached isn't poured again. Water
// that is left standing deeper than one layer is brought down to one layer. The sea stays as it is.
int MCGrid::settle_water() {
	if (!heightmap_mode || water_levels.size() != column_heights.size()) {
		return 0;
	}
	Vector2i columns = _column_counts();
	int top = _max_column_height();
	std::vector<uint8_t> seen(column_heights.size(), 0);
	int changed = 0;
	for (int source : _settle_sources()) {
		Vector2i column(source % columns.x, source / columns.x);
		for (int index : _flow_region(column, seen)) {
			size_t i = static_cast<size_t>(index);
			int wanted = _is_sea(i) ? sea_level : std::min(static_cast<int>(column_heights[i]) + 1, top);
			if (water_levels[i] != wanted) {
				_record_water(index);
				water_levels[i] = static_cast<uint8_t>(wanted);
				changed++;
			}
		}
	}
	if (changed > 0) {
		_queue_water();
	}
	return changed;
}

// Every column in p_region gets water level p_level (recorded for undo).
void MCGrid::_set_water(
		const std::vector<int> &p_region,
		int p_level
) {
	for (int index : p_region) {
		_record_water(index);
		water_levels[static_cast<size_t>(index)] = static_cast<uint8_t>(p_level);
	}
	if (!p_region.empty()) {
		_queue_water();
	}
}

// The sea: from p_column, every connected column whose ground is below p_level, all at p_level.
void MCGrid::_flood_sea(
		const Vector2i &p_column,
		int p_level
) {
	int level = std::clamp(p_level, 0, _max_column_height());
	int start = _column_index(p_column.x, p_column.y);
	if (!heightmap_mode || start < 0 || column_heights[static_cast<size_t>(start)] >= level) {
		return; // no sea there
	}
	auto below = [&](size_t, size_t p_to) { return column_heights[p_to] < level; };
	std::vector<uint8_t> seen(column_heights.size(), 0);
	_set_water(_flood(p_column, below, seen), level);
}

bool MCGrid::paint_water(const Vector2i &p_column) {
	int index = _column_index(p_column.x, p_column.y);
	if (!heightmap_mode || index < 0 || static_cast<size_t>(index) >= column_heights.size() || _is_wet(index)) {
		return false;
	}
	int height = column_heights[static_cast<size_t>(index)];
	if (height < 1) {
		return false;
	}
	set_column_height(p_column, height - 1);
	if (column_heights[static_cast<size_t>(index)] != height - 1) {
		return false; // a locked rim column
	}
	set_water_level(p_column, height);
	return true;
}

// Dry the water connected to p_column (4 neighbours, any level: a whole river with its falls). The
// sea (water at sea_level) stays, so drying a river that runs into it doesn't empty the sea.
int MCGrid::clear_water(const Vector2i &p_column) {
	int start = _column_index(p_column.x, p_column.y);
	if (start < 0 || !_is_wet(start) || _is_sea(static_cast<size_t>(start))) {
		return 0;
	}
	auto river = [&](size_t, size_t p_to) { return _is_wet(static_cast<int>(p_to)) && !_is_sea(p_to); };
	std::vector<uint8_t> seen(column_heights.size(), 0);
	std::vector<int> region = _flood(p_column, river, seen);
	for (int index : region) {
		_record_water(index);
		water_levels[static_cast<size_t>(index)] = 0;
	}
	_queue_water();
	return static_cast<int>(region.size());
}

// Sea level for randomize_heights (0: no sea).
void MCGrid::set_sea_level(int p_level) { sea_level = std::max(p_level, 0); }

// The water mesh needs rebuilding at the end of this frame.
void MCGrid::_queue_water() {
	water_dirty = true;
	_queue_rebuild();
}

} // namespace godot
