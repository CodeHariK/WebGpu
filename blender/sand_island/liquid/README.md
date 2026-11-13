# Liquid Shaders

The liquid shaders of the minecraft game's MC water: water, lava and green poison, each in a
simple **mobile** look and a no-budget **desktop** look; and the **slime**, a cheap shader whose
look is baked into textures in Blender. They are worked on here, one idea at a time,
and the parts that are liked go back into the game (its copies:
`minecraft/project/material/shaders/terrain/liquid/`, the mobile look only).

It is its own Godot project (Forward+, like the game), not part of `../water_tutorial/`: these
shaders run on the game's MC water mesh, not on the sand island's water.

```
make liquids          # the viewer (from the folder above: blender/sand_island/)
make liquid-shots     # shots/liquid_<shader>.png, by day and by night
make liquid-steps     # the poison (mobile) built step by step (steps.tscn; T: the water)
make liquid-steps-water  # the same viewer, starting on the water (mobile)
make liquid-steps-shots  # shots/<poison|water>_mobile_step<NN>.png, one per step
make liquid-sample    # export the sample meshes again from the game
make slime-bake       # bake the slime's textures in Blender (and import them)
make slime-river      # build the slime river sample in Blender (and import it)
make waterfall-simple # one waterfall sheet, a hand-written shader (waterfall_simple/)
make waterfall-simple-shot  # shots/waterfall_simple.png
make waterfall-full   # the whole waterfall rebuilt from the simple parts (waterfall_full.tscn)
make waterfall-full-build  # build that scene again (from full_layout.json), shots/waterfall_full.png
make waterfall-noise  # bake the waterfall's noises into noise/noise_a.png, noise_b.png (3D textures)
make whirlpool        # a whirlpool on one flat sheet, built step by step (whirlpool/)
make whirlpool-shots  # shots/whirlpool_step<NN>_<name>.png, one per step
```

## Folders

| Folder | What it holds |
|---|---|
| `water_mobile/` | the mobile water: a cartoon look from the world normal and position (triplanar, toon bands, depth-texture edge foam), no UV |
| `lava_mobile/`, `poison_mobile/` | the mobile look: steps 1 – 5 shared (`liquid_common`), lava's crust (6), poison's bubbles (7) |
| `water_desktop/`, `lava_desktop/`, `poison_desktop/` | the desktop look: refraction, absorption, caustics, glow, … (each file's header lists its parts) |
| `slime/` | `slime.gdshader` + `slime.tres` (the material with its textures); `slime_pattern.png`, `slime_slopes.png` (baked); `slime_bake.blend` (the node materials that make them); `slime_channels.png` (the four channels side by side) |
| `poison_mobile/steps/`, `water_mobile/steps/` | the step tracks (below); `steps.tscn` / `steps.gd` show them (T switches) |
| `shader_lib/` | `liquid_noise` (hash, noise), `liquid_common` (the mobile lava's and poison's steps 2 – 5), `liquid_desktop` (the desktop set's tools) |
| `sample/` | `water.res` + `terrain.res`: the game's water and the ground round it (its "last" inspect map); `slime_river.blend` + `.glb`: the slime river (Blender) |
| `water_stylized/` | the simple waterfall's look on the MC water mesh: falls, flat water, froth, shore lace in one shader (below) |
| `waterfall_simple/` | one sheet of the waterfall, the pool under it (`waterfall_sheet.glb`, `waterfall_pool.glb`), the froth tube and scattered stones (made by script), puffs, spray and bubbles, hand-written shaders built in steps (below); `waterfall_simple.tscn`; `waterfall_full.tscn` (the whole waterfall from these parts, `full_layout.json`, `meshes/`); `noise/` (its noises baked into two 3D textures) |
| `whirlpool/` | a whirlpool on one flat sheet from the world position alone (no flow map, no UV): `steps/` (9 steps, the last one is `whirlpool.gdshader`), `whirlpool.tscn` / `whirlpool.gd` (the sheet and the step viewer) |
| `tools/` | `export_liquid_sample.gd`: makes the MC sample (it runs inside the game project); `bake_slime.py`, `build_slime_river.py`: the Blender scripts (`make slime-bake`, `make slime-river`); `shoot_waterfall.gd`, `build_waterfall_full.gd`, `bench_waterfall.gd`, `bake_waterfall_noise.py`: the simple waterfall's |

Step-by-step tracks, as the sand island's shaders have: `poison_mobile/steps/` (12 steps, the last
one is `poison_mobile.gdshader`) and `water_mobile/steps/` (10 steps, the last one is
`water_mobile.gdshader`). Each step file is a whole shader; the lines a step adds end in
`// NEW`, and the comment at its top (shown in the viewer's panel) says what the step adds. The
lava gets a `steps/` folder the same way.

| Poison step | Adds |
|---|---|
| 1 colour | one matte green |
| 2 depth | debug: the mesh's depth (`CUSTOM0.x`) as grey |
| 3 shallows | pale at the edges, green where deep |
| 4 flow | debug: the flow (`COLOR`): **C** cycles all / r / g / b / a / dots moving with it / `UV2.x` (down a fall) / `UV2.y` (above the landing) |
| 5 ripples | two sizes of noise blobs, still |
| 6 slide | ripples slid along the flow (and why that stretches at bends) |
| 7 flow map | the two-copy trick: running, never stretching |
| 8 falls | threads sliding down the falls (`UV2`), faster as they fall |
| 9 foam | shore, landing and fall-foot froth |
| 10 bubbles | grow and pop, one per square of the plane |
| 11 drift | the bubbles ride the flow (the same trick) |
| 12 glow | a faint toxic `EMISSION`: the finished poison |

| Water step | Adds |
|---|---|
| 1 normal | debug: the world normal as colour; **C**: up only (grey) |
| 2 fall | flat or falling from `normal.y`: two blues |
| 3 toon light | a `light()` with a crisp lit / shade step, never dark |
| 4 triplanar | debug: patterns laid on from three sides (world position), weighted by the normal; **C**: the weights / a noise |
| 5 streaks | threads falling down the side views, two toon bands |
| 6 lip | a white band where it bends over a fall's edge |
| 7 lines | thin wavy lines on the top view (contour of noise, `fwidth`: pixels wide); **C**: the noise |
| 8 run | the lines run along the flow (`COLOR`; two copies, cross-faded) |
| 9 edge foam | the depth texture: white where shallow, round anything in the water; **C**: metres over the ground |
| 10 landing | churning white blobs where a fall lands (`COLOR.a`): the finished water |

### The mobile water, the poison and the sand island's water

| | `water_mobile` (cartoon) | `poison_mobile` | the sand island's water (`../water_tutorial/`, step 13) |
|---|---|---|---|
| Mesh | the MC water mesh | the MC water mesh | the island's water plane |
| Patterns laid by | world position + world normal (triplanar) | `UV` (metres) and `UV2` (falls) | world position |
| Falls | found by the normal; streaks on the side views | `UV2`: threads down the fall's own frame | none |
| Flat water | wavy toon lines running with the flow (`COLOR`) | soft ripples (light / dark), flow-map slid | baked `water_macro` texture (4 noises in one read), drifting |
| Depth / shore | the depth texture: foam round the shore and anything in the water | `CUSTOM0.x`: shallow colour, shore foam | `COLOR.r`: colours, shore wash wave, contour lines |
| Look | toon: few flat colours, crisp edges, white lip / streaks / rims | soft, matte, bubbles and a glow | soft, sparkles, vertex swell, fake sea bed |
| Light | own `light()`: lit or shade (60 %) | lit, matte, a little `EMISSION` | `unshaded` |
| Cost a pixel | 6 value noises on the flat (lines 4, wobble, churn), 4 on a fall, 1 depth read | 8 noises + bubbles (hashes) | 1 texture read + a few hash texel reads |

## The mesh data the shaders read

The MC water mesh carries, per vertex (the game's `src/marching_cubes/water/build/mc_water_build.cpp`):

| | What |
|---|---|
| `COLOR` | the flow: rg the direction (x, z) as 0..1 (0.5 = none), b 1 where it flows, a 1 where a fall lands |
| `UV` | metres: world x, z on the flat liquid; on a fall its own frame (x across, y down from the lip) |
| `UV2` | x: 0 on the flat liquid, 0..1 over a fall's lip, 1+ down the fall; y: metres above the landing, −1 on a stream |
| `CUSTOM0.x` | the depth in metres |

On another mesh they compile but show their defaults (a plane is all shore foam: no depth).

## The slime

A goo of lumpy pillows split by glowing veins, bubbles growing and popping, creeping along the
flow. Everything that would cost noise per pixel is baked in Blender instead, so it costs 2 texture
lookups a vertex and 4 a pixel, opaque, one draw: cheap enough for mobile.

| Texture | R | G | B | A |
|---|---|---|---|---|
| `slime_pattern.png` | height (pillows) | veins | bubbles: 1 at a centre .. 0 at the rim | each bubble's phase |
| `slime_slopes.png` | height slope x | height slope y | the way out of a bubble, x | .. y |

- **Seamless**: `bake_slime.py` reads 4D Voronoi and noise on a torus: u and v each go once round a
  circle, so the pattern meets itself at the edges (open `slime_bake.blend`: frame 1).
- **Bulge**: `vertex()` pushes the surface up by the height, so the mesh must be dense (the river
  sample's squares are 0.1 m; on the coarser MC mesh it bulges less).
- **Bubbles** aren't cross-faded between the flow-map trick's two copies: each copy's bubbles live
  through its own cycle and pop before it restarts, and the copy with more bubble wins.
- **Import**: both PNGs are data. Their `.import` turns off *Fix Alpha Border* (it would repaint the
  colour where alpha is low); the phase never goes below 0.03 for the same reason.
- Knobs: `tile_metres` (pillow size), `flow_speed`, `bulge`, `bumpiness`, the colours and glows.

## The slime river sample (`sample/slime_river.glb`)

`build_slime_river.py`: a winding channel into a round pool between stone banks (the banks are built
straight from the channel's outline, no booleans). The surface is a 0.1 m grid cut to the channel,
running 0.12 m into the banks so its ragged edge hides in them. It carries the MC mesh's `COLOR` flow
(along the channel; in the pool spreading out from the inlet and fading) and `UV` (world x, z in
metres); no `UV2` or depth, so the shaders read it as flat liquid. Godot's Blender import is off in
`project.godot` (the `.blend` files are the sources; the `.glb` is what Godot loads).

## The simple waterfall (`waterfall_simple/`)

A stylized waterfall (a third-party Blender scene, not kept in this repo) taken apart, starting
from one sheet: `waterfall_sheet.glb` is its "Plane.004" (exported once from it: subdivided, no thickness, no ripples, centred,
bottom at 0) and `waterfall_simple.gdshader` is its "waterfall" material, by hand:
colour laid on by height, the edges between the colours bent by voronoi cells.

| Layer | What |
|---|---|
| 1 deep | a height line bent 24 % toward a voronoi's distance, cut once: two deep blues |
| 2 light | another bent height line: light teal over the top part |
| 3 aqua | voronoi middles (closer than 0.487 to a point) as aqua circles, in a soft patch near the top |
| streaks | thin bright vertical lines: the crests of noise-bent stripes (Wave Texture), shown only where a sideways wave sliding down meets a slow noise; 2.4 x brighter |
| dark lines | a few thin dark vertical lines down the whole sheet: the troughs of nearly straight stripes (4 or 5 across), half as bright |
| lip glow | a near-white patch over the top, its edge boiling with noise; under it a band of 12 stepped bright rings (up to 2.5 x), broken into islands by noise |
| tiers | four brightness tiers by height (a noise-bent height line cut into 4 steps): 0.43 x at the bottom to 1.45 x at the top |
| lace | a white web over the sheet: voronoi F1 minus Smooth F1 (the cells' borders), on the UV, nudged by noise |
| drips | the lace only in a couple of wandering columns (a slow wave, its coordinates pulled toward noise): full white on the crests, faint on the sides |
| grade | hue + 0.06, saturation x 1.7, value x 1.9 over all of it |

The cells fall: Blender slides each voronoi's Mapping up through its 3D texture over time. Built
step by step in `steps/` (step 1: the three colour layers, step 2: the streaks, step 3: the dark
lines, step 4: the lip glow, step 5: the
height tiers, step 6: the lace,
step 7: the drips, step 8: the ripples, step 9: variety, step 10: baked noise; the last step is
`waterfall_simple.gdshader`); the scene's panel steps through them (left / right). Steps 1 – 7
are the whole Blender material; 8 – 10 add:

| Sheet step | Adds |
|---|---|
| 8 ripples | the vertex shader pushes the sheet out and in by a noise sliding down it (0.7 m): the flat sheet bulges like falling water (the ripples the export leaves out) |
| 9 variety | `instance uniform vec2 pattern_offset`: each sheet in a scene shifts its patterns (and ripples) so copies of one mesh don't repeat |
| 10 baked noise | the noise functions read two 3D textures (below) instead of working the noises out: 16 x faster, the same look |

The pool under it (`waterfall_pool.glb`: "Plane.005", one quad, placed where it is in the Blender
file relative to the sheet) gets its material ("Material") the same way, in `pool_steps/` (the
last step is `waterfall_pool.gdshader`; T switches the panel between the sheet and the pool):

| Pool step | Adds |
|---|---|
| 1 bands | dark at the back under the falls, teal in the middle, half as bright at the front: lines across the pool bent by voronoi cells; Blender's sun (about 2/3); the pool's grade (hue + 0.07, saturation x 1.5, value x 2) |
| 2 glints | aqua dashes: gradient (Perlin) noise stretched sideways, kept where it is high, in a soft round patch; two layers drifting opposite ways |
| 3 webs | three cell-border webs near the falls (the sheet's lace): one darkens the water 28 %, one is white, one brighter still |
| 4 tiers | six brightness levels, 1.7 x at the back to 0.35 x at the front: the finished pool |
| 5 baked noise | the noises from the 3D textures, as the sheet's step 10: 42 x faster |

The pool's noise is gradient (Perlin) noise, like Blender's: its values stay near the middle, so
Blender's cuts ("keep it above 0.59") leave as few glints as in Blender; a noise of random heights
spreads wider and fills the pool with them.

The froth where the fall lands is a tube (`froth_tube.gd` builds it: 10 m long, 1.6 m thick, ends
rounded, 160 x 48 vertices), its material Blender's bottom foam, in `froth_steps/` (the last step
is `waterfall_froth.gdshader`):

| Froth step | Adds |
|---|---|
| 1 white over blue | past-white over a clear blue, split by a line round the tube by height (Blender's foam uses no light) |
| 2 churn | the line pulled 31 % toward a noise changing in place: white and blue boil into each other |
| 3 boil | the vertex shader pushes the tube out and in along its normals by a big noise sinking 2.7 m a second: rolling lumps |
| 4 ridges | small creases: bands bent hard by a fine noise (Blender's Wood texture), rounded instead of its saw tooth |
| 5 light, rim | not in Blender: toon light (lit / 45 %) from the lumpy surface's real slope (`dFdx` / `dFdy`, without the ridges), a dark rim at the water line |
| 6 baked noise | the Perlin fBM (lumps, ridges, churn) from a 3D texture: 24 x faster |


The stones round it are made in Godot, Blender's way but each its own: `stone_mesh.gd` splits an
icosphere 3 times (1280 triangles), pushes every vertex along its normal by a Perlin fBM (seeded),
squashes the bottom and rebuilds the normals (smooth, or flat for facets); `stone_scatter.gd`
places them by a seed (a stack up each side of the sheet, a few along the pool), each its own
mesh variant, size, stretch and turn. Their material is Blender's "rocks", in `stone_steps/` (the
last step is `waterfall_stone.gdshader`):

| Stone step | Adds |
|---|---|
| 1 shape | debug: the world normal as colour (`MODEL_NORMAL_MATRIX`: right for stretched stones) |
| 2 two tones | lit or shaded, one cut on how much the stone faces the sun: two dark teal-greys, x 1.3 |
| 3 cracks | a voronoi's distance to the nearest cell border (about 4 cells a stone), broken up by noise, cut thin: lighter blue-grey lines |
| 4 baked noise | the border distance and the break-up from a 3D texture (less a small bias: the texture's blending rounds the border's sharp V off): 89 x faster |

With `multimesh` on (the default) the scatter makes one `MultiMeshInstance3D` a mesh variant: 6
draw calls for all the stones instead of one each.

The spray and puffs are `GPUParticles3D` under `Particles`, where the fall lands: **Spray**, 120
small white unshaded spheres thrown up 5 – 8 m/s and falling back; **Puffs**, 36 quads facing the
camera, rising slowly, with their material in `puff_steps/` (the last step is
`waterfall_puff.gdshader`):

| Puff step | Adds |
|---|---|
| 1 round | a white disc on the quad, turned to the camera (`INV_VIEW_MATRIX`), cut sharp (alpha scissor) |
| 2 lumpy | its edge pushed out and in by noise, each puff its own (`INSTANCE_ID`) |
| 3 grow & break | by its age (`INSTANCE_CUSTOM.y`) it grows, then blotches of noise eat it away |
| 4 light | lit as a ball (a sphere's normal from the quad): lit / 45 %, toon |

The bubbles are three subdivided quads by the fall, Blender's bubble material in `bubble_steps/`
(the last step is `waterfall_bubble.gdshader`):

| Bubble step | Adds |
|---|---|
| 1 dots | voronoi cells' middles (closer than 0.2) as white dots, rising, in a round patch; the rest cut away |
| 2 wobble | the vertex shader bends the quad by a noise sinking down: the dots wander |
| 3 baked noise | the dots' voronoi from a 3D texture (the wobble stays worked out: it runs per vertex): 34 x faster |

### The whole waterfall (`waterfall_full.tscn`)

`full_layout.json` was exported from the same Blender file (with the meshes): the 8
sheets' transforms (in the sheet's frame, converted to Godot's axes), the foam boxes and the camera
(transform, field of view, focus distance). `tools/build_waterfall_full.gd` builds the scene from
the simple parts: 8 sheets on one mesh (`meshes/sheet.res`, each its own `pattern_offset`), the
lower and upper pools, three froth tubes (the lower and upper landings and the top lip), two stone
scatters, two sets of particles, 8 bubbles, and Blender's camera with its depth of field.
`make waterfall-full-build` builds it and shoots `shots/waterfall_full.png` (from Blender's camera).

### Baked noise (`noise/`)

Nearly all of the waterfall's time went into its noises, worked out for every pixel: a voronoi
lookup checks 27 cells (the stones' border distance twice), an fBM adds 3 sizes of noise from 8
corners each. The noises themselves never change, only where a shader reads them (TIME slides the
point), so `tools/bake_waterfall_noise.py` (`make waterfall-noise`; Python with numpy) works them
out once on a 96 x 96 x 96 grid and saves two 3D textures, each its 96 slices side by side in a PNG
(12 x 8), imported as a Texture3D:

| Texture | r | g | b | a |
|---|---|---|---|---|
| `noise_a` | voronoi F1 | F1 minus Smooth F1 (lace, webs), x 2 | value fBM, roughness 0.5 | value fBM, roughness 0.2 |
| `noise_b` | Perlin fBM, roughness 0.5 | Perlin fBM, roughness 0.375 (glints) | distance to a cell border, x 2 | unused |

The grid covers 6 noise units on each side (16 texels a unit) and its random numbers repeat every
6 units, so the textures tile: a shader reads point p at p / 6 with repeat on. They are global
shader uniforms (`waterfall_noise_a`, `waterfall_noise_b` in `project.godot`), so no material sets
them. Each track's last step swaps its noise functions for one `texture()` each and changes
nothing else; the cells sit in other places (the grid wraps) but the look is the same. 2 x 3.4 MB
of GPU memory, 6 MB of PNG.

### Cost

Measured (`make waterfall-bench`, `tools/bench_waterfall.gd`): one shader fills the screen at
4800 x 2700 (13 million pixels), mobile renderer, Apple M2, 240 frames, the way the island's bench
times its shaders (`water_tutorial/perf.md`; its water comes out at the same 0.36 ms here). Cost =
minus a flat colour (0.25 ms).

| Shader | Live (ms) | Baked noise (ms) | Faster |
|---|---|---|---|
| sheet | 25.29 | 1.55 | 16 x |
| pool | 35.61 | 0.84 | 42 x |
| froth | 2.90 | 0.12 | 24 x |
| stones | 11.57 | 0.13 | 89 x |
| bubbles | 3.44 | 0.10 | 34 x |

Next to the island's shaders: its water (step 13) 0.36, snow 1.78 (baked: 1.08), the game's
`water_mobile` 3.37. The baked sheet is now about as costly as the snow, the rest under the island
water. Whole scenes from their cameras (at 13 million pixels): `waterfall_simple.tscn` 0.67 ms a
frame (11.9 before), `waterfall_full.tscn` 1.06.

## The whirlpool (`whirlpool/`)

A whirlpool flows the same way all round its middle, so it needs no flow map: the shader works
the flow out from where the pixel is. It runs on one flat sheet (`whirlpool.gd`: a `PlaneMesh`
24 m square in 0.12 m squares, dense enough for the funnel), in world space
(`world_vertex_coords`), measured from the sheet's origin: move the sheet and the whirlpool goes
with it, scale it and the pattern keeps its size. Its noise is the waterfall's baked
`waterfall_noise_a` (a global uniform): 4 texture reads a pixel. Built in `steps/` (the last step
is `whirlpool.gdshader`); `make whirlpool` steps through them (left / right).

| Step | Adds |
|---|---|
| 1 colour | metres from the middle (the world position minus `MODEL_MATRIX[3]`): deep in the middle, shallow out at `radius` |
| 2 blobs | still noise blobs laid on in metres: a 2D slice through the baked 3D noise |
| 3 turntable | the pattern read where the liquid was (`rotate(offset, -turn * TIME)`): it turns like a plate |
| 4 faster inside | each ring its own rate (`spin_speed(r)`): liquid-like, but it winds up into thinner and thinner spirals |
| 5 two copies | the flow-map trick, turned round: each copy turns for one `cycle` and restarts, half a cycle apart, cross-faded, the contrast kept (÷ √(w0² + w1²)) |
| 6 pull in | the liquid also sinks inward (× e^(pull × s)): a spiral, slow at the rim, fast near the eye |
| 7 streaks | long lines going round: noise read on a circle ((cos, sin) of the angle) and in rings (distance): no seam; this is why the noise is 3D |
| 8 funnel | the vertex shader sinks the middle (a bell, `depth`, `funnel`) and tips the normal by its slope; a dark eye |
| 9 foam | white where the streaks are high, crisp, in a ring round the eye: the finished whirlpool |

## The stylized water on the game's mesh (`water_stylized/`)

`water_stylized.gdshader` (shader 8 in the viewer) puts the simple waterfall's look on the MC
water mesh, one shader for all of it, like the mobile water:

| Part | From the mesh |
|---|---|
| falls | the sheet's layers: across = `UV.x` (metres along the edge), height = `UV2.y` / (`UV2.y` + `UV.y`): 1 at every lip, 0 where it lands, so the lip glow and the tiers fit any height; the cells sized in metres (one sheet: 7.96 x 13.49 m) |
| flat water | the pool's bands, glints and tiers by depth (`CUSTOM0.x`) instead of distance from the falls: deep dark, shallow light; glints drift with the flow (`COLOR.rg`) in streams |
| froth | the froth's churn where a fall lands (`COLOR.a`) and over the last 0.8 m of each fall |
| shore | the lace web where it is shallow (depth under 0.15 m), not on streams |
| light | flat: the sun times its shadow (60 % in shade), no sky light, so a day shot keeps the Blender colours and the night is dark |

The slime river sample has no depth, so its pool is all shore (lace everywhere).

## The viewer (`liquids.tscn`)

Lit like the game (sky, filmic tonemap, glow). Keys: **1 – 8** or **← →** pick the shader, **M** the
sample (MC sample / slime river), **N** day / night, drag to orbit, wheel to zoom, **R** the start
view, **F** the whole map. At night only what a shader lights itself (`EMISSION`: the lava, the
poison's and the slime's glow) stays bright. `make liquid-shots` saves every shader on both samples;
`godot --path liquid -- --capture-liquids --only=slime` just one.
