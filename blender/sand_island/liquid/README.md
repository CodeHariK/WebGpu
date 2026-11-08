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
make liquid-sample    # export the sample meshes again from the game
make slime-bake       # bake the slime's textures in Blender (and import them)
make slime-river      # build the slime river sample in Blender (and import it)
```

## Folders

| Folder | What it holds |
|---|---|
| `water_mobile/`, `lava_mobile/`, `poison_mobile/` | the mobile look: steps 1 – 5 shared, lava's crust (6), poison's bubbles (7) |
| `water_desktop/`, `lava_desktop/`, `poison_desktop/` | the desktop look: refraction, absorption, caustics, glow, … (each file's header lists its parts) |
| `slime/` | `slime.gdshader` + `slime.tres` (the material with its textures); `slime_pattern.png`, `slime_slopes.png` (baked); `slime_bake.blend` (the node materials that make them); `slime_channels.png` (the four channels side by side) |
| `shader_lib/` | `liquid_noise` (hash, noise), `liquid_common` (the mobile set's steps 2 – 5), `liquid_desktop` (the desktop set's tools) |
| `sample/` | `water.res` + `terrain.res`: the game's water and the ground round it (its "last" inspect map); `slime_river.blend` + `.glb`: the slime river (Blender) |
| `tools/` | `export_liquid_sample.gd`: makes the MC sample (it runs inside the game project); `bake_slime.py`, `build_slime_river.py`: the Blender scripts (`make slime-bake`, `make slime-river`) |

No step-by-step tracks yet: each folder will get a `steps/` folder, as the sand island's shaders have.

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
