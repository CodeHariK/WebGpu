// Water from scratch, step 2c (see wfs_flow.h).
#include "marching_cubes/water_from_scratch/wfs_flow.h"
#include <cmath>

namespace godot {

namespace {
const float SAME = 0.02f; // m: surfaces closer than this are the same water
const int STEP_X[4] = { 1, -1, 0, 0 }; // the 4 neighbours
const int STEP_Z[4] = { 0, 0, 1, -1 };
} // namespace

std::vector<WfsFlow> wfs_flow(
		const WfsColumns &p_columns,
		const std::vector<float> &p_surface
) {
	std::vector<WfsFlow> flow(p_columns.heights.size());
	std::vector<int> queue;
	// 1. the outlets
	for (int z = 0; z < p_columns.depth; z++) {
		for (int x = 0; x < p_columns.width; x++) {
			int i = p_columns.index(x, z);
			if (!p_columns.is_wet(i)) {
				continue;
			}
			float surface = p_surface[static_cast<size_t>(i)];
			float out_x = 0.0f;
			float out_z = 0.0f;
			for (int k = 0; k < 4; k++) {
				int nx = x + STEP_X[k];
				int nz = z + STEP_Z[k];
				if (!p_columns.inside(nx, nz)) {
					continue;
				}
				size_t n = static_cast<size_t>(p_columns.index(nx, nz));
				bool wet = p_columns.is_wet(static_cast<int>(n));
				bool lower_water = wet && p_surface[n] < surface - SAME;
				bool spill = !wet && static_cast<float>(p_columns.heights[n]) - 0.5f < surface - SAME;
				if (lower_water || spill) {
					out_x += static_cast<float>(STEP_X[k]);
					out_z += static_cast<float>(STEP_Z[k]);
				}
				if (lower_water) {
					flow[n].landing = 1;
				}
			}
			float length = std::sqrt(out_x * out_x + out_z * out_z);
			if (length > 0.0f) {
				WfsFlow &f = flow[static_cast<size_t>(i)];
				f.x = out_x / length;
				f.z = out_z / length;
				f.flows = 1;
				queue.push_back(i);
			}
		}
	}
	// 2. the rest of each body, from the outlets: point back where it was reached from
	for (size_t head = 0; head < queue.size(); head++) {
		int c = queue[head];
		int x = c % p_columns.width;
		int z = c / p_columns.width;
		float surface = p_surface[static_cast<size_t>(c)];
		for (int k = 0; k < 4; k++) {
			int nx = x + STEP_X[k];
			int nz = z + STEP_Z[k];
			if (!p_columns.inside(nx, nz)) {
				continue;
			}
			size_t n = static_cast<size_t>(p_columns.index(nx, nz));
			if (p_columns.is_wet(static_cast<int>(n)) && std::abs(p_surface[n] - surface) < SAME && !flow[n].flows) {
				flow[n].x = static_cast<float>(-STEP_X[k]);
				flow[n].z = static_cast<float>(-STEP_Z[k]);
				flow[n].flows = 1;
				queue.push_back(static_cast<int>(n));
			}
		}
	}
	return flow;
}

} // namespace godot
