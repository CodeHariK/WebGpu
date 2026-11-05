// Water from scratch, step 1 (see wfs_flood.h).
#include "marching_cubes/water_from_scratch/wfs_flood.h"
#include <algorithm>

namespace godot {

namespace {
const int STEP_X[4] = { 1, -1, 0, 0 }; // the 4 neighbours: +x, -x, +z, -z
const int STEP_Z[4] = { 0, 0, 1, -1 };
} // namespace

std::vector<int> wfs_flood_from(
		const WfsColumns &p_columns,
		int p_start,
		std::vector<uint8_t> &r_seen
) {
	std::vector<int> queue; // the queue; everything that went through it, in order, is the region
	if (r_seen[static_cast<size_t>(p_start)]) {
		return queue;
	}
	queue.push_back(p_start); // 1. the start column
	r_seen[static_cast<size_t>(p_start)] = 1;
	for (size_t head = 0; head < queue.size(); head++) { // 2. the column at the front
		int column = queue[head];
		int x = column % p_columns.width;
		int z = column / p_columns.width;
		int ground = p_columns.heights[static_cast<size_t>(column)];
		for (int k = 0; k < 4; k++) { // 3. its 4 neighbours
			int nx = x + STEP_X[k];
			int nz = z + STEP_Z[k];
			if (!p_columns.inside(nx, nz)) {
				continue; // off the grid
			}
			int neighbour = p_columns.index(nx, nz);
			if (r_seen[static_cast<size_t>(neighbour)]) {
				continue; // already queued
			}
			if (p_columns.heights[static_cast<size_t>(neighbour)] > ground) {
				continue; // higher: a bank
			}
			r_seen[static_cast<size_t>(neighbour)] = 1; // same or lower: the water runs there
			queue.push_back(neighbour);
		}
	} // 4. until the queue is used up
	return queue;
}

WfsFloodResult wfs_settle(const WfsColumns &p_columns) {
	WfsFloodResult result;
	result.levels = p_columns.levels;
	size_t count = p_columns.heights.size();
	// the sources: every wet column that isn't the sea, highest ground first
	std::vector<int> sources;
	for (size_t i = 0; i < count; i++) {
		int column = static_cast<int>(i);
		if (p_columns.is_wet(column) && !p_columns.is_sea(column)) {
			sources.push_back(column);
		}
	}
	std::stable_sort(sources.begin(), sources.end(), [&](int p_a, int p_b) {
		return p_columns.heights[static_cast<size_t>(p_a)] > p_columns.heights[static_cast<size_t>(p_b)];
	});
	result.sources = static_cast<int>(sources.size());
	std::vector<uint8_t> seen(count, 0);
	for (int source : sources) {
		std::vector<int> region = wfs_flood_from(p_columns, source, seen);
		if (!region.empty()) {
			result.pours++;
		}
		for (int column : region) {
			size_t i = static_cast<size_t>(column);
			// one layer deep; the sea keeps its level
			result.levels[i] = p_columns.is_sea(column) ? p_columns.sea_level : std::min(p_columns.heights[i] + 1, p_columns.top);
		}
		result.reached += static_cast<int>(region.size());
	}
	return result;
}

} // namespace godot
