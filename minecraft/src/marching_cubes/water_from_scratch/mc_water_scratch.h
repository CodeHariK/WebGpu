// MCWaterScratch: runs the water-from-scratch steps one at a time on an MCGrid's columns and times
// each, so every step can be understood, checked and measured on its own (see README.md).
//
// From GDScript:
//   var scratch := MCWaterScratch.new()
//   scratch.load_grid(grid)            # copy the grid's heights and water levels
//   print(scratch.run_flood(grid))     # step 1: {usec, sources, pours, reached, wet, matches, ...}
//   print(scratch.run_step2(grid))     # step 2: spills, held / stream, flow, each timed
//   print(scratch.run_step3(grid))     # step 2d (circle spill) and step 3 (cell classes)
#ifndef MC_WATER_SCRATCH_H
#define MC_WATER_SCRATCH_H

#include "marching_cubes/water_from_scratch/wfs_columns.h"
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/packed_vector2_array.hpp>

namespace godot {

class MCGrid;

class MCWaterScratch : public RefCounted {
	GDCLASS(MCWaterScratch,
			RefCounted)

	WfsColumns columns; // the copied input
	float drop = 0.15f; // m held water sits below the next tier (the grid's water_drop)
	int spill_reach = 1; // columns a diagonal spill wets (the grid's water_spill_reach)
	float spill_radius = 0.0f; // m the circle spill (the grid's water_spill_radius; 0: off, else no column spill)

protected:
	static void _bind_methods();

public:
	// Copy p_grid's column heights and water levels (the input of every step).
	void load_grid(Object *p_grid);
	// Step 1, the water path (wfs_flood.h), timed (best of p_runs) and compared with p_grid's own levels.
	Dictionary run_flood(
			Object *p_grid,
			int p_runs = 20
	);
	// Step 2 (wfs_spill.h, wfs_held.h, wfs_flow.h) on step 1's levels, each part timed (best of
	// p_runs), compared column by column with the grid's own (MCGrid.get_water_columns_debug):
	// {spill_usec, held_usec, flow_usec, spilled, held, streams, flowing, landings,
	//  levels_differ, held_differ, surface_differ, flow_differ}
	Dictionary run_step2(
			Object *p_grid,
			int p_runs = 20
	);
	// Steps 2d and 3 (wfs_circle_spill.h, wfs_cells.h) after steps 1 - 2c, each timed (best of
	// p_runs), and every body compared with the grid's (MCGrid.get_water_bodies_debug):
	// {spill_points_usec, circle_usec, cells_usec, spill_points, bodies, cells_with_water, by class
	//  (bank, wet, lower, dry corners), bodies_differ, first_difference}
	Dictionary run_step3(
			Object *p_grid,
			int p_runs = 20
	);
};

} // namespace godot

#endif // MC_WATER_SCRATCH_H
