* https://lettier.github.io/3d-game-shaders-for-beginners/index.html
* https://fortnite.gg/map-evolution
* https://i.insider.com/5b47c2667708e960932a27d6?width=1148&format=jpeg&auto=avif&quality=85%2C80
* Volcano/Crafter with particle effects/Floating rocks on lave/water
* overcooked hex map
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

## Vehicles (ArcadeVehicle forms)

`ArcadeVehicle` (src/vehicle) is the one general vehicle class — every kind is a preset + a
locomotion mode on it, not a new class (existing: foliotoycar, driftcar, monstercar, mariokart,
forzacar). These three change *how the body moves*, so each needs a locomotion mode (an HSM state
+ config flags), not just a `.tres`. Design doc: `docs/Vehicle.md`. Test in `run_octo` (`CAR=<name>`).

- [ ] **Boat** — water locomotion: no ground raycast/suspension while over a `water` Area3D; buoyancy
      to the water level (reuse the water Area3D group from River/Lake), planing at speed, water drag +
      turn-from-rudder (steer scales with speed, none at rest), bank into turns. Wake + spray particles,
      bob at idle. Preset `boat.tres`; add a `LOCOMOTION_BOAT` mode + a water-volume probe.
- [ ] **Bike** — two-wheel: front/rear wheel only, lean into turns (visual + a real roll that feeds
      lateral grip), self-balance upright when grounded, wheelie on hard accel / stoppie on hard brake,
      narrower collider. Countersteer feel, tighter turn radius than the cars. Preset `bike.tres` + a
      `LOCOMOTION_BIKE` mode (2 wheels, lean controller).
- [ ] **UFO** — hovering flight: no wheels/suspension, omnidirectional thrust, altitude hold + raise/
      lower, tilt-to-move (pitch/roll toward travel), yaw spin, gentle hover bob, ignores terrain contact
      (clamped min ground clearance instead). Optional tractor-beam hook later. Preset `ufo.tres` +
      a `LOCOMOTION_HOVER` mode.
- [?] shared: a `LocomotionMode` on VehicleConfig (wheeled / boat / bike / hover) so the state machine
      picks the ground-contact + steering model from config; wheel count from the preset's WheelConfigs.
- [?] transform zones (Odyssey capture): drive through a zone to swap the active preset/form
      (ties into the "car forms via zones" idea under Collectibles below).

## Characters — rig & animation

Character docs: `docs/Characters.md`, `docs/CharacterMovement.md`.

- [x] Spring-body character (physics, replaces kinematic Celeste) — ONE class `SpringCharacter :
      RigidBody3D` (`src/character/`, no shared base). A faithful Very Very Valet toy controller:
      PD ride-spring floats it and pushes the ground body back (presses on platforms/cars); PD upright
      torque-spring keeps it vertical + faces travel (tips and recovers); goal-velocity move with a
      clamped acceleration force (snappy but weighty); camera-relative input. Mario jump on top
      (coyote, buffer, variable height, light-up/heavy-down gravity). Verified stable: floats upright,
      ~18 m/s, jump cut works, presses the world. tested in the octo scene as a
      GameManager target (TAB-switchable, camera auto-TPS).
  - **`ArcadeVehicle` is intentionally left untouched** — no shared base. If the same spring bug ever
    shows up in both, extract just a `cast_spring()` helper; the car keeps its own suspension/steer.
  - [ ] tune the toy feel to match VVV exactly (ride/upright spring constants, accel curves, jump arc)
  - [x] moveset state machine (`src/character/ai/`, mirrors ArcadeVehicle's `ai/`): `CharacterState` base +
        grounded/airborne parents; walk / fall / wall-climb / dash / ground-pound states, with double
        jump and wall jump handled in the owner's centralised transitions. Builds + headless smoke
        test pass (settles, ~17.6 m/s, jumps, lands). Feel of the new moves still needs interactive tuning.
  - [x] physics-feel pass: deterministic velocity control (engine gravity off, self-integrated
        asymmetric Mario arc, planted ride-servo, still pushes the car); raycast wall system
        (multi-ray detect, precise hug, auto-mantle onto ledges, wall-slide); precision layer
        (Celeste corner-correction + hard-landing snap). Prototype/Hulk vertical wall-climb.
  - [x] ported from Celeste: sprint (Shift / full stick x sprint_multiplier), moving-platform carry,
        melee jump-kick (C -> lunge to EnemyManager's best target, damages EnemyBase, refunds dash/air-jump).
  - [x] live tuning UI (`src/character/character_ui.*`, ported from CelesteUI on the shared CUI):
        "Character" button -> tabbed slider panel (Move/Jump, Ride/Abilities, Combat/Feel) bound to the
        tunables, Save/Load to `user://character_settings.cfg`, real-time speed graph.
  - [ ] still to add: a dedicated pound button (air-dash vs ground-pound currently split by whether a
        direction is held); interactive feel tuning via the new sliders.
  - [ ] retire `CelesteController` once this reaches the feel we want

- [ ] Character IK — adopt Godot 4.6+'s new `SkeletonModifier3D`-based IK framework (the old
      `SkeletonIK3D` node is deprecated). Drop solvers into the skeleton's modifier stack, so IK layers
      on top of played animations instead of replacing them:
      - `TwoBoneIK3D` — arms / legs (fast analytic solver)
      - `FABRIK3D` / `CCDIK3D` / `JacobianIK3D` — longer chains
      - `SplineIK3D` — tails, spines, tentacles
      Uses (targets are any Node3D):
      - [ ] foot planting — feet raycast to the terrain/road surface so they sit on slopes & steps
      - [ ] hands snap to held items (Camera / Guitar / Mic / Bat / Choco Gun / Dagger in Characters.md)
      - [ ] look-at / aim — head + arms track the aim target (dropkick, shoot, throw)
      - [ ] twist + angular-velocity constraints per joint so elbows/knees don't over-rotate
      All engine classes → drivable from GDExtension C++. Prototype on the Celeste controller first.

- [ ] Procedural leg coordination (t3ssel8r robot) — a gait *coordinator* that drives the IK feet
      directly, no keyframed walk cycle. Each leg has a home anchor offset from the body; while a foot
      is planted it stays pinned in world space and the body slides over it, and when it strays past a
      trigger distance the coordinator eases it in a quick step to a new anchor predicted ahead along
      the velocity (the t3ssel8r "step to where it's going" trick). The coordination logic is the AI
      part: it sequences which legs may lift so a support set is always planted (never lift neighbours
      together), keeping the body balanced. Body height + tilt ride a spring over the mean foot position.
      Build on the `TwoBoneIK3D` foot targets from the IK task above; scales biped → quadruped → hexapod/robot.
      - [ ] per-leg step state machine (planted / lifting), stride length + step-trigger distance,
            speed-scaled cadence
      - [ ] gait patterns: biped alternating, quadruped diagonal trot, hexapod tripod
      - [ ] body PD spring follows mean foot pos and tilts to the foot plane (reuse `StatelessSpring` /
            `pd_torque_align`)
      - [ ] anticipation: predict the next anchor ahead along travel so faster movement takes longer strides

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
- [ ] Capture-style transformations → for us: car forms (boat, bike, hover/UFO, monster truck) via
      zones — swap the active `ArcadeVehicle` preset/form (see **Vehicles (ArcadeVehicle forms)** above)
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

# Platforms & puzzle rooms

Mario / Donkey Kong platform pieces plus a Dino-and-Aliens (2004) style puzzle loop: kill the
aliens, solve laser / box puzzles, find the key, escape. Rule to keep: **deterministic and readable**
— every timed piece ticks from one shared beat clock (per-object offset) and telegraphs before it
acts; puzzle objects snap to a grid. Reach for platform gaps comes from `JumpMetrics` (F3 rings).

## Puzzle redesign — kid-friendly dungeons (Link's Awakening structure)

The game is for children: fun and always solvable, never 15 mechanics at once. Lab = `2d_map`
Map19 (Puzzle Islands); port to Godot (`src/puzzle/`) once the rules settle.

**Principles**
- One new mechanic at a time: teach it alone (safe room) → test it → twist it (combine with one
  known mechanic). Late levels still use everything, but each ROOM holds 2–3 ideas max.
- Two separate dials: **progression** (which mechanics are unlocked — only goes up, one per
  world) and **difficulty** (how hard each room is — rises within a world, drops when a new
  mechanic arrives).
- Goal is always: collect every gold key → escape. Aliens / enemies are optional bonus
  (stars / treasure), never required for keys.
- Never stuck: room reset on re-enter, enough ammo, undo, 3 hearts, no instant deaths, slow
  readable enemies.
- Dino & Aliens 2004 reference: study by PLAYING (notes + screenshots per level: room size,
  new mechanic, box / alien counts, enemy behaviour, solve time) — don't decompile or copy its
  code (copyright); turn the notes into our own generator templates.

**Progression worlds** (each world's first level teaches its mechanic alone)
- [ ] W1 crates + gold keys + exit  · W2 box bombs + aliens  · W3 enemies (patroller, red)
- [ ] W4 lasers + mirror crates  · W5 teleporters  · later: coloured gates, wooden doors,
      plates, batteries, pulsing lasers, yellow / blue enemies (one per world)
- [ ] Parked in the lab behind flags (not in early worlds): battery slots / swapping, pulsing
      lasers + laser damage per second, laser channels, throw / remote bombs, melee + enemy hp,
      wooden walls, dead emitters, yellow sentry, blue laser enemy

**Dungeon structure (Link's Awakening)**
- [ ] Level = small dungeon: room tree; each room is ONE self-contained puzzle with a clear
      reward (door opens, small key, chest); gold key = boss key opening the exit
- [ ] Room-puzzle library, each type built + verified on its own (fast solver, small searches):
      push (crate → plate), clear-the-room, bomb (box bomb → alien), laser (mirror → receiver),
      teleporter, gate / key
- [ ] Dungeon graph: locked doors on routes between rooms, keys as room rewards, shortcuts /
      one-way doors back to earlier rooms
- [ ] **Room reset**: leaving a room puts its crates back (replaces global undo; no soft-locks)
- [ ] Hints: statue / glow showing the next useful crate or switch (from the solver)

**Room roles + pacing** (intensity curve, not uniform difficulty)

Rooms shouldn't all be equally hard. Good dungeons follow an intensity curve: calm, harder, a
rest, then a peak at the end. Kids get a rhythm of tension, then relief.

| Room role      | Enemies            | What it's for                                          |
|----------------|--------------------|--------------------------------------------------------|
| Start / safe   | 0                  | Orientation, maybe a pickup                            |
| Puzzle         | 0–1 slow           | Crates, lasers or keys — thinking without pressure     |
| Combat (arena) | 2–5                | Clear the room → a door opens or a key drops           |
| Mixed          | 1–2                | A puzzle while dodging a patroller                     |
| Rest / reward  | 0                  | Hearts, ammo, treasure after a hard room               |
| Final          | peak for the level | Last challenge before the exit                         |

- [~] Room roles (table above) as a generator concept — lab: `Map19Dungeon.ts` (World 1: start /
      puzzle / rest / bonus / final; combat + mixed come with enemies)
- [~] Generator picks a role sequence along the route (e.g. start → puzzle → combat → rest →
      mixed → final); level difficulty sets how high the PEAKS go, not how many enemies are
      spread everywhere
- [ ] Threat budget per room = Σ enemy weights (patroller 1, red 2, …) within its role's range
- [ ] Combat rooms optional where possible (sneak past; clearing gives stars / treasure)
- [ ] Enemy speed scales with world (biggest kid-friendliness lever), not just enemy count
- [ ] Armoured enemy (bomb-only) to give bombs a clear purpose

**Ammo scales with threat** — rule: you're always given enough to win
- [ ] Threat per room = Σ enemy weights (patroller 1, red 2, …)
- [ ] Bombs ≈ 1.5 × enemies that need them, placed in / just before the combat room (a child
      can miss a throw and still win)
- [ ] Refill crate just outside every combat room (run out → get more; nobody gets stuck)
- [ ] Dynamic help: low hearts → more heart drops (kids won't notice, it just feels fair)
- [ ] Bombs vs puzzles — more bombs = more ways to blow up a crate the puzzle needs. Fixes:
  - [ ] **Metal crates** (bomb-proof steel) in puzzle rooms; only wooden crates in combat rooms break
  - [ ] **Room reset**: leave + re-enter → room back to how it started; nothing permanently ruined
- [ ] Generator verifies combat rooms: enough bombs reachable before the room, threat within
      the budget for its role, a safe spot by the door

**Generator UI panel** (grouped, collapsible; checkbox = allowed, stepper = count, slider = amount)
- [ ] LEVEL: preset dropdown (World · Level → fills everything, then override), seed + 🎲 + 🔒,
      size W / H / rooms, gold keys, "teaching level" (new mechanic only)
- [ ] MECHANICS: checkbox + count per mechanic; **max mechanics per room** stepper (2–3)
- [ ] ENEMIES: density (1 per N floor cells, scales with size) or exact counts per type;
      speed multiplier; threat budget per room role
- [ ] PICKUPS: hearts, bombs, ammo generosity slider (1.0–2.0× bombs per threat, default 1.5)
- [ ] PACING: role sequence (auto from difficulty or hand-picked) + peak intensity slider
- [ ] DIFFICULTY: target slider → generate N candidates, keep the closest; estimated score bar
      (sokoban / mechanics / enemies) + solution length in moves; badges: ✓ solvable,
      ✓ every mechanic needed (necessity check), generation time
- [ ] PACING (UI): per-level role sequence (or auto from difficulty) + peak-intensity slider
- [ ] ENEMIES (UI): one threat budget per room role instead of a single global count
- [ ] AMMO (UI): generosity slider 1.0–2.0× bombs per threat, default 1.5×
- [ ] 1. **Lock seed / favourites**: star good seeds and build a level list (campaign editor)
- [ ] 2. **Copy / paste layout as text**: export a level as ASCII, hand-tune it, paste it back
      (tutorial levels; porting to Godot)
- [ ] 3. **Solution overlay**: the solver's path as a dotted line; step through it
- [ ] 4. **Danger heatmap**: shade cells by how many enemies cover them — is the route fair?
- [ ] 5. **Playtest stats**: time, undos, deaths, hearts lost per attempt — with a child
      testing, shows where they ACTUALLY get stuck (matters more than the computed score)
- [ ] 6. **Star thresholds**: finish in N moves for 3 stars, from the solver's best solution
      (replay value without adding mechanics)
- [ ] 7. **Hint button**: highlight the next useful crate / switch, taken from the solver
- [ ] 8. **"Why is this hard?"**: hover the difficulty bar → which room and which mechanic adds most
- [ ] Generation in a Web Worker (difficulty 8+ takes 3–5 s and freezes the page)

**Order** (all of this depends on the room-based dungeon generator):
1. Room roles + the pacing sequence
2. Threat budgets per room
3. Ammo placed relative to threat
4. Room reset + metal crates
5. UI controls on top (sections + presets → target difficulty → copy / paste → solution overlay)

## Puzzle rooms roadmap (Dino and Aliens + Luigi's Mansion + Resident Evil)

(History — superseded by the kid-friendly redesign above; water / lava / ice were removed.)
Forest islands: rooms are tiny islands (water / lava around), some locked behind keys found in
earlier rooms. **Level difficulty = Sokoban difficulty (solver pushes / states) + number of
mechanics (mirrors, traps, ice) + Σ enemy weights**; each enemy class adds its own puzzle rule.

- [x] PuzzleGrid: ASCII layout, push boxes (grid-snapped, no pull), props, jump locked in rooms
- [x] SokobanGenerator + SokobanSolver: seeded BSP rooms, doorway crates, key / exit / aliens /
      box bombs, verified by BFS over pushes
- [x] **1. Cell types + island rooms**: `~` water, `^` lava, `=` bridge, `I` ice;
      push a box into water → it sinks and becomes floor (bridge); into lava → it burns (lost);
      on ice a box slides until blocked. Generator: island rooms ringed by water/lava, some
      doorways are water gaps that need a crate pushed in, small pools; solver understands all
- [~] **2. Lock & key graph** (Zelda / RE) — coloured keys + gates done in the 2d_map lab (Map19): coloured / shaped keys + matching locked doors,
      placed so every key comes before its lock; one-way shortcut doors opened from the far
      side; locked doors visible early (backtracking)
- [~] **3. Grid enemies with one readable rule each** — lab (Map19, real time): patroller, red
      melee (dumb, slow), yellow shooter (row/col bullets, finds firing spots), blue 360° laser
      (pathfinding, continuous beam); player 3 hearts (bullet 1, bite 1, laser 0.5/s); range circles.
      Ideas still open: (Hitman GO / Lara Croft GO / Into the Breach,
      always telegraph the next move): patroller (fixed loop), chaser (blocked by boxes),
      armoured (bomb / laser only), sleeper (vision line), sound hunter (comes to box pushes,
      RE Licker), hider in crates (Luigi Boo), light-shy (stunned by laser / mirror), stalker
      across rooms (RE Mr. X)
- [~] **4. Difficulty score → generator budget** (pushes + states + mechanics + enemy weights) —
      lab: score with enemy-on-route weighting, tiers, DIFFICULTY 1–10 budget slider; next: port to C++
- [ ] **5. Mechanics batch** below (laser / mirror, timed spikes, bombs on the beat clock)
- [?] Ideas: Luigi suck / pull tool (limited uses, fixes "stuck forever"), dark rooms lit when
      cleared (key / chest appears), clear-the-room-to-unlock; Zelda crystal switches (red / blue
      blocks), torch lighting order; Bomberman chain reactions; tiny hand-made tutorial rooms
      (Stephen's Sausage Roll / Baba Is You)

## Puzzle batch (first) — one complete test room

- [~] **Beat clock** (lab: pulsing lasers): one global timer; lasers, spikes and on/off blocks read it with an offset so
      patterns repeat exactly and players learn the rhythm
- [~] **Laser emitter** (lab: always-on + pulsing; burns player, kills enemies): always on or timed (e.g. 2 s on / 1 s off), flicker warning before firing;
      beam is a raycast chain (≤ 8 bounces), hurts player and aliens, stopped by boxes
- [~] **Mirror** (lab: pushable mirror box, E flips, solver-aware): reflects 90°; rotatable (hit it to turn 90°) or pushable on the grid
- [~] **Laser receiver** (lab: lights → laser gate H): beam on it → opens a gate / moves a platform
- [ ] **Timed spikes**: up/down on the beat, wobble warning before rising
- [ ] **Push box (Sokoban)**: grid-snapped, one cell per push, no pull, blocks lasers
- [~] **Pressure plate** (lab: crate on plate → plate gate J): holds a gate open while a box or the player stands on it
- [ ] **Key + exit door**: key appears when all aliens are dead (or sits behind the puzzle)
- [ ] Small test room combining all of the above
- [~] **Batteries + slot emitters** (lab: E puts a battery in / takes it out; batteries move between emitters; solver-aware)
- [~] **Mirror boxes** (lab: pushable, E flips; lasers detonate box bombs)
- [~] **Goal = all gold keys + escape** (lab: several gold keys, aliens are a bonus; solver collects every key)
- [x] **Removed water, lava and ice from the lab** (none of them made a puzzle; the island ring is now hedge)
- [~] **Teleporter pads** (lab: pairs; sealed key room / doorway replacement / gate-free shortcuts; solver-aware)
- [~] **Generated laser puzzles** (lab: mirror chain with an off-line mirror to push back, laser through a wooden wall;
      per-gate laser channels O/H · o/U · 0/V)
- [~] **Bomb doors** (lab: enemies can't pass; blasts break them; seal reward side rooms or act as shortcuts)
- [~] **Thin wooden walls** (lab: block walking / crates / bullets, lasers pass; blasts break them)
- [~] **Player kit** (lab): melee swipe (knockback, hp per enemy), bombs from ammo pickups —
      time / throw / remote; cross blasts break crates, kill enemies + aliens, chain box bombs,
      hurt the player; lasers detonate bombs; heart pickups

## Bomberman batch (Dino and Aliens plays like Bomberman)

- [ ] **Grid arena**: indestructible pillars (checkerboard) + destructible soft blocks;
      power-ups and the key / exit hidden inside soft blocks
- [ ] **Bomb** (one "drop bomb" button, mobile friendly): ~3 s fuse, + shaped blast of N cells,
      stopped by pillars, breaks soft blocks, chain-reacts other bombs, hurts player and aliens
      (reuse mortar `_blast` + puff bursts); blast can clear boxes that block a laser
- [ ] **Wandering aliens** on the grid: simple patterns (random turn / chase / wall-pass)
- [ ] **Exit door** opens only when every alien is dead (EnemyManager)
- [ ] **Power-ups**: bomb count, fire range, speed, kick (reuse grid push box),
      throw (reuse lift & throw); later: remote detonator, bomb-pass / wall-pass

## Platform batch (second)

- [x] Moving platform
- [ ] **Crumbling platform**: shakes ~0.5 s after landing, drops, respawns
- [ ] **On/off blocks** (3D World beat blocks): two colour sets swap solid ↔ ghost on the beat clock
- [ ] **Bounce pad / mushroom**: fixed-height launch (height from `JumpMetrics` maths)
- [ ] **Barrel cannon** (DK): enter, it aims / rotates, jump to blast out on a fixed arc
- [ ] **One-way / cloud platform**: jump up through, land on top
- [ ] **Conveyor belt**: pushes player and boxes along
- [?] Tilting seesaw / rotating platform (physics-y, less deterministic — later)

Here are the mechanics I'd add. Each one works with the crates, and most also work with the enemies, which is where the interesting puzzles come from.

**Light and lasers**
- **Laser emitter:** always on, or on a beat (for example 2 s on, 1 s off) with a flicker warning before it fires. It hurts the player and enemies, and crates block it.
- **Mirror crate:** a pushable crate that turns the beam 90°. Bumping it rotates it.
- **Receiver:** a beam hitting it opens a gate. Two receivers can need two beams at once.
- **Prism:** splits one beam into two.
- **Glass block:** you can't walk through it, but lasers and bullets pass through.
- **Lure kill:** walk an enemy across a beam to destroy it.

**Switches and logic**
- **Pressure plate:** keeps a gate open only while something stands on it. A crate, the player or an enemy all count, so you can bait a red onto the plate.
- **Crystal switch (Zelda):** hitting it swaps which of the red and blue blocks are raised.
- **Lever:** a one-time toggle, used for one-way shortcuts back to earlier rooms.

**Floor**
- **Pit:** fill it with a crate like water, but it's deadly to walk into, and enemies fall in too.
- **Crumbling floor:** turns into a pit after you walk over it once. Routes become one-way.
- **Conveyor belt:** moves crates, the player and enemies one cell per beat.
- **One-way arrow tile:** you can only cross it in one direction.
- **Timed spikes:** rise and fall on the beat, with a wobble warning before rising.

**Movement**
- **Teleport pads:** paired pads that move the player, crates and bullets.
- **Fan or wind:** pushes crates down a lane, like ice but only in one direction.

**Bombs**
- **Cracked wall:** an exploding box bomb opens it, which gives you a shortcut or a secret room.
- **Chain reactions:** a laser sets off a bomb, and bombs set off their neighbours (Bomberman).
- **Fuse tile:** a bomb pushed onto it starts a countdown.

**Rooms**
- **Dark room:** you only see a small radius around yourself until you light the torches. Enemies hide in the dark (Luigi's Mansion).
- **Clear-the-room:** the doors lock until every enemy is gone, then the key appears.

All the timed pieces (lasers, spikes, conveyors, crystal blocks) should run on one shared beat clock, so their patterns repeat exactly and players can learn the rhythm.

**What I'd build first:** the beat clock, laser emitter, mirror crate, receiver gate and pressure plate. That's the "Puzzle batch" already listed in `TODO.md`, and the laser also gives you a way to kill enemies. Pits, conveyors and cracked walls would come after that.

Should I start the laser and mirror batch in the Map19 lab, with the generator placing them, or would you rather start with a different group?
