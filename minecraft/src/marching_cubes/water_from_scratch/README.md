# Water from scratch

The water mesh rebuilt one step at a time, each step in its own small file, timed on its own
(`MCWaterScratch`, script `Claude outputs/scripts/wfs_time.gd`). The existing builder
(`grid/mc_grid_water*.cpp`) stays as it is until this one is understood and measured.

## The steps

| # | Step | What it does | File | Time (your map) |
|---|------|--------------|------|-----------------|
| 1 | Water path | flood fill: which columns hold water, at what level | `wfs_flood.*` | ~1 µs (33 x 33 columns) |
| 2 | Spill, held / stream, flow | diagonal spills, thin streams vs held pools, flow direction | | |
| 3 | Cell classes | each cell's 4 corners: bank / wet / lower / dry | | |
| 4 | Sampling the shape | the water's shape per cell on a fine grid, marching squares: triangles + edge pieces (bank / fall) | | |
| 5 | Outline | edge pieces chained into closed loops, smoothed | | |
| 6 | Inset | fall parts pulled in by the bevel radius | | |
| 7 | Flat water mesh | the inset loops triangulated | | |
| 8 | Waterfall mesh | the bevel / drop / foot profile swept along the fall parts | | |
| 9 | Assembly | particle points, triangle winding, mesh upload | | |

## Step 1: the water path

Input: per corner column its ground height and its water level (wet when level > height).
Water runs to the 4 neighbours whose ground is not higher (same: spreads, lower: falls on);
higher ground is a bank. A breadth-first flood fill from each wet column, highest ground first,
each column visited once: cost grows linearly with the number of columns. Everything it reaches
stands one layer deep (level = ground + 1); the sea keeps its level.
