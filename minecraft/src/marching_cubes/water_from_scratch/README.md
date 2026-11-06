# Water from scratch

The water mesh rebuilt one step at a time, each step in its own small file, timed on its own
(`MCWaterScratch`, script `Claude outputs/scripts/wfs_time.gd`). The existing builder
(`grid/mc_grid_water*.cpp`) stays as it is until this one is understood and measured.

## The steps

| # | Step | What it does | File | Time (your map) |
|---|------|--------------|------|-----------------|
| 1 | Water path | flood fill: which columns hold water, at what level | `wfs_flood.*` | ~1 µs (33 x 33 columns) |
| 2 | Spill, held / stream, flow | diagonal spills, thin streams vs held pools, flow direction | `wfs_spill.*`, `wfs_held.*`, `wfs_flow.*` | ~3 + 1 + 3 µs |
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

## Step 2: spill, held / stream, flow

Three passes, each a scan of the columns plus one breadth-first walk (linear):

- **2a spill** (`wfs_spill.*`): a dry column diagonal to stored water, past a low corner, gets wet;
  from there it spreads over ground not higher, up to `water_spill_reach` columns. Mesh only, never stored.
- **2b held / stream** (`wfs_held.*`): water is held (surface `water_drop` below the next tier) when
  every side is the grid edge, a bank, same-or-higher water, or a framed fall onto lower water; else
  it is a stream (0.1 m above its ground). Same-level water beside a stream (8 neighbours) is a stream too.
- **2c flow** (`wfs_flow.*`): outlets point at lower water or a spill point; the rest of the body
  points back toward the nearest outlet. Lower water a fall lands in gets foam.

Checked column by column against the builder (`MCGrid.get_water_columns_debug`): levels, held,
surface and flow all match on the test map (1089 columns: 1 spilled, 583 held, 6 stream, 7 flowing).

## Step 2d: the circle spill (when its radius is above 0)

Replaces 2a's column spill. A spill point is the centre of a cell with a wet corner W, the dry
column D diagonally across (not a bank, not below the water's bed) and a lower column beside them
(the low corner the water runs past). After 2c, so held / stream and the flow don't change, D gets
W's water for the mesh only (level, surface, held, flow) and is marked as spilled; the spill cell
remembers the level spilling there. One scan over the cells (`wfs_circle_spill.*`, ~8 µs for the points).
Step 2's runner now follows the grid: with a circle spill radius set, 2a's column spill is off.
