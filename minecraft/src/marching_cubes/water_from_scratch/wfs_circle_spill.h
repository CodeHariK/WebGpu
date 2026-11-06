// Water from scratch, step 2d: the circle spill (when its radius is above 0; it replaces the column
// spill of step 2a).
//
// A spill point is the centre of a cell where the water runs past a low corner onto dry ground:
//
//      W  L        W  a wet column (level l)            the cell's 4 corners
//      .  D        D  dry, diagonally across from W: not a bank (ground below l), not below the
//                     water's bed (its lowest ground in the cell)
//                  L  beside them, below the bed: the low corner the water runs past
//
// The water then also covers a circle of the spill radius round that point. Here, after the flow
// (so held / stream and the flow don't change), only the bookkeeping for the mesh is done:
//   - D gets W's water: its level, surface, held and flow (wetted = 1: it is wet only by the spill,
//     not one of the water's own corners, so its ground is wet only inside the circle)
//   - the spill cell remembers the level spilling there (cell_level), so every cell within one cell
//     of it knows the circle reaches it (step 3: the spill mask)
// One scan over the cells (4 corners each): linear.
#ifndef WFS_CIRCLE_SPILL_H
#define WFS_CIRCLE_SPILL_H

#include "marching_cubes/water_from_scratch/wfs_columns.h"
#include "marching_cubes/water_from_scratch/wfs_flow.h"
#include "marching_cubes/water_from_scratch/wfs_held.h"
#include <vector>

namespace godot {

// One spill point.
struct WfsSpillPoint {
	int cell = 0; // cz * (width - 1) + cx
	int from = 0; // W: the wet column it spills from
	int onto = 0; // D: the dry column diagonally across
};

// What applying the spill points left behind (for step 3).
struct WfsCircleSpill {
	std::vector<uint8_t> wetted; // per column: 1 if wet only by the circle spill
	std::vector<int> cell_level; // per cell: the level spilling there (0: none)
};

// The spill points of p_columns' water (stored levels: before anything is spilled).
std::vector<WfsSpillPoint> wfs_spill_points(const WfsColumns &p_columns);

// Wet each point's dry column with its source's water (levels in r_columns, and r_surfaces,
// r_flow), for the mesh only.
WfsCircleSpill wfs_apply_circle_spill(
		const std::vector<WfsSpillPoint> &p_points,
		WfsColumns &r_columns,
		WfsSurfaces &r_surfaces,
		std::vector<WfsFlow> &r_flow
);

} // namespace godot

#endif // WFS_CIRCLE_SPILL_H
