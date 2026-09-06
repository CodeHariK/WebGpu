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
- [x] `ConvexHullRockMesh` resource (roughness, flat bottom) + thin `ConvexHullRock` node
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
      - [ ] TERRAIN mode: heights from the deformer's baked profile (`_bake_terrain_profile`) so the
            mesh lies on the shaped roadbed; banking off in that mode
      - [ ] water preset: flat, no collision, scrolling cartoon shader, Area3D
      - [ ] presets for lane markings / edge stripes via UV (texture_length already there)
      - [x] `loafter/ProceduralRoad` retired: `procgen.tscn` migrated to `TerrainSplineRoad` (custom
            profile + adaptive), baked meshes removed from that scene (109 KB → 5 KB)
- [ ] **ProceduralLofter** (tunnels, tubes, tube-slides, bridge girders): saved-child bug fixed;
      later split `generate_lofted_mesh` (250 lines), drop the const_casts, add collision, rename folder
      `loafter` → `lofter`
- [ ] **TerrainSplinePainter** — write Terrain3D control map along a corridor (texture id + blend)
      - reuse the deformer distance field; road shoulders, riverbanks, mountain rock automatically
- [x] **TerrainSplineArray** — meshes at regular intervals along a spline: sides, offsets, tangent /
      inward facing, tilt, jitter, `stretch_to_ground` pillars (Terrain3D heights or raycast), MultiMesh +
      per-instance collision; demo pillars + rail posts on `TrackSpline`
- [ ] **River** = `CUT_ONLY` terrain-following deformer with negative `max_height` (riverbed) + water ribbon
- [ ] **Lake** — closed spline: cut-only deformer + flat water cap at `bottom_y` (reuse cliff cap builder)
- [ ] **Bridge** — ribbon with terrain-independent height + arrayed pillars
- [ ] Toon material for cliff slots (flat light response, rim/outline); the default StandardMaterial
      goes too dark on the shadow side

## Later

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
- [ ] Item boxes (respawn timer), coins on the track, checkpoint gates + lap counter + respawn,
      start grid + countdown, shortcut ramps that need a boost
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
- [ ] Day-night + weather zones (snow on the cliff `top_material`), waterfalls at cliff edges
      (ribbon shader), lava rivers (water preset, orange, damage), geysers on a timer, hot air balloons

Infrastructure these need: a `BeatClock` autoload, a `Respawnable` interface (position + rotation +
what to reset), an `Activator`/`Activatable` signal pattern (switch → any target), and an `InteractZone`
Area3D base with car / player filtering. Build those three first; the rest are mostly data.


- [ ] Procedural map generator: roads from a graph, rivers by downhill walk on the noise,
      mountains as blobs, towns as arrays → emits splines only
- [ ] Cliff extras: caprock/grass overhang over the wall, darker crack strips at column boundaries,
      finer columns preset, tunnels/portals through a cliff wall (skip wall between two arc lengths)
- [ ] Rock: subdivide + noise displacement pass for chunkier boulders; SDF rock via surface nets if
      realistic boulders are ever wanted
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
