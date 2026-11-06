// Water from scratch, step 2a: diagonal spills.
//
// The pour (step 1) runs left / right / front / back only. A dry column that touches the water only
// diagonally stays dry, and shows as a notch at the water's edge where the terrain has a low corner:
//
//      W  .        W  wet column (ground g)
//      L  D        L  a column lower than g, beside both
//                  D  dry, diagonal to W, ground not higher than g
//
// The water runs past the low corner L, so it spills onto D. From D it spreads on over dry ground
// that is not higher (4 neighbours) for up to p_reach columns in all (1: only D; 0: no spills).
// Every spilled column stands one layer deep (level = ground + 1).
//
// Only stored water spills (a spilled column is not a new source), so spilling never runs away.
// The algorithm: one scan for the landings (8 x columns), then a breadth-first walk from them
// that stops at p_reach steps — linear in the number of columns.
//
// Spills are for the mesh only: the stored levels never include them (stored spill water would be a
// source next time the water settles and flood its whole tier).
#ifndef WFS_SPILL_H
#define WFS_SPILL_H

#include "marching_cubes/water_from_scratch/wfs_columns.h"
#include <vector>

namespace godot {

// p_columns' water levels with the diagonal spills added (see the top of this file).
std::vector<int> wfs_spill(
		const WfsColumns &p_columns,
		int p_reach
);

} // namespace godot

#endif // WFS_SPILL_H
