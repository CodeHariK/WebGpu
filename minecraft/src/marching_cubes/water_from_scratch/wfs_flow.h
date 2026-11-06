// Water from scratch, step 2c: which way the water flows.
//
// Water runs toward the nearest place it can leave its surface:
//   1. outlets: a wet column beside lower water, or beside dry ground below its surface (a spill
//      point). It points at them (the sum of those directions, normalized). The lower water it
//      falls into is a landing (foam there).
//   2. the rest of the body: breadth-first from the outlets through water with the same surface
//      (4 neighbours); each column points back at the column it was reached from, so following the
//      arrows leads to the nearest outlet.
// A body with no outlet (a pond, the sea) is still: it doesn't flow.
//
// Cost: one scan (4 sides per column) and one walk, each column at most once.
// The mesh carries this in its vertex colour for the shader (rg direction 0..1, b flows, a landing).
#ifndef WFS_FLOW_H
#define WFS_FLOW_H

#include "marching_cubes/water_from_scratch/wfs_columns.h"
#include <cstdint>
#include <vector>

namespace godot {

// One column's flow.
struct WfsFlow {
	float x = 0.0f; // direction (unit length when it flows)
	float z = 0.0f;
	uint8_t flows = 0; // 1: it flows (an outlet or reached from one)
	uint8_t landing = 0; // 1: a fall lands in it
};

// The flow of every column (see the top of this file). p_surface: from step 2b (dry: -1e6).
std::vector<WfsFlow> wfs_flow(
		const WfsColumns &p_columns,
		const std::vector<float> &p_surface
);

} // namespace godot

#endif // WFS_FLOW_H
