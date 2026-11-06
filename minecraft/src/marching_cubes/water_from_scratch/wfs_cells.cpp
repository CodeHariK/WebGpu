// Water from scratch, step 3 (see wfs_cells.h).
#include "marching_cubes/water_from_scratch/wfs_cells.h"
#include <algorithm>

namespace godot {

namespace {
const int CORNER_X[4] = { 0, 1, 1, 0 }; // the builder's corner order
const int CORNER_Z[4] = { 1, 1, 0, 0 };

// The circle spill points (at p_level) that can reach cell (p_cx, p_cz): each within one cell.
uint16_t spill_mask(
		const WfsColumns &p_columns,
		const WfsCircleSpill &p_spill,
		int p_cx,
		int p_cz,
		int p_level
) {
	if (p_spill.cell_level.empty()) {
		return 0;
	}
	int cells_x = p_columns.width - 1;
	int cells_z = p_columns.depth - 1;
	uint16_t mask = 0;
	for (int dz = -1; dz <= 1; dz++) {
		for (int dx = -1; dx <= 1; dx++) {
			int x = p_cx + dx;
			int z = p_cz + dz;
			if (x >= 0 && z >= 0 && x < cells_x && z < cells_z && p_spill.cell_level[static_cast<size_t>(z * cells_x + x)] == p_level) {
				mask |= static_cast<uint16_t>(1u << ((dz + 1) * 3 + (dx + 1)));
			}
		}
	}
	return mask;
}
} // namespace

std::vector<WfsBody> wfs_cells(
		const WfsColumns &p_columns,
		const WfsSurfaces &p_surfaces,
		const WfsCircleSpill &p_spill
) {
	std::vector<WfsBody> bodies;
	for (int cz = 0; cz + 1 < p_columns.depth; cz++) {
		for (int cx = 0; cx + 1 < p_columns.width; cx++) {
			size_t index[4];
			int ground[4];
			int level[4];
			bool wet[4];
			for (int k = 0; k < 4; k++) {
				index[k] = static_cast<size_t>(p_columns.index(cx + CORNER_X[k], cz + CORNER_Z[k]));
				ground[k] = p_columns.heights[index[k]];
				wet[k] = p_columns.is_wet(static_cast<int>(index[k]));
				level[k] = wet[k] ? p_columns.levels[index[k]] : 0;
			}
			// each water level at the corners is a body, made when its first corner comes up
			for (int i = 0; i < 4; i++) {
				bool first = wet[i];
				for (int k = 0; k < i && first; k++) {
					first = !(wet[k] && level[k] == level[i]);
				}
				if (!first) {
					continue;
				}
				WfsBody body;
				body.cx = cx;
				body.cz = cz;
				body.level = level[i];
				body.bed = 1 << 20;
				body.top = -1e6f;
				auto own = [&](int k) { return wet[k] && level[k] == body.level; };
				for (int k = 0; k < 4; k++) {
					if (own(k)) {
						body.bed = std::min(body.bed, ground[k]);
						body.top = std::max(body.top, p_surfaces.surface[index[k]]);
						body.stream = body.stream || !p_surfaces.held[index[k]];
					}
				}
				for (int k = 0; k < 4; k++) {
					bool spilled = !p_spill.wetted.empty() && p_spill.wetted[index[k]];
					if (ground[k] >= body.level) {
						body.corner[k] = WFS_BANK;
					} else if (ground[k] < body.bed) {
						body.corner[k] = WFS_LOWER;
						float y = wet[k] ? p_surfaces.surface[index[k]] : static_cast<float>(ground[k]) - 0.5f;
						body.bottom = std::max(body.bottom, y); // where the fall lands
					} else {
						body.corner[k] = own(k) && !spilled ? WFS_WET : WFS_DRY;
					}
				}
				body.spill_mask = spill_mask(p_columns, p_spill, cx, cz, body.level);
				bodies.push_back(body);
			}
		}
	}
	return bodies;
}

} // namespace godot
