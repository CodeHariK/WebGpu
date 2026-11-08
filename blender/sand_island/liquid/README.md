# Liquid Shaders

The liquid shaders of the minecraft game's MC water: water, lava and green poison, each in a
simple **mobile** look and a no-budget **desktop** look. They are worked on here, one idea at a time,
and the parts that are liked go back into the game (its copies:
`minecraft/project/material/shaders/terrain/liquid/`, the mobile look only).

It is its own Godot project (Forward+, like the game), not part of `../water_tutorial/`: these
shaders run on the game's MC water mesh, not on the sand island's water.

```
make liquids          # the viewer (from the folder above: blender/sand_island/)
make liquid-shots     # shots/liquid_<shader>.png, by day and by night
make liquid-sample    # export the sample meshes again from the game
```

## Folders

| Folder | What it holds |
|---|---|
| `water_mobile/`, `lava_mobile/`, `poison_mobile/` | the mobile look: steps 1 – 5 shared, lava's crust (6), poison's bubbles (7) |
| `water_desktop/`, `lava_desktop/`, `poison_desktop/` | the desktop look: refraction, absorption, caustics, glow, … (each file's header lists its parts) |
| `shader_lib/` | `liquid_noise` (hash, noise), `liquid_common` (the mobile set's steps 2 – 5), `liquid_desktop` (the desktop set's tools) |
| `sample/` | `water.res` + `terrain.res`: the game's water and the ground round it (its "last" inspect map) |
| `tools/` | `export_liquid_sample.gd`: makes `sample/` (it runs inside the game project) |

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

## The viewer (`liquids.tscn`)

Lit like the game (sky, filmic tonemap, glow). Keys: **1 – 6** or **← →** pick the shader, **N** day /
night, drag to orbit, wheel to zoom, **R** the start view, **F** the whole map. At night only what a
shader lights itself (`EMISSION`: the lava, the poison's glow) stays bright.
