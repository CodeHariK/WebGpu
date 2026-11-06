// Water from scratch, step 3: the cells' water bodies and their corner classes.
//
// The mesh is built cell by cell: a cell is the square between 4 corner columns. Each water level
// present at the cell's corners is one body (two bodies when a waterfall's top and foot share a
// cell). For each body:
//   bed     its lowest ground in the cell (over its own corners)
//   top     its surface: the highest surface of its own corners
//   stream  any of its own corners is a stream
//   bottom  where its fall lands: the highest of the surfaces / ground of the corners below the bed
//           (-1e6: no fall)
//   corner  each corner's class for this body:
//             BANK   ground at or above the body's level: the water ends under it
//             LOWER  ground below the bed: the water falls down there
//             WET    one of the body's own corners
//             DRY    dry ground of the bed's tier: a gap (step 2's spills deal with it; a circle-
//                    spilled corner counts as dry: its ground is wet only inside the circle)
//   spill_mask  the circle spill points that reach the cell: bit (dz + 1) * 3 + (dx + 1) for a
//               spill point in the cell (dx, dz) away (each within one cell), at this body's level
// These four classes are all the shape needs (step 4): the cell's water edge is where their rounded
// blend crosses half way, so the same 4 classes always give the same shape (baked once as a tile).
//
// Corner order (as the builder's tiles): 0 (cx, cz + 1), 1 (cx + 1, cz + 1), 2 (cx + 1, cz),
// 3 (cx, cz). One pass over the cells, 4 corners each: linear.
#ifndef WFS_CELLS_H
#define WFS_CELLS_H

#include "marching_cubes/water_from_scratch/wfs_circle_spill.h"
#include "marching_cubes/water_from_scratch/wfs_columns.h"
#include "marching_cubes/water_from_scratch/wfs_held.h"
#include <cstdint>
#include <vector>

namespace godot {

enum WfsCorner : uint8_t {
	WFS_BANK = 0,
	WFS_WET = 1,
	WFS_LOWER = 2,
	WFS_DRY = 3,
};

// One body of water in one cell.
struct WfsBody {
	int cx = 0;
	int cz = 0;
	int level = 0;
	int bed = 0;
	float top = 0.0f;
	bool stream = false;
	float bottom = -1e6f;
	uint8_t corner[4] = { WFS_DRY, WFS_DRY, WFS_DRY, WFS_DRY };
	uint16_t spill_mask = 0;
};

// Every cell's bodies, cell by cell (z then x), each cell's in corner order. p_columns: the levels
// after step 2 (spills included); p_spill: step 2d's (empty when the circle spill is off).
std::vector<WfsBody> wfs_cells(
		const WfsColumns &p_columns,
		const WfsSurfaces &p_surfaces,
		const WfsCircleSpill &p_spill
);

} // namespace godot

#endif // WFS_CELLS_H
