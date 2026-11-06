// Water from scratch, step 2d (see wfs_circle_spill.h).
#include "marching_cubes/water_from_scratch/wfs_circle_spill.h"
#include <algorithm>

namespace godot {

namespace {
const int CORNER_X[4] = { 0, 1, 1, 0 }; // round the cell: (0, 0) (1, 0) (1, 1) (0, 1)
const int CORNER_Z[4] = { 0, 0, 1, 1 };
} // namespace

std::vector<WfsSpillPoint> wfs_spill_points(const WfsColumns &p_columns) {
	std::vector<WfsSpillPoint> points;
	int cells_x = p_columns.width - 1;
	for (int cz = 0; cz + 1 < p_columns.depth; cz++) {
		for (int cx = 0; cx < cells_x; cx++) {
			int index[4];
			int ground[4];
			for (int k = 0; k < 4; k++) {
				index[k] = p_columns.index(cx + CORNER_X[k], cz + CORNER_Z[k]);
				ground[k] = p_columns.heights[static_cast<size_t>(index[k])];
			}
			auto level_of = [&](int k) { return p_columns.levels[static_cast<size_t>(index[k])]; };
			for (int k = 0; k < 4; k++) {
				if (!p_columns.is_wet(index[k])) {
					continue;
				}
				int level = level_of(k);
				int bed = 1 << 20; // the water's lowest ground in the cell
				for (int m = 0; m < 4; m++) {
					if (p_columns.is_wet(index[m]) && level_of(m) == level) {
						bed = std::min(bed, ground[m]);
					}
				}
				int across = (k + 2) % 4;
				bool dry = ground[across] < level && !p_columns.is_wet(index[across]) && ground[across] >= bed;
				bool lower = ground[(k + 1) % 4] < bed || ground[(k + 3) % 4] < bed;
				if (ground[k] < level && dry && lower) {
					points.push_back({ cz * cells_x + cx, index[k], index[across] });
					break; // one point per cell
				}
			}
		}
	}
	return points;
}

WfsCircleSpill wfs_apply_circle_spill(
		const std::vector<WfsSpillPoint> &p_points,
		WfsColumns &r_columns,
		WfsSurfaces &r_surfaces,
		std::vector<WfsFlow> &r_flow
) {
	WfsCircleSpill out;
	out.wetted.assign(r_columns.heights.size(), 0);
	out.cell_level.assign(static_cast<size_t>(std::max(r_columns.width - 1, 0) * std::max(r_columns.depth - 1, 0)), 0);
	for (const WfsSpillPoint &point : p_points) {
		size_t from = static_cast<size_t>(point.from);
		size_t onto = static_cast<size_t>(point.onto);
		out.cell_level[static_cast<size_t>(point.cell)] = r_columns.levels[from];
		if (r_columns.is_wet(point.onto)) {
			continue; // another spill wetted it already
		}
		r_columns.levels[onto] = r_columns.levels[from];
		r_surfaces.surface[onto] = r_surfaces.surface[from];
		r_surfaces.held[onto] = r_surfaces.held[from];
		r_flow[onto] = r_flow[from];
		out.wetted[onto] = 1;
	}
	return out;
}

} // namespace godot
