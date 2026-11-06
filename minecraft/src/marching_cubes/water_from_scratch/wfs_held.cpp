// Water from scratch, step 2b (see wfs_held.h).
#include "marching_cubes/water_from_scratch/wfs_held.h"

namespace godot {

namespace {
const float STREAM = 0.1f; // m a stream's surface sits above its ground
const int STEP_X[4] = { 1, -1, 0, 0 }; // the 4 neighbours
const int STEP_Z[4] = { 0, 0, 1, -1 };
const int AROUND_X[8] = { 1, -1, 0, 0, 1, 1, -1, -1 }; // the 8 neighbours
const int AROUND_Z[8] = { 0, 0, 1, -1, 1, -1, 1, -1 };
} // namespace

bool wfs_fall_framed(
		const WfsColumns &p_columns,
		int p_x,
		int p_z,
		int p_step_x,
		int p_step_z,
		int p_level
) {
	auto bank = [&](int p_bx, int p_bz) {
		return !p_columns.inside(p_bx, p_bz) || p_columns.heights[static_cast<size_t>(p_columns.index(p_bx, p_bz))] >= p_level;
	};
	auto level = [&](int p_index) { return p_columns.levels[static_cast<size_t>(p_index)]; };
	// across the fall: (x, z) step (1, 0) -> walk along z, and the other way round
	const int across_x = p_step_z;
	const int across_z = p_step_x;
	for (int sign : { 1, -1 }) {
		int mx = p_x;
		int mz = p_z;
		for (int guard = 0; guard < p_columns.width + p_columns.depth; guard++) {
			mx += across_x * sign;
			mz += across_z * sign;
			if (bank(mx, mz)) {
				break; // a bank ends the run: framed on this side
			}
			int m = p_columns.index(mx, mz);
			if (!p_columns.is_wet(m) || level(m) != p_level) {
				return false; // lower ground or other water: open
			}
			int bx = mx + p_step_x;
			int bz = mz + p_step_z;
			if (bank(bx, bz)) {
				break; // the same water turns away behind a bank: framed
			}
			int below = p_columns.index(bx, bz);
			if (!p_columns.is_wet(below) || level(below) >= p_level) {
				return false; // it doesn't fall the same way: open
			}
			// it falls the same way: the run goes on
		}
	}
	return true;
}

WfsSurfaces wfs_surfaces(
		const WfsColumns &p_columns,
		float p_drop
) {
	size_t count = p_columns.heights.size();
	WfsSurfaces out;
	out.held.assign(count, 0);
	out.surface.assign(count, -1e6f);
	auto level = [&](int p_index) { return p_columns.levels[static_cast<size_t>(p_index)]; };
	auto ground = [&](int p_index) { return p_columns.heights[static_cast<size_t>(p_index)]; };
	// 1. held where every side holds the water in
	std::vector<int> streams; // the walk's queue
	for (int z = 0; z < p_columns.depth; z++) {
		for (int x = 0; x < p_columns.width; x++) {
			int i = p_columns.index(x, z);
			if (!p_columns.is_wet(i)) {
				continue;
			}
			bool held = true;
			if (!p_columns.is_sea(i)) {
				for (int k = 0; k < 4 && held; k++) {
					int nx = x + STEP_X[k];
					int nz = z + STEP_Z[k];
					if (!p_columns.inside(nx, nz)) {
						continue; // the grid's edge holds it
					}
					int n = p_columns.index(nx, nz);
					if (!p_columns.is_wet(n)) {
						// dry ground below the surface: the water spills onto it
						held = !(static_cast<float>(ground(n)) < static_cast<float>(level(i)) - p_drop);
					} else if (level(n) < level(i)) {
						held = wfs_fall_framed(p_columns, x, z, STEP_X[k], STEP_Z[k], level(i));
					}
				}
			}
			out.held[static_cast<size_t>(i)] = held ? 1 : 0;
			if (!held) {
				streams.push_back(i);
			}
		}
	}
	// 2. same-level water beside a stream (8 neighbours) drains through it: a stream too
	for (size_t head = 0; head < streams.size(); head++) {
		int c = streams[head];
		int x = c % p_columns.width;
		int z = c / p_columns.width;
		for (int k = 0; k < 8; k++) {
			int nx = x + AROUND_X[k];
			int nz = z + AROUND_Z[k];
			if (!p_columns.inside(nx, nz)) {
				continue;
			}
			int n = p_columns.index(nx, nz);
			if (out.held[static_cast<size_t>(n)] && !p_columns.is_sea(n) && p_columns.is_wet(n) && level(n) == level(c)) {
				out.held[static_cast<size_t>(n)] = 0;
				streams.push_back(n);
			}
		}
	}
	// 3. the surfaces
	for (size_t i = 0; i < count; i++) {
		int column = static_cast<int>(i);
		if (!p_columns.is_wet(column)) {
			continue;
		}
		if (out.held[i]) {
			out.surface[i] = static_cast<float>(level(column)) - 0.5f - p_drop;
			out.held_count++;
		} else {
			out.surface[i] = static_cast<float>(ground(column)) - 0.5f + STREAM;
			out.stream_count++;
		}
	}
	return out;
}

} // namespace godot
