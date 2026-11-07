# Water

The water layer of the grid's heightmap mode: ponds, rivers, the sea and waterfalls. Only one
thing is stored: a **water level** per corner column (0 = dry). Everything else is worked out again
on every rebuild, in the steps below, and turned into one water mesh.

The code is still made of `MCGrid` methods (declared in `terrain/mc_grid.h`); the folders only group
them by step. `build/mc_water_build.cpp` (`_rebuild_water`) is where to start reading: it calls the
steps in this order.

Steps 1 – 3 work out the water per column and per cell; step 4 makes the mesh: each cell stamps
its cached tile. (An "outline" mode that traced and filled whole lakes was removed: even optimized
it rebuilt in about twice the time, every edit, and drew more triangles.)

## Folders

```
water/
  1_flood/       step 1: the stored water (pours, the sea, settle)
  2_columns/     step 2: per column - 2a spill, 2b held / stream, 2c flow
  3_bodies/      step 3: per cell - the bodies and their shape (banks from the tile meshes)
  4_tiles/       step 4: the mesh - cached tiles, the fall strips, the cut under them
  build/         runs the steps in order, makes the mesh, the debug views
  debug/         overlays and the step-by-step walk
  mc_water_types.h   the water types MCGrid uses (falls, the mesh being built, trace steps, baked tiles)
  mc_water_steps.h   the column grid's 4 neighbour steps (+x, -x, +z, -z)
```

## The steps

Grid words: a **column** is a grid point (a corner) with a ground height and a water level; a
**cell** is the 1 m square between 4 columns.

### 1_flood — the stored water

`mc_water_flood.cpp`. Pouring (`fill_water`) floods out left / right / front / back over ground that
is not higher and stands one layer deep. Also the sea (`_flood_sea`), `settle_water`, painting and
clearing. In: the ground heights. Out: a water level per column (this is what is saved).

### 2_columns — what the water does per column (built each rebuild, never stored)

| Sub-step | File | In → out |
|---|---|---|
| 2a spill | `mc_water_2a_spill.cpp` | levels → levels with diagonal spills (water running past a low corner onto a dry column it only touches diagonally) |
| 2b held | `mc_water_2b_held.cpp` | levels → surface height per column: **held** (banks keep it in, one layer deep) or **stream** (a thin sheet that keeps running) |
| 2c flow | `mc_water_2c_flow.cpp` | surfaces → flow direction per column (toward the nearest fall; ponds are still) |

### 3_bodies — water per cell

`mc_water_bodies.cpp` (`_water_bodies`). Each cell gets one flat sheet (**body**) per water level
among its 4 corners, and each corner is classed for that sheet: **bank** (rock above the water),
**wet**, **lower** (water or ground below), **dry**. The body's 2D shape is in
`mc_water_shape.h`: the same function the cliff tiles are built from, so the water's edge lands on
the cliff's edge. The banks and the fall line come from the meshes: the tile at the waterline cut by
a flat plane at the water's height, and the tile under the bed cut just below its grass
(`mc_water_banks_slice.cpp`, from the tile meshes baked once in `mc_water_banks_bake.cpp`), baked
once per mesh, turn and height. The corner blend still shapes the edge toward dry corners.

### 4_tiles — the mesh, one tile per body

`mc_water_tiles.cpp`. A body's shape depends only on its corner classes and heights (22 kinds on
the test map), so each kind is baked once into a **water tile**: sample a grid over the cell, trace
the wet area's edge with marching squares, keep it in a cache. Every cell then stamps its tile and
the vertices are welded. `mc_water_fall_strips.cpp`: the fall strips down the cliffs, part of the
same pass. `mc_water_surface_cut.cpp`: before the strips are made, the flat water is cut away from
under each strip's bevel (its footprint), so it never sticks out through it.

### build — the rebuild and the mesh

- `mc_water_build.cpp`: `_rebuild_water` (the steps in order, each timed in `water_stage_usec`)
  and `_water_mesh` (one surface, falls first).
- `mc_water_view.cpp`: the view modes (normal, wireframe, UV, UV + wireframe, pieces: every mesh
  piece in its own colour, flow: the flow the shader gets).
- `mc_water_liquid.cpp`: the map's one liquid (water, lava, green poison), handed to
  `project/material/shaders/terrain/liquid.gdshader` (one material, one draw).

### debug

- `mc_water_trace.cpp`: the step-by-step trace the Debug tab's buttons walk through (steps 1, 2a – 2c).
- `mc_water_trace_cell.cpp`: steps 3 – 4 for one cell (the Debug tab's water steps walk, a click
  picks the cell, then Prev / Next): its corners, each body's corner classes and shape (3), the
  tile's samples, edge pieces and triangles, the tiles placed round it (4). A yellow post marks the cell.
- `mc_water_debug_stages.cpp`: the stage 0 – 4 overlay (columns, cells, edge pieces, mesh).
- `mc_water_debug_overlay.cpp`: held / stream / sea squares, flow arrows, the pour replay.
