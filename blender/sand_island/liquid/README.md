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
| `tools/` | `export_liquid_sample.gd`: makes the MC sample (it runs inside the game project); `bake_slime.py`, `build_slime_river.py`: the Blender scripts (`make slime-bake`, `make slime-river`) |

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

## The viewer (`liquids.tscn`)

Lit like the game (sky, filmic tonemap, glow). Keys: **1 – 7** or **← →** pick the shader, **M** the
sample (MC sample / slime river), **N** day / night, drag to orbit, wheel to zoom, **R** the start
view, **F** the whole map. At night only what a shader lights itself (`EMISSION`: the lava, the
poison's and the slime's glow) stays bright. `make liquid-shots` saves every shader on both samples;
`godot --path liquid -- --capture-liquids --only=slime` just one.
