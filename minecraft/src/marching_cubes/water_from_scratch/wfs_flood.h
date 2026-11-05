// Water from scratch, step 1: the water path (which columns hold water, at what level).
//
// Water poured on a column runs on to its 4 neighbours (left / right / front / back) wherever the
// ground there is NOT HIGHER than the ground it is on:
//   same height   it spreads
//   lower         it falls down there and carries on from there
//   higher        a bank: it stops
// Everywhere it reaches it stands one layer deep: level = ground + 1.
//
// The algorithm is a breadth-first flood fill:
//   1. put the start column in a queue and mark it seen
//   2. take the column at the front of the queue
//   3. look at its 4 neighbours: off the grid, already seen or higher -> skip; else mark it seen and
//      add it to the back of the queue
//   4. repeat 2-3 until the queue is empty; every column taken off the queue is reached
// Each column goes through the queue at most once, so the cost grows with the number of columns
// (linear), whatever the map looks like.
//
// "Settling" re-runs it for all the water on the map: every wet column (not the sea) is a source,
// highest ground first; a column one pour already reached is not poured again. The sea keeps its level.
#ifndef WFS_FLOOD_H
#define WFS_FLOOD_H

#include "marching_cubes/water_from_scratch/wfs_columns.h"
#include <vector>

namespace godot {

// What step 1 found.
struct WfsFloodResult {
	std::vector<int> levels; // the new water level per column
	int sources = 0; // wet columns that could pour
	int pours = 0; // pours that reached something new
	int reached = 0; // columns the water reached
};

// One pour from p_start (steps 1-4 above). Columns already in r_seen are skipped; the reached ones
// are added to r_seen and returned nearest first.
std::vector<int> wfs_flood_from(
		const WfsColumns &p_columns,
		int p_start,
		std::vector<uint8_t> &r_seen
);

// Settle all the water on the map (see the top of this file).
WfsFloodResult wfs_settle(const WfsColumns &p_columns);

} // namespace godot

#endif // WFS_FLOOD_H
