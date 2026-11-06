// Water from scratch, step 2b: held water and streams, and where each surface sits.
//
// Every wet column is one of two kinds:
//   held    banks hold it in, so it stands one layer deep: its surface is p_drop below the next
//           tier (level - 0.5 - p_drop, in grid space where a column's top is at height - 0.5)
//   stream  it runs out somewhere, so it is a thin sheet STREAM (0.1 m) above its own ground
//
// A column is held when each of its 4 sides is one of:
//   - the grid's edge
//   - dry ground the water can't spill onto (ground at least level - p_drop)
//   - water at the same level or higher
//   - lower water down a FRAMED fall (see wfs_fall_framed below)
// The sea is always held. Anything else (dry ground below the surface beside it, or a fall off an
// open edge) makes it a stream.
//
// Then streams spread: same-level water beside a stream drains out through it, so it is a stream
// too. This walk uses all 8 neighbours: diagonal columns share a terrain cell, and a cell has one
// surface per level, so a held corner diagonal to a stream corner would lift the stream to the held
// height (floating water). Breadth-first from the first streams, each column at most once.
//
// Cost: one scan (4 sides per column, plus the framed-fall walks along fall edges) and one walk.
#ifndef WFS_HELD_H
#define WFS_HELD_H

#include "marching_cubes/water_from_scratch/wfs_columns.h"
#include <cstdint>
#include <vector>

namespace godot {

// What step 2b found, per column.
struct WfsSurfaces {
	std::vector<uint8_t> held; // 1: held, 0: a stream (or dry)
	std::vector<float> surface; // the water surface's height (grid space); dry: -1e6
	int held_count = 0;
	int stream_count = 0;
};

// Is the fall from column (p_x, p_z) one step (p_step_x, p_step_z) down onto lower water framed —
// cut into the land like a river's? Walk sideways along the edge over the columns that fall the
// same way; at both ends there must be a bank (ground at least p_level, or the grid's edge) or the
// water must turn away behind one. A fall off the open edge of a tier is not framed.
bool wfs_fall_framed(
		const WfsColumns &p_columns,
		int p_x,
		int p_z,
		int p_step_x,
		int p_step_z,
		int p_level
);

// Held or stream, and the surface height, for every column (see the top of this file).
// p_columns.levels: the levels with the spills (step 2a).
WfsSurfaces wfs_surfaces(
		const WfsColumns &p_columns,
		float p_drop
);

} // namespace godot

#endif // WFS_HELD_H
