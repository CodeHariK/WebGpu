* [I Added Realistic Water And Rivers To My Game - Village Builder Devlog #12
](https://www.youtube.com/watch?v=xWrk-0hW62U)
* https://watabou.itch.io/
* https://www.reddit.com/r/Unity3D/comments/1al1yku/terraced_terrain_generator_a_free_opensource_tool/
* https://gamedev.stackexchange.com/questions/115554/how-to-achieve-a-layered-terrain-simlar-to-godus


# TerraSpline roadmap

Cartoon open world for mobile: car racing + Zelda-like exploration. Everything is authored as
`ProceduralSpline3D` + components; a procedural map generator will later emit the same splines.
Rule to keep: **generators produce splines, components produce geometry.**

Legend: `[x]` done · `[ ]` next · `[~]` in progress · `[?]` idea, undecided

## Done

- [x] Streaming terrain (TerrainSplineCompositor): chunked, budgeted, async math, batched Terrain3D upload
- [x] Distance-field deformer pass (exact, bit-identical to legacy, 15× faster)
- [x] Deformer blend modes relative to base elevation (spline sits on the terrain surface)
- [x] Terrain-following roads: `HEIGHT_TERRAIN`, smoothing, grade limit, earthwork
      (cut & fill / cut only / fill only), depth caps, road blur, profile through other splines
- [x] Debug overlays: heightmap preview (H), live streaming map (M)
- [x] ~~`ConvexHullRockMesh`~~ retired → `RockMesh` / `Rock` (src/environment)
- [x] `TerrainSplineCliff`: free-standing stylised cliff/mesa — strata, ledges, bevel, columns,
      clefts, talus, profile curve (inverted / butte silhouettes), smooth top cap (grass/snow slot),
      trimesh collision, nothing saved but parameters
- [x] Demo presets: mushroom mesa (`CliffSpline`), Monument-Valley butte (`ButteSpline`)

## Next (in order)

- [~] **TerrainSplineRoad** — promote `loafter/ProceduralRoad` (Curve2D cross-section swept along the
      spline, banked via spline tilt, U/V with texture_length, adaptive sampling) into terraspline:
      - [x] SPLINE mode (floating, Mario Kart): presets slab / slab+rails / half-pipe / custom Curve2D,
            underside + caps, vertex colours per region, banking from spline tilt, fixed/adaptive
            stations, section windows (gaps = jumps), trimesh collision, internal children, small files
      - [x] demo `TrackSpline` (banked continuous loop over the mesas; jumps = two sections with a gap)
      - [x] TERRAIN mode: heights from the deformer's baked profile via `bake_road_profile` /
            `make_profile_context`; upright frames; demo `RoadSpline2/GroundRoad` sits +0.07 m on the bed
      - [x] water preset: `PROFILE_WATER`, Area3D (group water), built-in scrolling toon shader
      - [x] lane markings: `marking_lanes` (n−1 dividers, dashed or solid), `marking_edges`, width /
            dash / inset / colour — drawn by the built-in toon road shader from UV2 (deck-relative), no textures
      - [x] `loafter/ProceduralRoad` retired: `procgen.tscn` migrated to `TerrainSplineRoad` (custom
            profile + adaptive), baked meshes removed from that scene (109 KB → 5 KB)
- [ ] **ProceduralLofter** (tunnels, tubes, tube-slides, bridge girders): saved-child bug fixed;
      later split `generate_lofted_mesh` (250 lines), drop the const_casts, add collision, rename folder
      `loafter` → `lofter`
- [x] **TerrainSplinePainter** — Terrain3D control map along a corridor: `texture_id`, `strength`,
      `paint_curve`; shape from the sibling deformer's footprint (`compute_weight_field`, same distance
      field) or custom width/falloff; stacks (dominant texture kept as the other layer). Demo shader
      `stripe_toon_cheap` decodes it into `paint_color_1..4`; demo road shoulders, river banks, lake beach
      - [ ] later: share one distance field between a deformer and its painter (currently computed twice)
      - [ ] later: `stripe_toon.gdshader` (full) and `_cheapest` don't read the control map yet
- [x] **TerrainSplineArray** — meshes at regular intervals along a spline: sides, offsets, tangent /
      inward facing, tilt, jitter, `stretch_to_ground` pillars (Terrain3D heights or raycast), MultiMesh +
      per-instance collision; demo pillars + rail posts on `TrackSpline`
- [x] **River** — demo `RiverSpline`: cut-only Subtract deformer (bed) + water road at −0.9 m on the same profile
- [x] **Lake** — `TerrainSplineLake`: closed spline → Delaunay water sheet (`level_from_spline` /
      `water_level`, `depth`, `shore_offset`), Area3D group water; toon water shader moved to shared
      `tr_water.h/.cpp`; demo `LakeSpline` = fill-interior Replace deformer (basin) + lake
- [x] **Bridge** — demo `BridgeSpline`: SPLINE-mode slab+rails road over the river + `stretch_to_ground` pillars and rail posts (no new code)
- [x] **RaceTrack** (`src/racing/`) — SplineComponent: checkpoint gates (Area3D boxes every
      `checkpoint_spacing`, banked with the spline, gate 0 = start/finish, `show_gates` debug boxes),
      per-body progress (gates in order only), laps / `race_finished`, wrong-way (velocity vs segment
      direction for `wrong_way_time`), fall-off (`kill_depth` below the last gate → `respawn` upright
      `respawn_back` behind it, velocities cleared). Signals for HUD/audio. Demo `TrackSpline/Race`.
      - [ ] start grid + countdown (`track_body` enrols without crossing the line; needs a grid layout helper)
      - [ ] HUD: lap / position / wrong-way banner reading `get_progress` (ranking = lap + progress)
- [x] Toon material — `tr_toon.h/.cpp` `make_toon_solid_material()`: vertex-colour albedo, diffuse in
      `bands` steps down to `shadow_level` with a cool `shadow_tint` (never black), rim light, no
      specular; now the default for unset cliff slots (wall + cap) and roads. `make_toon_outline_material`
      (inverted hull, for `next_pass`) exists but is off by default — flat-shaded meshes crack at hard edges
      - [ ] later: expose bands / shadow_level / rim on the cliff and road nodes instead of needing a custom material

## Later

### Environment props (`src/environment/` — procedural meshes, semi-realistic like Odyssey / Link's Awakening)

Shared pattern (as ConvexHullRockMesh): `XxxMesh : ArrayMesh` generator (seed, size, variation params,
smooth or faceted normals, vertex colours or gradient) + thin `Xxx` node with `_validate_property` so
nothing generated is saved; a `PropMaterial` shared shader (smooth stylized shading, fresnel rim,
optional translucency / subsurface for crystals and leaves, gradient by height) — not flat cel.
Everything is placeable by `TerrainSplineArray` / `TerrainSplineScatter` and later the map generator.

Geology
- [x] Crystal cluster — `CrystalClusterMesh` / `CrystalCluster` + shared `make_prop_material()`
      (wrap diffuse, specular, fresnel rim, transmission, tip glow); demo `CrystalBig`, `CrystalBlue`.
      Docs: `src/environment/Environment.md`
      - [ ] later: cluster on a slope (align the base plane to the ground normal), LOD (drop pebbles far away)
- [x] Rocks — `RockMesh` / `Rock`: noise-displaced sphere or cube base (`roundness`), shear, tilt, planar
      facets, flat bottom, PILE / OUTCROP / STACK clusters for rocky cliff feet and mountains, crevice + strata + moss colouring,
      per-blob convex collision, `custom_mesh` slot; convex-hull rock and convhull_3d removed. Demo ×3
      - [ ] pebbles preset (tiny, many, via Scatter), stone pillars & arches, stalagmites for caves
      - [x] `SplineRocks` — rock wall / boulder line along a spline (rows, variants, ground snap, colliders); demo `RockWallSpline`
      - [ ] later: SplineRocks picking heights from a sibling cliff (rocks at the wall's foot automatically)
- [ ] Ice blocks / icicles, snow piles, lava rocks with emissive cracks
Vegetation
- [x] ~~Procedural trees~~ removed — trees / leaves / detailed foliage are authored in Blender and
      placed by `TerrainSplineScatter` / `TerrainSplineArray` (macro system places, Blender supplies detail)
- [ ] Bushes, hedges, tall grass / reed clumps, flowers patches, mushrooms (Odyssey-size), cacti
- [ ] Fallen logs, stumps, roots; lily pads, cattails, vines hanging from cliff lips
Man-made
- [ ] Fences (post & rail along a spline via Array), signposts, lanterns / torches, wells, barrels, crates
- [ ] Wooden planks / rope bridges, stone walls & ruins (pillars, broken arches), windmill, tents / stalls
- [ ] Racing set: tyre barriers, cones, checkpoint arch + banners / flags, grandstands, start-gantry lights
Water & sky
- [ ] Waterfall ribbon (spline road water preset, vertical), splash pool foam, geyser, fountain
- [ ] Clouds (soft blobs, drifting), floating islands (cliff loop + underside cone), rainbow arcs
Gameplay props (share meshes with interactors)
- [ ] Coins / moons / regional coins, treasure chests, ? blocks, breakable pots, hay bales

### Environment interactors (level "verbs"; each a small node, most reusable by both racing and exploration)

Movement / timing (Odyssey, 3D World, Galaxy)
- [ ] Beat blocks: red/blue platforms that flip on a global beat (one `BeatClock` autoload, blocks
      subscribe; racing variant: alternating road segments)
- [ ] Crumbling / falling platforms (touch → shake → fall → respawn); disappearing on a timer
- [ ] Moving platforms along a spline (reuse `ProceduralSpline3D`; loop / ping-pong / wait at ends)
- [ ] Rotating platforms, spinning cogs / gears (player carried by angular velocity), rolling logs
- [ ] Spring pads / bounce mushrooms, launch cannons (parabola preview line), zip lines along a spline
- [ ] Wind zones / updrafts (Area3D force), conveyor belts (surface velocity on a road segment)
- [ ] Seesaws / tilting platforms (RigidBody with limits), swinging pendulum platforms
- [ ] Sinking sand / mud (slow zone), ice (low friction surface), sticky honey walls

Hazards
- [ ] Lasers (rotating, sweeping, timed), Thwomp-style crushers, spike traps on a timer, flamethrowers
- [ ] Rolling boulders on a slope spline, Bullet-Bill launchers that home briefly, Chain-Chomp tether
- [ ] Poison / lava surfaces (respawn zone, cartoon splash), electric floors on a beat
- [ ] Wind-up fans that push cars off a ledge (racing), oil slicks / banana-style spinouts

Switches & gates (Link's Awakening, Zelda dungeons)
- [ ] Pressure plates / floor switches (weight, hold vs toggle), crystal switches (hit to toggle
      raised/lowered blue-orange blocks), timed switches with a ticking countdown and door
- [ ] Keys / small keys / locked doors, boss doors, one-way doors, shutter doors that close behind you
- [ ] Pushable blocks on a grid (sokoban puzzles), pull levers, torches to light (all four → door)
- [ ] Bombable cracked walls, cuttable bushes / tall grass with drops, liftable pots & rocks
- [ ] Warp pipes / holes (Odyssey 2-D sections), teleporter pads, warp points that unlock

Racing specific (Mario Kart, Diddy Kong Racing)
- [ ] Boost pads / dash panels, anti-gravity strips (up vector from the road frame — we have tilt),
      glider ramps + glide zones, underwater sections (slow + float)
- [ ] Item boxes (respawn timer), coins on the track, start grid + countdown, shortcut ramps that
      need a boost (checkpoint gates + laps + respawn: done, `RaceTrack`)
- [ ] Track hazards: swinging pendulums over the road, Thwomps on the straight, rolling barrels,
      cars/trains crossing (Toad's Turnpike), water splashes / mud slowing zones
- [ ] Destructible scenery (crates, hay bales), bumpers / pinball bumpers

Collectibles & exploration (Odyssey moons, Link's Awakening seashells, Banjo)
- [ ] Coins / regional coins, moons / big collectible with fanfare, hidden ? blocks, hint arrows
- [ ] Capture-style transformations → for us: car forms (boat, hover, monster truck) via zones
- [ ] Binoculars / lookout points, treasure chests, NPC stalls (buy cosmetics), photo spots
- [ ] Timer challenges (key → door in N s), rings-in-a-row, koopa freerunning race NPCs
- [ ] Flowers / grass that react (bend) to the car; birds that scatter; sheep to herd into a pen

Ambient / world
- [x] Day / dusk / night cycle — `SkyCycle` (`src/sky/`, extends WorldEnvironment): procedural toon sky
      shader (banded gradient, horizon glow, flat sun, invented ringed moon, twinkling stars), drives the
      sun DirectionalLight3D (→ dim coloured moonlight at night) + ambient from `time_of_day` / `day_length`;
      three tweakable phase palettes. Demo scene uses it. Docs `src/sky/Sky.md`
      - [x] dark night: low ambient + dark depth fog (distance → black), `SkyCycle::is_night()` hook; `Flashlight`
            (SpotLight3D, F toggle, flicker, battery, `auto_night`) — handheld on GameCamera + headlights on the
            vehicle; `StreetLamp` (pole + head + downward light, auto-on at night) — demo has three by the lake
      - [ ] later: second moon / ringed planet, drifting toon clouds, aurora, weather zones, gameplay hooks
            (headlights at night, shop hours); snow on cliff `top_material` by season
- [ ] Waterfalls at cliff edges (ribbon shader), lava rivers (water preset, orange, damage), geysers on a timer, hot air balloons

Infrastructure these need: a `BeatClock` autoload, a `Respawnable` interface (position + rotation +
what to reset), an `Activator`/`Activatable` signal pattern (switch → any target), and an `InteractZone`
Area3D base with car / player filtering. Build those three first; the rest are mostly data.


- [ ] Procedural map generator: roads from a graph, rivers by downhill walk on the noise,
      mountains as blobs, towns as arrays → emits splines only
- [ ] StylizedTerrain (self-generated, no Terrain3D — `src/terrain/stylized/Stylized.md`): terraced
      (marching-squares contour extract → extrude, `get_contours()` exposes the loops) + faceted done
      (`make run_stylized`). Next up:
      - [ ] Spline-deformer shaping: drive the height field with placed `ProceduralSpline3D` +
            `TerrainSplineDeformer` (reuse `blend_height` + `evaluate_spline_point_segmented`) so roads /
            cliffs / lakes / painter tooling work on this backend as they do on Terrain3D
      - [ ] Chunked streaming around the player (as the TerraSpline compositor does) instead of one patch
      - [ ] Cap cleanup: min-area cull for the tiny single-cell nubs where height just tips a band
      - More styles on the same generator (all heightmap, no overhangs, mobile-cheap):
      - [ ] Faceted low-poly hills (Alto's Odyssey / Monument Valley): smooth noise, vertices snapped to a
            coarse grid, flat-shaded (per-tri normals) — no quantization, big flat tris catching light.
            Cheapest stylization; basic version already the `terrace=false` path, add coarse-snap control
      - [ ] Hex / tile plateaus (Islanders / board-game): sample the field on a hex or square lattice,
            flatten each cell to one quantized height, short vertical sides — terracing at 1-cell res;
            placement / turn-based feel. Borrow lattice topology from `marching_prism`
      - [ ] Mesa / plateau sculpting (BOTW Great Plateau / Odyssey): keep smooth terrain but clamp slope —
            flatten below a threshold, steepen above — flat meadows meeting near-vertical walls. Natural
            partner to `TerrainSplineCliff` (gives it flat tops to hang walls off). NOTE: pure slope-clamp
            just rounds smooth fbm (no walls to keep); the look needs the steps created — e.g. quantize
            with a slow per-region height offset (varied-height plateaus) + a bilateral flatten of the tops
      - [ ] Dunes / ridged desert (Journey): feed the existing ridged fbm (`prop_geometry.h`) into the
            field for sharp ridges / soft troughs; sand shading via a slope/height gradient. Nearly free
      - [ ] Painterly banded slopes (Ghibli / Wind Waker): smooth hills, colour by height + slope bands in
            the shader (grass low, rock on steep faces, snow high) as flat cel bands not blends. A shared
            material layer over any style, not a separate terrain
      - [ ] Patchwork farmland (Animal Crossing / Stardew): gentle terrace steps + checkerboard/field cell
            tint for cultivated land — the lived-in village look
      - [ ] Floating shelf islands (Skyward Sword): discrete heightmap patches with a skirt/flat bottom,
            separated by gaps — no overhangs, just island chunks at different elevations; distinct zones
- [ ] Cliff extras: caprock/grass overhang over the wall, darker crack strips at column boundaries,
      finer columns preset, tunnels/portals through a cliff wall (skip wall between two arc lengths)
- [ ] Scatter: exclusion from road/river corridors (needs the ribbon's mask), grass via Terrain3D instancer
- [ ] Frozen benchmark scene (the live demo keeps changing, so the golden hash no longer means much)
- [ ] `make format` over the whole tree as one separate commit

## Open questions / bugs

- [?] Editor hang once when setting `max_cut_depth = 0` — not reproducible headless (game or
      editor); need console output or an Activity Monitor sample if it recurs
- [?] Cap triangulation is unconstrained Delaunay clipped by centroid: a very sharp concave rim
      notch could leave a gap; constrained triangulation if it ever shows

## Conventions

- `make debug` builds; `make format` / `make format-check` (clang-format, 120 cols, one parameter per line)
- Bench: `godot --headless --scene scene/terraspline/chunked_terrain_demo.tscn -- --terraspline-bench`
- Architecture and per-flow call trees: `src/terrain/terraspline/Terraspline.md`
