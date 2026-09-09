# StylizedTerrain — self-generated cartoon terrain

An alternative to Terrain3D for terrain we want full art control over. We generate the mesh ourselves,
so we own every step of the look — the exact thing an addon smooths away. No overhangs (it is a
heightmap); the point is creative control, not caves.

## Classes

| Class | Kind | Purpose |
|---|---|---|
| `StylizedTerrainMesh` | ArrayMesh (generator) | A fractal-noise height field over a `size` patch, meshed as either **terraced** flat plateaus with crisp risers (the Godus / hill-town look) or **faceted** low-poly hills. Flat-shaded, with per-band / per-height vertex colours so the toon material lights it with no textures. Only parameters are saved; rebuilds on change; `_surfaces` is stripped from storage. |
| `StylizedTerrain` | MeshInstance3D (node) | Displays the mesh with the shared toon solid material (band colours light for free), keeps a trimesh collider in sync, strips the generated mesh/material from storage. The self-generated stand-in for a Terrain3D region. |

## Height field → mesh

1. **Height** `h(x,z)` = `prop::fbm` (reused from the props) mapped to 0..1, times `height_scale`, pulled
   down at the border by a radial `island_falloff` mask.
2. **Terrace** (`terrace = true`) is an explicit **extract → triangulate → extrude** contour pipeline, not
   per-cell quantization (which looks blocky, like Minecraft):
   - **Caps / triangulate**: each grid triangle is sliced at every `step_height` band boundary it crosses
     (`clip_h`), the iso-line crossing interpolated along the triangle's edges. Each slice is a flat plate
     at its band floor; border edges drop a skirt. Triangles (not quads) avoid marching-squares saddle
     ambiguity. Heights are sampled at shared grid corners so crossings match seamlessly across cells.
   - **Extract**: every band-boundary crossing is collected as a segment and `stitch_segments` joins them
     (by exact endpoint match) into closed / border-open **contour loops** per band.
   - **Extrude**: each loop is extruded into the vertical terrace wall (from the lower band floor up to
     its own), facing the lower side (found by sampling the height field either side of each edge).
   The loops are kept and exposed as `get_contours()` — an Array of `PackedVector3Array`, each a terrace
   outline at its height — so gameplay can run paths, walls, or placement along a terrace ring. Finer
   `cell_size` = smoother contours. After neumueller.dev "Terraced Terrain: Extracting Contours".
   **Faceted** (`terrace = false`): the continuous height on a grid, each triangle flat-shaded — low-poly
   rounded hills. Both are the same generator; terrace vs faceted and the palette are the only differences.
3. **Colour**: `low_color` -> `high_color` by normalised height, blended toward `edge_color` near sea
   level, with a little `color_jitter`. Written as vertex colours; the toon shader reads them as albedo.

`sample_height(Vector2)` returns the final surface height for placement / gameplay queries.

## Key parameters

`size`, `cell_size` (grid), `height_scale`, `seed`, the `noise_*` set (`frequency`, `octaves`,
`lacunarity`, `gain`, `ridged`), `island_falloff`, `terrace`, `step_height`, and the colour set.

## Meshing note

Godot front faces are clockwise from the visible side, so `push_quad` winds the geometry opposite the
desired outward normal and shades with that normal — otherwise flat top tiles cull and the terrain looks
see-through. Flat shading means unique verts per face (no sharing); fine for a demo patch.

## Demo

`project/scene/stylized/stylized_terrain.tscn` (`make run_stylized`): a 160 m terraced island under the
toon material.

## Next

- **Spline shaping** — drive heights with the placed `ProceduralSpline3D` + `TerrainSplineDeformer`
  corridors (reuse `evaluate_spline_point_segmented` and the deformer's static `blend_height`) so roads,
  cliffs, lakes and the painter tooling work on this backend exactly as they do on Terrain3D.
- **Chunked streaming** around the player (as the TerraSpline compositor does) instead of one patch.
- More styles on the same generator: **hex / tile plateaus**, **mesa** (slope-clamped flats + steep
  walls), **dunes** (ridged), plus a shared height/slope colour-band pass.
