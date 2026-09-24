# Folio-2025 → Godot C++ Port — TODO

> **Naming convention:** every ported class is prefixed `Folio` (FolioGame, FolioTicker,
> FolioEvents, FolioTime, FolioViewport, FolioQuality, FolioResources, FolioView + its
> FolioView* helpers). Apply this to ALL future ported classes. Filenames stay lowercase;
> `Game/*.js` citations in comments are the JS source and keep their original names.

Porting the reusable game tech from Bruno Simon's folio-2025 (Three.js r183
WebGPU / TSL + Rapier) into this Godot 4 GDExtension project.

- **Scope:** reusable tech only — engine spine, shared visuals, render pipeline,
  car, environment, audio, UI framework. **Skip** portfolio content (Areas:
  Career/Projects/Lab/Bowling/Cookie, Achievements, BlackFriday, Easter).
- **Target:** this `minecraft` project (reuse GDExtension build, ArcadeVehicle,
  TerraSpline).
- **Fidelity:** match the WebGPU look 1:1 (Forward+ desktop first; mobile cost later).
- **Source of truth:** `/Users/Shared/Code/WebGpu/folio-2025/sources/Game/`

## Conventions
- One responsibility per file/function; small files; thorough doc comments.
- clang-format, one parameter per line.
- Each ported system gets a catalog entry (template at bottom) before code.

---

## Tier 0 — Engine spine  (nothing renders until this exists)
- [x] `Ticker` — single per-frame tick with **integer priority ordering**  → catalog below
      (inputs low → physics pre(2) → physics post(5) → gameplay → render(998)).
      Do NOT rely on Godot per-node `_process` order; build an explicit sorted bus.
- [x] `Events` — ordered pub/sub bus (RefCounted; `src/folio/events.{h,cpp}`).
- [x] `Game` → `FolioGame` — composition root; builds+wires the spine (`src/folio/game.{h,cpp}`).
- [ ] (later) `Game` boot orchestrator — construct subsystems in folio's init() order;
      staged resource load (intro batch → full batch → physics).
- [x] `Time` → `FolioTime` (time scale + bullet-time; `src/folio/time.{h,cpp}`).
- [x] `Viewport` → `FolioViewport` (size + DPR + resize events; `src/folio/viewport.{h,cpp}`).
- [x] `Quality` (quality tier 0/1 + `change` event; `src/folio/quality.{h,cpp}`).
- [x] `ResourcesLoader` → `FolioResources` (keyed batch loader; `src/folio/resources.{h,cpp}`).
- [x] `View` (camera) — SPLIT into `src/folio/view/` (orchestrator + 6 helpers; deferred trio remains).
- [x] Global shader clock ✅ — `FolioTicker` publishes `folio_elapsed` / `folio_delta` /
      `folio_elapsed_scaled` / `folio_delta_scaled` as `RenderingServer` global shader
      uniforms every frame (`_register_shader_globals` / `_update_shader_globals`).
      `*_scaled` carry the 2× world scale + bullet-time (driven by `FolioTime` → ticker
      `scale`). Consumers: `water_surface` reads `folio_elapsed_scaled`; grass/flowers/
      foliage read `folio_wind_time` (FolioWind's sub-clock, accumulated from
      `delta_scaled`); rain/wind lines accumulate `local_time` from `get_delta_scaled()`.
      No folio shader uses raw `TIME`. (Raw `scale` isn't exported as its own uniform —
      it's baked into the `*_scaled` timelines; add `folio_scale` only if a shader ever
      needs to read it directly.)

## Tier 1 — Shared visual state  ★ HIGHEST FIDELITY RISK
Analyze these together as ONE unit — they define the whole look.
- [x] Catalog `MeshDefaultMaterial` output pipeline: light bounce → water tint →
      lambert light → core+drop shadow → fog → reveal discard.
- [x] Shared uniform owners: `FolioLighting` ✅ · `FolioFog` ✅ · `FolioReveal` ✅ · `FolioWater` ✅ · `FolioNoises` ✅ · `FolioTerrain` ✅. **All Tier-1 uniform owners complete.**
- [x] **Keystone:** base Godot spatial shader `material/shaders/folio/mesh_default.gdshader`
      reproduces the full pipeline against the global uniforms, incl. real cast (drop)
      shadows via a `light()` pass ✅. Refactored into a shared `#include` ✅.
- [x] `MeshGridMaterial` ✅ (standalone unlit triplanar grid).

## Tier 2 — Render pipeline
- [x] `FolioRendering`: native glow bloom + cheap-DOF tilt-shift, quality-switched ✅.
      Map to `WorldEnvironment` glow + custom DOF `CompositorEffect`/post shader.
- [x] Day `Cycles` ✅ (`FolioCycle` interpolator + `FolioDayCycles` → Lighting/Fog/Reveal).
- [x] `Wind` ✅ (`FolioWind` shared sway field; consumed by Grass).
- [x] `Weather` ✅ (`FolioWeather` — noise-driven temperature/humidity/clouds/wind/rain/snow
      globals; drives Wind strength + water ripples/ice/splashes).
- [x] `YearCycles` ✅ (`FolioYearCycles` + `FolioCycle`) — seasonal baseline: interpolates
      winter/spring/summer/fall presets (leaves/temperature/humidity/clouds/wind) over a
      tunable year loop (tick 7, before Weather at 8). Pushes temperature + humidity into
      Weather (retires its `base_temperature`/`base_humidity` stand-ins → weather drifts with
      the season); `FolioLeaves` pulls its density from the `leaves` track (fall → full leaf
      storm, spring → bare — verified). `set_progress_override`/`set_duration` tune/lock it;
      demo `run_folio_world ARGS="--season=..."` or `--debug` keys 1-4 (seek+continue).
      Beyond folio: winter baseline is cold enough to snow on its own, and a seasonal
      vegetation tint (`folio_season_tint`, applied in grass/foliage) recolours the world
      (winter cool, spring fresh-green, summer warm, fall golden). Weather now also takes a
      seasonal cloud baseline (`base_clouds`) from YearCycles so winter is cloudy enough that
      cold + rain coincide and it actually snows (the temp/cloud noise channels never lined up
      before). `FolioCycle::seek()` jumps
      a cycle to a phase and keeps it running. Deferred: seasonal
      clouds/wind don't feed Weather (no baseline hook), and `Overlay`.
- [ ] `Overlay` (folio Cycles sibling; deferred).

### ⚠️ Weather/season system — known issues & planned rework (NOT fixing now)
Observed via the `--debug` parameter logger + `docs/env_report.html` plot (2+ year run).
Deferred deliberately; capture only:
- **Too many coupled parameters.** Weather juggles 7 signals — humidity, temperature,
  clouds, wind, rain, snow, leaves — on independent noise channels. Too complex to reason
  about or tune; the seasons only "line up" by luck.
- **Rain ≈ snow.** Rainfall and snowfall look/behave the same on the plot — snow is just
  "rain while cold", so they track each other instead of reading as distinct events.
- **Leaves never actually fall, but the stat says ~60%.** The `leaves` density track sits
  high for much of the year (fall preset = 1.0) yet no visible leaf-fall event happens;
  the % is "leaves track > 0.5 seconds", not a fall event.
- **Snow stat vs line mismatch.** Report shows ~40% "snow active" while the snow line reads
  ~0 for long stretches — the snow signal decays negative and the `snow>0.01` counter
  over-reports. NOTE: `*_pct_of_year` also exceeds 100% on multi-year runs (rain 121.9%)
  because the logger divides accumulated seconds by ONE year length — a reporting artifact,
  not the sim.
- **PLAN → simplified DETERMINISTIC weather.** Replace the noise soup with an explicit,
  deterministic schedule keyed to the year phase, with proper named periods:
  clear-summer / rainy / leaf-fall(autumn) / snow(winter). Each period has a defined
  start/duration so snow, leaves, and rain are discrete, predictable, testable events —
  not emergent noise overlaps. Collapse the 7 signals to the few that actually drive visuals.
- **PLAN → tree & grass appear/disappear system.** Vegetation should spawn/despawn with the
  season (trees + grass grow in / shed / go bare) rather than only tinting — tie to the new
  deterministic schedule.

## Tier 3 — Car   ⏸️ DECISION: keep existing ArcadeVehicle (do NOT port folio physics)
- [x] Decision: **use the project's `ArcadeVehicle` as-is.** Both are custom raycast
      vehicles on a dynamic rigid body with spring suspension, so the *feel* is already
      in the same family — folio uses Rapier's built-in `DynamicRayCastVehicleController`
      (`world.createVehicleController` + per-wheel engine/brake/steer/suspension),
      ArcadeVehicle hand-rolls raycast springs on a `RigidBody3D` with an HSM
      (grounded/airborne/driving/drifting/gliding/ramp) and its own `VehicleConfig`.
      Porting folio's would mean swapping physics engines (Rapier→Godot) for no feel
      win. Keep ours; borrow only specific *ideas* later if wanted (flip/upside-down/
      stuck detection, suspension height presets, ice-slip friction).
- [~] Deferred folio car pieces (revisit only if needed): `VisualVehicle` (car mesh +
      squash/lean — could dress ArcadeVehicle in the folio look later), `Player`
      (boost/suspension state machine), `Inputs/*` (nipple/wheel — project has its own),
      `Tracks` (wheel marks carved into terrain data — ties to FolioTerrain's deferred
      track map), `Trails` (ribbon behind car).

## Tier 4 — Environment   (composed by FolioWorld ✅)
- [x] Terrain content data map loaded ✅ (folio `terrain.png` as swappable test asset).
- [~] `Floor` — visual + physics done ✅ (`FolioFloor` + `mesh_floor.gdshader`). Now also
      builds a `HeightMapShape3D` collider that mirrors the shader displacement
      (terrain_data.b × -1.5 × edge_fade) so vehicles drive the same hills they see, plus
      invisible perimeter walls (bedrock stand-in) at the island edge. Camera-follow recenter
      (endless island) and the folio `terrain.glb` heightfield source still deferred.
- [~] `WaterSurface` — done ✅ (ripples + shore mask + screen-blur refraction, lit+fogged).
- [x] `Grass` ✅ (`FolioGrass` GPU blade field, wind-swayed).
- [x] `InstancedGroup` ✅ (`FolioInstancedGroup` MultiMesh helper) + terrain-aware scatter demo.
- [x] `Foliage` leaf material ✅ (`FolioFoliage` cross-quad leaf cloud) — demoed as bushes.
- [x] `Trees` ✅ (`FolioTrees` = instanced trunk + FolioFoliage crown).
- [x] `Flowers` ✅ (`FolioFlowers` tiny wind-swayed tufts).
- [x] `WindLines` ✅ (`FolioWindLines` — pooled gust ribbons, weather-driven).
- [x] `RainLines` ✅ (`FolioRainLines` — falling rain; becomes falling snow when cold).
- [x] `Leaves` ✅ (`FolioLeaves` + `leaves.gdshader`) — a field of autumn leaves
      tumbling/drifting around the view, one mesh driven in the vertex stage; reads
      the global shader clock (`folio_elapsed_scaled`, so it obeys scale/bullet-time)
      and the global wind field, follows the view's optimal area, brown→orange per
      leaf with an ambient floor. Folio's GPU-compute sim is captured as look, not
      ported: vehicle push, explosion, terrain-aware damping/floor (clamps to y=0),
      and YearCycles density are deferred (the `amount` gate is now driven by FolioYearCycles).
- [x] `Snow` ✅ (ground accumulation) (`FolioSnow` + `snow_ground.gdshader`) — a
      camera-following snow sheet that LIES ON the terrain (same `folio_terrain_data.b`
      height source as the floor) via the shared `folio_material` pipeline (lit/shadow/
      fog match the world). `coverage` accumulates from weather (cold+wet builds up,
      warm melts; eased over time), fades in + scales perlin lumps, fades out over water
      and at the roaming edges; hash/perlin glitter sparkle. Deferred vs folio: the RT
      elevation field, wheel tracks (FolioTracks). Demo: `run_folio_world ARGS="--weather=snow"`.
- [~] `Scenery` — STARTED ✅: `scenery.glb` (track curbs, basalt rocks, wooden fences,
      bridge, road) imported to `assets/folio/scenery/` and instanced via
      `scene/folio/scenery.gd` with auto trimesh colliders on the solid objects; drivable
      in `car_world`. TODO: apply folio MeshDefaultMaterial (currently glb materials), and
      bring the rest of the environment glbs (playground/ramps, birchTrees, bushes, areas,
      poleLights, bricks, fences) for the full replica.
- [ ] `Whispers` (remaining Tier-4 ambient; `InstancedGroup` mesh helper already done above).

## Tier 5 — Audio
- [~] `Audio` — foundation ✅ (`FolioAudio`: groups/items registry + spatial distance fade
      + persisted mute). Music playlist / ambiances / one-offs (content) deferred.

## Tier 6 — UI framework
- [~] Foundation ✅ (`FolioUI`: CanvasLayer above post + full-rect Control root + mute button;
      `FolioMenu` centred overlay with fade + Esc toggle, rows driving Audio/Quality).
- [x] Shared theme ✅ — FolioUI now loads the game's editor-editable `ui_theme.tres` (same one
      CUI uses); extended it with PanelContainer + Label so FolioUI panels/labels style from it.
      `FolioTheme::build()` loads the .tres with a code fallback. Single source of truth for the look.
- [x] `Notifications` ✅ (`FolioNotifications`: top-centre transient toast stack; mute changes toast).
- [x] `Title` ✅ (`FolioTitle`: start overlay; Play → fade out + `started`; HUD hidden until start).
- [x] `Modals` ✅ (`FolioModal`: reusable confirm/alert dialog; menu "Quit to title" routes through it).
- [ ] `InteractivePoints`, `Map`, gamepad focus nav
      (their `CUI` DOM system → Godot `Control` nodes) — build on the FolioUI root + theme.

## Tier 7 — Portfolio content  (DEFERRED / likely skip)
- [ ] Areas/*, Achievements, Career, BlackFriday, KonamiCode, Easter.

---

## Per-system analysis catalog template
For each system, capture before writing code:
- **Purpose:** one line.
- **Tick:** subscribes at priority N / emits events.
- **Shaders/uniforms owned.**
- **Reads from:** other systems.
- **Godot equivalent.**
- **Port risk / notes.**

## Open decisions
- [ ] Car: keep `ArcadeVehicle` or adopt folio's Rapier-controller model?
- [ ] Renderer tier for final mobile target (Forward+ now, Mobile later).
- [ ] Which env systems are in-scope for the actual game vs reference-only.


---

# Catalog entries

## Tier 0 · Ticker  (folio `Game/Ticker.js`, 72 lines)  ✅ analyzed

**Purpose.** The single frame clock and heartbeat. Owns time (`elapsed`/`delta`),
a time `scale`, a frame-countdown scheduler (`wait`), the shared shader-time
uniforms, and fires the one per-frame `'tick'` event that the entire engine
subscribes to. Everything else is a `tick` subscriber.

**Driven by (important).** The Ticker does NOT run itself. `Rendering.setRenderer()`
calls `renderer.setAnimationLoop(t => ticker.update(t))`, so the **renderer's
present cadence drives the whole simulation**. One entry point, one clock.

**`update(elapsedMs)` does, in order:**
- `delta = min(elapsedSeconds - elapsed, maxDelta=1/30)` — clamps big steps after a
  stall (spiral-of-death guard). No minimum clamp.
- `deltaScaled = delta * scale` with `scale = 2` → the game world runs at **2×
  wall-clock**. `elapsedScaled` accumulates the scaled timeline separately.
- `deltaAverage` = rolling mean of the last 30 deltas (consumers use it, e.g.
  `PhysicsVehicle` steps physics at `min(1/60, deltaAverage)`).
- Writes 4 uniforms: `elapsed`, `delta`, `elapsedScaled`, `deltaScaled`.
- Decrements every `waits` countdown; fires + removes any that reach 0.
- `events.trigger('tick')`.

**Tick / priority model (the key pattern).** `events.on('tick', cb, order)` runs
callbacks in **ascending integer `order`** (default = 1). Observed priorities in
the real codebase form a fixed frame pipeline:

| order | stage (observed) |
|------:|------------------|
| 0–1   | early setup / `Time` |
| **2** | physics **pre** (`PhysicsVehicle.updatePrePhysics`, …) |
| 3–4   | physics world step sits here |
| **5** | physics **post** (read-back of body transforms) |
| 6–10  | gameplay / visuals (10 is the bulk — ~21 subscribers) |
| 13–14 | late visual |
| **998** | render (`Rendering.render`) |
| 1000, 2000 | post-render / cleanup |

**Shaders/uniforms owned.** `elapsed`, `delta`, `elapsedScaled`, `deltaScaled` —
global TSL uniforms every animated shader reads. These become Godot **global
shader uniforms** (one clock shared by all ported shaders).

**`wait(frames, cb)`.** Frame-count scheduler (counts frames, not seconds — distinct
from gsap's second-based `delayedCall`).

**Reads from.** Only the `Game` singleton (registration). No gameplay inputs.

**Godot equivalent.**
- A `Ticker` object with its OWN ordered subscriber list (port `Events` first).
  Do NOT use Godot per-node `_process` order — it is not the stable integer
  priority this design needs. One root node's `_process(delta)` (or
  `_physics_process`) calls `ticker.update()` once.
- Time scale 2 → keep BOTH timelines (raw + scaled) like folio; prefer computing
  scaled delta yourself over `Engine.time_scale` (which would also scale physics).
- `maxDelta` clamp → keep `min(delta, 1/30)`.
- Shader time → `RenderingServer.global_shader_parameter_set(...)` once per frame,
  at a low tick priority.
- `wait(frames)` → a countdown list; `deltaAverage` → a 30-entry rolling buffer.

**Port risks / notes.**
- Ordering is load-bearing but expressed as bare magic numbers. Record every
  system's tick priority as it is ported; consider named constants in the Godot
  port (`PRIO_PHYSICS_PRE = 2`, `PRIO_PHYSICS_POST = 5`, `PRIO_RENDER = 998`).
- Renderer-drives-clock: in Godot the engine drives the loop — funnel it through
  ONE call site to keep folio's determinism; don't scatter `_process`.
- No fixed-timestep accumulator: folio uses variable (clamped) delta. If you want
  deterministic physics, this is where you may intentionally diverge
  (`_physics_process` fixed step). **Open decision.**

**Depends on (port order):** `Events` (must exist first). Next file → `Events.js`.


## Tier 0 · Events  (folio `Game/Events.js`, 65 lines)  ✅ ported

**Purpose.** Tiny named pub/sub bus each system owns. Generic messages
(`stop`/`start`, `change`, `enter`/`leave`, `muteChange`, …), separate from the
per-frame Ticker. Usage in source: 204 `on`, 61 `trigger`, 8 `off`.

**Semantics (1:1):** `on(name, cb, order=1)` ascending integer order, stable
within an order; `trigger(name, args)` applies args to each listener
(`fn.apply` → `callable.callv`); `off(name, cb)` removes one, `off(name)` removes all.

**Port.** `src/folio/events.{h,cpp}`, class `Events : RefCounted` (plain object,
no scene tree — mirrors folio systems each holding an `Events`). Dispatch over a
snapshot so a listener may on/off mid-trigger. Registered in `register_types.cpp`.

**Status:** built + linked + registered. No scene (RefCounted). Verified by
compile/link/registration; dispatch pattern identical to the runtime-verified Ticker.

**Note.** `Ticker` keeps its OWN ordered list rather than depending on `Events`,
to stay dependency-free; both share the same ordered-snapshot dispatch idea.

**Port files so far:**
- `src/folio/ticker.{h,cpp}` + `project/scene/folio/ticker.tscn` (registered, scene-verified).
- `src/folio/events.{h,cpp}` (registered).


## Tier 0 · Time  (folio `Game/Time.js`, 85 lines)  ✅ ported

**Purpose.** Time-scale controller + bullet-time (slow-mo). Does NOT hold a clock —
it OWNS the Ticker's `scale`. Normal = `default_scale` (2x); `activate_bullet_time()`
eases scale to `bullet_scale` (0.5) and back via a 0..1 `progress`. Ticks at
priority **0** (before physics) so the scaled delta is set for the frame.

**Port.** `src/folio/time.{h,cpp}`, class **`FolioTime` : Node** (renamed — Godot
already has a core `Time` singleton; `godot::Time` would collide). Subscribes to
`Ticker::connect_tick(update, 0)` in `_ready` (retries via `call_deferred` if the
Ticker singleton is not up yet). Registered in `register_types.cpp`.

**Not ported:** folio also scales `gsap.globalTimeline.timeScale()` so tweens slow
with the world — no Godot GSAP equivalent; mirror the scale when a tween layer lands.

**Status:** built + linked + registered; runs clean in `project/scene/folio/core.tscn`
(Ticker + FolioTime), 10 frames headless, subscription verified.

**Port files so far (Tier 0):**
- `src/folio/ticker.{h,cpp}`  + `project/scene/folio/ticker.tscn`
- `src/folio/events.{h,cpp}`
- `src/folio/time.{h,cpp}`
- `project/scene/folio/core.tscn` (Ticker + FolioTime; the growing core composition)


## Tier 0 · Viewport  (folio `Game/Viewport.js`, 55 lines)  ✅ ported

**Purpose.** Measures drawable size + device pixel ratio and broadcasts resize
events. First real consumer of the ported `Events` bus (folio does `new Events()`).

**Events (1:1 names):** `change` (every resize — Rendering resizes on it),
`throttleChange` (once, 400 ms after resizing stops — heavy rebuilds like Floor).

**Web → Godot mapping:** DOM rect size → `DisplayServer::window_get_size()`;
`devicePixelRatio` → `DisplayServer::screen_get_scale()` clamped to max 2; the DOM
`resize` listener → root `Window::size_changed` signal; debounce → one-shot `Timer`.

**Port.** `src/folio/viewport.{h,cpp}`, class **`FolioViewport` : Node** (renamed —
Godot has a core `Viewport`). Owns a `Ref<Events>` exposed via `get_events()`, so
ported consumers call `viewport->get_events()->on("change", ...)`. Registered.

**Status:** built + linked + registered; runs clean in `core.tscn` (now Ticker +
FolioTime + FolioViewport), 10 frames headless.

**Port files so far (Tier 0):**
- `src/folio/ticker.{h,cpp}` + `project/scene/folio/ticker.tscn`
- `src/folio/events.{h,cpp}`
- `src/folio/time.{h,cpp}`
- `src/folio/viewport.{h,cpp}`
- `project/scene/folio/core.tscn` (Ticker + FolioTime + FolioViewport)


## Tier 0 · Quality  (folio `Game/Quality.js`, 49 lines)  ✅ ported

**Purpose.** Global quality tier. `level` 0 = highest, 1 = low (default 1 on
mobile). ~15 systems branch on it (bloom mips, cheap-DOF, pre-render, antialias);
they subscribe to `change` to rebuild when it flips.

**Port.** `src/folio/quality.{h,cpp}`, class **`Quality` : Node** (no collision).
Owns a `Ref<Events>`; `change_level(level)` no-ops if unchanged else triggers
`change` with `[ level ]`. Mobile sniff `navigator.userAgent` → `OS::has_feature("mobile")`.
Registered; in `core.tscn`.

**Status:** built + linked + registered; `core.tscn` (Ticker + Time + Viewport +
Quality) runs clean, 10 frames headless.

**Tier 0 remaining:** `View` (camera), `ResourcesLoader`, then the `Game` orchestrator.


## Tier 0 · ResourcesLoader  (folio `Game/ResourcesLoader.js`, 124 lines)  ✅ ported

**Purpose.** Keyed batch loader: load `[key, path, type, modifier?]` entries,
cache by path, run an optional modifier per resource, report progress, return
`{ key: resource }`.

**Godot reality.** Most of folio's machinery is dropped: the editor import
pipeline turns `.glb`→PackedScene, `.png`/`.ktx`→Texture2D, and `ResourceLoader`
picks the importer — so no loader-type system, no Draco/KTX setup. Kept only the
observable behaviour: batch spec, path cache, modifier Callable, progress, dict.

**Port.** `src/folio/resources.{h,cpp}`, class **`FolioResources` : RefCounted`**
(renamed to avoid confusion with Godot core `ResourceLoader`). `type` (entry[2])
accepted but ignored. Load is synchronous (folio is async) — threaded upgrade =
`ResourceLoader::load_threaded_request` + polling, to wire to the loading screen
at boot. Failed load logs + skips (folio rejects the batch). Registered.

**Verified (runtime, Mac):** loaded `res://scene/folio/core.tscn` →
`is_packedscene=true`, progress callback fired `0/1`, path cache populated, and a
second load returned the SAME cached object. Smoke test kept at
`docs/folio_port/res_smoke_test.gd` (outside `res://`).

**Note.** Most folio texture modifiers (filters/wrap/flipY/colorSpace/mipmaps) are
IMPORT settings in Godot (.import), not runtime — set them there; the `modifier`
Callable remains for genuine runtime post-processing.

**Tier 0 remaining:** `View` (camera, ~789 lines), then the `Game` orchestrator.


## Tier 0 · View  (folio `Game/View.js`, 788 lines)  🔨 SPLIT in progress

Too big for one file — decomposed into `src/folio/view/`. `View : Node3D` is the
one registered node (the camera rig); the sub-parts are lightweight non-GDCLASS
helpers it owns (mirrors folio's `this.spherical = {}` sub-objects; keeps ClassDB
clean).

| file | responsibility | deps ready? | status |
|---|---|---|---|
| `spherical.{h,cpp}` | orbit angles + radius → camera offset | pure math | ✅ done |
| `roll.{h,cpp}` | camera z-roll spring + kick | pure | ✅ done |
| `optimal_area.{h,cpp}` | ground framing quad (frustum→floor raycast) | pure | ✅ done |
| `zoom.{h,cpp}` | zoom ratio state + smoothing | input hooks stubbed | ✅ done |
| `focus_point.{h,cpp}` | tracked/smoothed follow target + magnet | hooks stubbed | ✅ done |
| `cinematic.{h,cpp}` | scripted camera moves | tween/DOF hooks | ✅ done |
| `view.{h,cpp}` | orchestrator: camera, mode, tick(7), update() | composes above | ✅ done |
| `map_controls`, `free_mode`, `speed_lines` | pointer/pinch, free-fly, VFX | need Inputs/CameraControls/TSL | ⛔ deferred |

**`spherical` (done).** `ViewSpherical` (plain). Holds phi (down-tilt, 0.31π hi /
0.27π low quality), theta (0.25π yaw), radius edges 15..30 + `non_ideal_ratio_offset`
9. `update(smoothed_ratio, ratio_overflow)` → `radius_current` (lerp) + `offset`
(via `from_spherical`, = THREE `setFromSphericalCoords`). Compiles + links.
Note: `Math_PI` isn't in godot-cpp — use `Math::PI` (the constexpr the project uses).

**`roll` (done).** `ViewRoll` (plain). 1-D damped spring → camera.rotation.z; `kick(strength)` random-dir impulse. Integrated with SCALED delta (slows in bullet time). Note: no `Math::randf` in godot-cpp — use `UtilityFunctions::randf()`.

**`optimal_area` (done).** `ViewOptimalArea` (plain). Projects the camera frustum onto y=0 to get the visible ground: `recompute(phi,theta,radius_max,fov_y,aspect)` (heavy; init + throttled resize) fills `base_position`/`radius`/near+far distances/`quad_base`; `apply_focus(smoothed,raw)` (cheap per-frame) offsets to the focus (position uses smoothed focus, quads use raw). Computed ANALYTICALLY from the camera transform+fov+aspect (no Raycaster/screen-size), so it is self-contained.

**`zoom` (done).** `ViewZoom` (plain). base_ratio (player) + speed-reactive term (smoothstep on focus speed, high-quality only) → smoothed_ratio (drives spherical radius). Input hooks `scroll()`/`set_toggle_active()`/`add_base_ratio()` left public for Inputs wiring. Smoothing uses UNSCALED delta.

**`focus_point` (done).** `ViewFocusPoint` (plain). Follows `tracked_position` (player copies its pos in each frame → hook `set_tracked_position`), magnet-eases, low-passes into `smoothed_position` (camera look-at). `update(delta)` returns focus travel speed (feeds zoom). Hooks: `resume_tracking`/`pan`/`set_position`/`set_tracking` (inputs + areas). X/Z only, Y=0.

**`cinematic` (done).** `ViewCinematic` (plain). `start(pos,target,ratio_overflow)` locks a target pose (with non-ideal-ratio pushback); `apply(default_cam_xform)` blends camera pose toward it by `progress` (lerp origin + slerp rotation). Hooks: `progress` driven by an external tween (→1 over ~1.5s on start, →0 over ~1s on end), `dof_target` (0 during / 1.5 after) for the render DOF pass.

**`view` (done).** `View : Node3D` (registered). Owns one Camera3D (fov 25, near .1, far 200), mode, ratio_overflow; composes the 6 helpers; ticks at priority 7. update() sequence: focus → zoom → spherical → position → look+roll (roll post-multiplied about local Z, scaled delta) → cinematic blend → camera transform → optimal area. Standalone: aspect from Godot viewport, resize on window size_changed; hooks `set_target_position` (player), `set_quality_level`, `cinematic_*`, `roll_kick`. In `core.tscn`.
**Verified (runtime, Mac):** core.tscn (Ticker+Time+Viewport+Quality+View) runs clean; camera solves to pos≈(13.9,13.4,13.9) (radius≈24 looking at origin), optimal_radius≈20.4. Smoke test at docs/folio_port/view_smoke_test.gd.
**Deferred (need Inputs/CameraControls/TSL):** `map_controls`, `free_mode`, `speed_lines` — plus wiring `View` through FolioViewport/Quality/Inputs once the Game orchestrator lands. TIER 0 CORE COMPLETE except these.


## Tier 0 · Game  (folio `Game/Game.js`, orchestrator — Tier-0 slice)  ✅ ported

**Purpose.** Boot orchestrator + singleton. folio's Game is the composition root
(`Game.getInstance()` + `this.ticker`/`this.view`/... that all systems reach
through). This slice constructs and wires the ported spine and owns them.

**Port.** `src/folio/game.{h,cpp}`, class **`FolioGame` : Node** (renamed — project
already has `GameManager`). `_boot()` creates children in code (folio style) in
dependency order: **Quality → Ticker → Time → Viewport → Resources → View**, feeds
View its quality level before add_child, and subscribes View to Quality `change`.
Accessors `get_ticker/get_time/get_viewport_system/get_quality/get_view/get_resources`
+ `get_singleton()`. Registered. Scene: `project/scene/folio/game.tscn` (one node).

**Verified (runtime, Mac):** game.tscn boots clean (10 frames headless);
`get_ticker()`/`get_view()` return live objects; View solves pos≈(13.9,13.4,13.9),
optimal_radius≈20.4 — same as manual core.tscn, so the full boot+wiring path works.
Smoke test: docs/folio_port/game_smoke_test.gd.

**TODO in `_boot()` (as tiers land, folio order):** staged resource load + loading
screen, Rendering/post, Lighting/Fog/Reveal/Water, Materials, Physics, World,
Player, Inputs, Audio, UI.

---

## ✅ TIER 0 COMPLETE (spine)
Ticker, Events, Time, Viewport, Quality, ResourcesLoader, View (split), Game — all
built, registered, format-clean, and runtime-verified. `game.tscn` = one-node boot.
Deferred within Tier 0: View's `map_controls`/`free_mode`/`speed_lines` (need
Inputs/CameraControls/TSL). Next milestone: **Tier 1 — shared visual state**
(Lighting/Fog/Reveal/Water/Terrain + MeshDefaultMaterial → base .gdshader), the
1:1 fidelity keystone.


## Tier 1 · Lighting  (folio `Game/Ligthing.js` [sic])  ✅ ported (FolioLighting)

**Purpose.** The sun: a `DirectionalLight3D` that aims from a spherical (phi,theta)
direction at the View's optimal-area centre, PLUS the shared lighting uniforms the
base material reads. Ticks at priority 9.

**Global shader uniforms published (consumed by the base .gdshader next):**
`folio_light_direction/_color/_intensity`, `folio_light_bounce_edge_low/_high/
_distance/_multiplier`, `folio_bounce_color`, `folio_core_shadow_edge_low/_high`,
`folio_shadow_color`.

**Port.** `src/folio/lighting.{h,cpp}`, `FolioLighting : Node3D`. Direction via
`FolioViewSpherical::from_spherical` (reused). Light follows `FolioGame->get_view()`
optimal radius/position; energy/bias/blur/max-distance approximate folio's manual
ortho shadow box (Godot fits directional shadows to the camera). Registered; added
to `FolioGame::_boot()` (after View) and `core.tscn`.

**Deps deferred (hooks):** DayCycles drives direction sway + color/intensity/shadow
color — `use_day_cycles` defaults false; `set_day_progress/ _light_color/
_light_intensity/ _shadow_color` wire it later.

**Verified (Mac, real Metal):** boots clean, sun dir (0.39,0.81,0.44), DirectionalLight3D
present. Smoke: docs/folio_port/light_smoke_test.gd.

**⚠️ Gotcha (fixed here + retro-fixed FolioTicker):** `RenderingServer.
global_shader_parameter_get` / `_get_list` are EDITOR-ONLY and spam "should never be
used outside the editor" at runtime. Register globals with `global_shader_parameter_add`
guarded by a process-static bool (never check existence via `_get`); only `_set` after.
Can't read globals back at runtime — verify them via a shader that consumes them.


## Tier 1 · Fog  (folio `Game/Fog.js`)  ✅ ported (FolioFog)

**Purpose.** Distance fog + sky-gradient uniforms the base material reads
(`fog.strength.mix(outputColor, fog.color)`). Ticks at 10.

**Published globals:** `folio_fog_color_a/_b` (COLOR), `folio_fog_radial_center`
(VEC2), `folio_fog_radial_start/_end` (FLOAT), `folio_fog_near/_far` (FLOAT).
Shader computes: `mix = smoothstep(start,end, len(SCREEN_UV-center))`,
`fogColor = mix(colorA,colorB,mix)`, `fogFactor = smoothstep(near,far, viewDist)`.

**Port.** `src/folio/fog.{h,cpp}`, `FolioFog : Node`. near/far pulled from
`FolioView` optimal near/far distances (added getters), compressed by day-cycle
ratios (hooks `set_day_fog_colors`/`set_day_fog_ratios`). Registered; in
`FolioGame::_boot()` (after Lighting) and `core.tscn`.

**Deferred:** rendering the actual background gradient (WorldEnvironment/sky/full-
screen pass) — uniforms here are enough to build it at the scene/render tier.

**Verified (Mac, real Metal):** game.tscn boots clean, no errors.

## Tier 1 · Reveal  (folio `Game/Reveal.js`)  ✅ ported (FolioReveal)

**Purpose.** Intro reveal-ring: world draws only within a growing radius; bright
ring at the edge; beyond = discarded. Base material reads:
`d=len(pos.xz-center); if d>distance discard; mix=step(distance-thickness,d);
out=mix(out, color*intensity, mix)`. Ticks at 10.

**Published globals:** `folio_reveal_center` (VEC2), `folio_reveal_distance/
_thickness/_intensity` (FLOAT), `folio_reveal_color` (COLOR).

**Port.** `src/folio/reveal.{h,cpp}`, `FolioReveal : Node`. Default distance =
99999 (fully revealed, so nothing hidden without an intro). Hooks: `set_center`,
`set_distance` (intro/tween animates 0→3.5→30→99999), `set_thickness`,
`set_intensity_multiplier`, `set_day_reveal_color/intensity`. Registered; in
`FolioGame::_boot()` (after Fog) and `core.tscn`.

**Deferred:** the intro step machine (world.step/grid/inputs/audio/zoom) — port
with the intro/UI tier; the uniforms + set_distance are enough to drive it.

**Verified (Mac, real Metal):** game.tscn boots clean.

## Tier 1 · Water  (folio `Game/Water.js`)  ✅ ported (FolioWater)

**Purpose.** Water-line params. Base material whitens geometry near the surface:
`abs(pos.y - surface_elevation) > surface_thickness` keeps color, else white (foam).

**Published globals (static, no tick):** `folio_water_surface_elevation`,
`folio_water_surface_thickness` (FLOAT). `depth_elevation` (-1.5) is NOT a shader
uniform — it's the deep-water level for the water-surface mesh (later); getter only.

**Port.** `src/folio/water.{h,cpp}`, `FolioWater : Node`. Set once in _ready;
setters re-push. Defaults: elevation -0.3, thickness 0.013. Registered; in
`FolioGame::_boot()` (after Reveal) and `core.tscn`. Builds + boots clean.

## Tier 1 · Noises  (folio `Game/Noises.js`)  ✅ ported (FolioNoises)

**Purpose.** Shared tileable noise textures every material samples: voronoi (RGB =
minDist/edgeDist/cellHash), perlin (R, remapped), hash (R). Folio bakes them via
TSL into 128² render targets at boot.

**Port.** `src/folio/noises.{h,cpp}`, `FolioNoises : Node`. CPU-generated at _ready
from faithful ports of hash/random/voronoi/perlin (pure periodic math → tiles
seamlessly), stored as `FORMAT_RGBAF` `ImageTexture`s, published as GLOBAL
`sampler2D` uniforms: `folio_noise_voronoi/_perlin/_hash`. Also `get_voronoi/
_perlin/_hash` getters. One-time CPU cost, deterministic, no GPU render targets.
Registered; in `FolioGame::_boot()` (after Water) and `core.tscn`.

**Verified (Mac, real Metal):** voronoi 128x128 valid, perlin valid, res 128, no
errors. Smoke: docs/folio_port/noise_smoke_test.gd.

**Note:** shaders sample these with `global uniform sampler2D folio_noise_perlin :
repeat_enable, filter_linear;` (wrapping/filtering is sampler-side).

## Tier 1 · Terrain  (folio `Game/Terrain.js`)  ✅ ported (FolioTerrain)

**Purpose.** Provides the ground-bounce tint every `MeshDefaultMaterial` mixes in.
Folio samples a vertical gradient by terrain elevation, then blends toward a flat
grass color by the terrain data map's green channel. This is what gives shadowed
undersides their warm/cool ground colour instead of flat black.

**Port.** `src/folio/terrain.{h,cpp}`, `FolioTerrain : Node`. Generates on CPU at
_ready: a 1×16 sRGB gradient texture (stops orange `#ffa94e`@0.1, teal
`#5bc2b9`@0.3, deep blue `#13375f`@0.9) and a 1×1 all-grass default data texture;
grass color `#b8b62e`. Published as GLOBAL uniforms: `folio_terrain_gradient`
(sampler2D), `folio_terrain_data` (sampler2D), `folio_terrain_grass_color` (COLOR),
`folio_terrain_subdivision` (FLOAT 128), `folio_terrain_size` (FLOAT 192). Hooks:
`set_terrain_data(Ref<Texture2D>)`, `set_grass_color(Color)`. Registered via
`global_shader_parameter_add` behind a file-static guard (never `_get` — editor-only).
Registered in register_types.cpp; in `FolioGame::_boot()` (after Noises) with
`get_terrain()` accessor; in `core.tscn`.

**Shader logic (for the base .gdshader):**
`uv = worldXZ / subdivision / 1.5 + 0.5; data = texture(terrain_data, uv);`
`base = texture(gradient, vec2(0, 1.0 - data.b));`
`bounceColor = mix(base, grass_color, data.g)`.

**Verified (Mac, real Metal Forward+, Godot 4.7.1):** `make debug` clean,
`game.tscn --quit-after 120` rc=0, no editor-only errors.

**Deferred:** real content data map (`terrain/terrain.png`) + wheel tracks.

## Tier 1 · Base shader — MeshDefaultMaterial  ✅ keystone (pipeline + cast shadows)

**Source.** `folio-2025/sources/Game/Materials/MeshDefaultMaterial.js` — extends
`MeshLambertNodeMaterial` but overrides `outputNode` to compute the final stylized
colour itself.

**Port.** `project/material/shaders/folio/mesh_default.gdshader`, `shader_type
spatial`, `render_mode unshaded, cull_back`. We mirror folio's custom output by
computing colour by hand in `fragment()` and writing straight to `ALBEDO` (no engine
lighting). Pipeline is 1:1 with folio's `outputNode`:

1. base = `base_color` (per-material `_colorNode`, white default)
2. **light bounce** — `smoothstep(bounceLow, bounceHigh, dot(N, down))` ×
   `pow(max(0,(bounceDist - max(0,posY))/bounceDist), 2)` × `bounceMultiplier`,
   mixing toward `FolioTerrain.colorNode(terrainNode(worldXZ))` (gradient by
   elevation, blended to grass by coverage).
3. **water** — within `surfaceThickness` of `surfaceElevation`, flatten to white.
4. **light tint** — `× light_color × light_intensity`.
5. **core shadow** — `smoothstep(coreHigh, coreLow, dot(N, lightDir))`.
6. **combined shadow** — `mix(out, baseColor × shadowColor, max(core, drop))`.
7. **fog** — radial screen-space colour `mix(colorA, colorB, smoothstep(start,end,
   |SCREEN_UV - center|))`, strength `smoothstep(near, far, -VERTEX.z)` (folio
   `rangeFogFactor`).
8. **alpha-test discard** (`alpha < alpha_test`, folio 0.1).
9. **reveal** — discard beyond `distance`; `step(distance - thickness, dist)` ring
   tinted `reveal_color × intensity` at the growing frontier.

**Per-material inputs (folio parameters):** `base_color`, `alpha`, `alpha_test`, and
feature toggles `has_light_bounce / has_water / has_core_shadows / has_fog /
has_reveal` (default true).

**Consumes globals:** all Tier-1 owners — `folio_light_*`, `folio_shadow_color`,
`folio_core_shadow_edge_*`, `folio_terrain_gradient/_data/_grass_color/_subdivision`,
`folio_water_surface_*`, `folio_fog_*`, `folio_reveal_*`. World pos/normal via
varyings from `MODEL_MATRIX`.

**Verified (Mac, real Metal Forward+, Godot 4.7.1):** compiles clean against the live
globals; `shader_preview` renders the test sphere with warm sun tint on top and green
ground-bounce on the underside (light-bounce + light-tint confirmed). Screenshot:
`docs/folio_port/shader_preview.png`; archived test scene/script:
`docs/folio_port/shader_compile_test.tscn.txt`, `shader_preview.gd.txt`. Live preview
scene: `project/scene/folio/material_preview.tscn` (`make run_folio_mat`).

**Cast (drop) shadows — DONE.** The shader is now shaded (`render_mode cull_back,
ambient_light_disabled`); fragment() writes folio's colour (incl. core shadow, fog,
reveal) to `ALBEDO`, and a `light()` pass reads the sun's `ATTENUATION` to fold the
cast shadow in as folio's `max(core, drop)` — reproduced exactly by mixing toward the
shadow colour a second time with `t = (drop-core)/(1-core)`. Single sun + ambient
disabled means final pixel == `DIFFUSE_LIGHT`. Toggle `has_drop_shadows`.

**Verified (Mac, real Metal Forward+):** `make run_folio_mat` — sphere + box cast
folio-tinted drop shadows on the ground; box sun-away face shows core shadow + green
ground bounce. Screenshot `docs/folio_port/material_preview.png`.

**Deferred / next:**
- **Colour space** — global colours are used raw; verify sRGB→linear vs folio.
- **Wheel tracks** — `terrainNode` track-carving multiply (game.tracks) still stubbed.
- Note the minor reorder: drop shadow is applied after fragment's fog/reveal (light()
  runs post-fragment). Visible only where fog is strong / during the reveal wipe;
  near-field steady-state matches folio.
- Refactored into `material/shaders/folio/folio_material.gdshaderinc` ✅ — `mesh_default.gdshader`
  is now ~26 lines over it (verified byte-identical render). New materials include it,
  set the two varyings in their own `vertex()` (Godot forbids varying writes from a
  helper), then call `folio_shade()` / `folio_light_shadow()`.

## Tier 1/Materials · MeshGridMaterial  ✅ ported (standalone)

**Source.** `folio-2025/sources/Game/Materials/MeshGridMaterial.js` — a `NodeMaterial`
with `lights=false, normals=false`: a pure UNLIT triplanar grid (background colour +
grid lines). NOT part of the lit MeshDefault pipeline, so it does **not** use the
shared include.

**Port.** `project/material/shaders/folio/mesh_grid.gdshader`, `shader_type spatial`,
`render_mode unshaded, cull_back`. Faithful ports of `toMask` (dominant-axis triplanar
select), `toTriplanarUv`, `toGrid` (cheap) and `toAntialiasedGrid` (Ben Golus method,
folio default). Reference modes via `grid_reference` int (0 uv · 1 worldTriplanar ·
2-4 worldX/Y/Z · 5 localTriplanar · 6-8 localX/Y/Z), `grid_scale` global multiplier,
`grid_background`, and two configurable line sets (`line0_*`, `line1_*`, `line_count`)
covering major/minor. `deriv_mask = clamp(1 - length(fwidth(mask)), 0, 1)`.

**Verified (Mac, real Metal Forward+):** `make run_folio_grid` — antialiased ground grid,
crisp minor + bold major lines, no moiré into the distance. Screenshot
`docs/folio_port/grid_preview.png`; scene `project/scene/folio/grid_preview.tscn`.

**Deferred:** folio supports an arbitrary array of lines; the port fixes it at 2 (extend
with more `lineN_*` sets if a material needs them).

## Tier 2 · FolioRendering  ✅ ported (post-processing)

**Source.** `folio-2025/sources/Game/Rendering.js` (+ `Passes/cheapDOF.js`). folio's
Rendering owns the WebGPU renderer AND the post chain; Godot owns the renderer, so
this ports only the portable part — the composite, quality-switched:
  level 0 (high): cheapDOF(scene) + bloom   -> DOF on, wider glow
  level 1 (low):  scene           + bloom   -> DOF off, narrow glow

**Port.** `src/folio/rendering.{h,cpp}`, `FolioRendering : Node`.
- **Bloom** -> a `WorldEnvironment` with Godot's native additive glow (closest 1:1,
  cheap): `glow_strength = 0.25`, `glow_hdr_bleed_threshold = 1.0` (folio values),
  additive blend. Glow mip spread widened at high quality (levels 0-4) vs low (0-1)
  to mirror folio's nMips 5 vs 2. NOTE glow levels are 0-indexed (0..6).
- **cheapDOF** -> `project/material/shaders/folio/cheap_dof.gdshader`, a fullscreen
  `canvas_item` `ColorRect` on a `CanvasLayer` (layer 100) reading the screen texture.
  Fake tilt-shift: `strength = smoothstep(start,end,|SCREEN_UV.y-0.5|)`, hash-blur
  (golden-angle disk, `repeats` taps) of radius `strength*amount`, `mix(screen,blur,
  strength)`. folio defaults start 0.2 / end 0.5 / repeats 25 / amount 0.003. Toggled
  visible only at level 0.
- Registered; in `FolioGame::_boot()` (last), synced to `quality.level` and subscribed
  to FolioQuality `change`. `get_rendering()` accessor. In `core.tscn`.

**Verified (Mac, real Metal Forward+):** `make run_folio_post` — sharp central band,
grid blurring toward top/bottom (tilt-shift), bright line shows a soft bloom halo.
Screenshot `docs/folio_port/post_preview.png`; scene `project/scene/folio/post_preview.tscn`
(`--nodof` sets quality low to A/B the DOF).

**Deferred / next:**
- **Fog background** — DONE ✅. `material/shaders/folio/background.gdshader` draws the
  radial fog gradient on a fullscreen quad pinned to the reverse-Z far plane
  (`POSITION.z = 0`, `render_priority -100`, huge custom AABB so it is never culled),
  owned by FolioRendering. Empty/sky pixels now fade into the (cycle-animated) fog
  colour. Verified day (cyan→lavender) + night (deep blue→magenta).
- Bloom is threshold-gated at 1.0 (folio value) — only HDR-bright (>1) pixels bloom;
  tune threshold/strength once real content + day cycles land.
- `Monitoring`/stats overlay (folio `#stats` hash) not ported (dev-only).

## Tier 2 · Day Cycles  ✅ ported (FolioCycle + FolioDayCycles)

**Source.** `folio-2025/sources/Game/Cycles/{Cycles,DayCycles}.js`.

**FolioCycle** (`src/folio/cycles/cycle.{h,cpp}`, plain helper). Time-driven keyframe
interpolator: named float + colour tracks share one list of `stops` (0..1). `update
(elapsed)` computes `progress = fmod(elapsed/duration)` (or a forced value), finds the
surrounding stops, smoothstep-interpolates every track. Seamless loop via folio's
"fake steps" (append copy of first past 1.0, prepend copy of last below 0.0) in
`finalize()`. Deferred vs folio: gsap override tweens + punctual/interval events.

**FolioDayCycles** (`src/folio/cycles/day_cycles.{h,cpp}`, `Node`). Owns a FolioCycle
with the four folio presets (day/dusk/night/dawn) over a 4-min loop; ticks at order 8
(before Lighting 9, Fog/Reveal 10) and pushes into the existing day-cycle hooks:
Lighting (`set_day_progress` → sun angle, `set_day_light_color/intensity`,
`set_day_shadow_color`), Fog (`set_day_fog_colors`, `set_day_fog_ratios`), Reveal
(`set_day_reveal_color/intensity`). Registered; in `FolioGame::_boot()` after Rendering;
`get_day_cycles()` accessor (also bound the other subsystem getters for GDScript). In
`core.tscn`.

**Verified (Mac, real Metal Forward+):** `make run_folio_cycles` (live 4-min loop;
`--phase=<0..1>` locks a phase). day = warm cream, dusk = pink/salmon, night = cool
blue (brighter), dawn = warm orange, with the sun rotating through each. Screenshots
`docs/folio_port/cycle_{day,dusk,night,dawn}.png`; scene
`project/scene/folio/cycles_preview.tscn`.

**Deferred / next:**
- **YearCycles** — its consumers (foliage leaves/temperature/humidity/clouds/wind) and
  Weather aren't ported yet; add when Tier-4 environment lands.
- `electricField` / `temperature` day tracks + night/deepNight interval events (no
  consumers yet).
- Fog background ✅ (see FolioRendering) — sky now fades into the animated fog colour.

## Tier 4 · Terrain data map + Floor  ✅ data map / 🔨 Floor (visual)

**Terrain data map.** FolioTerrain now loads `project/material/textures/folio/terrain_data.png`
(folio's `terrain/terrain.png`, copied in as a **placeholder/test asset**) as the content
data map — RGBA: R=road/slab, G=grass coverage, B=land elevation. Loaded via a raw
`Image::load_from_file` + `ImageTexture::create_from_image` so the channels stay linear
DATA (not sRGB-decoded by the import pipeline). Swappable at runtime via
`set_terrain_data()` — a procedural map can replace it later. Falls back to the 1×1
all-grass default if the file is missing. `floor/slabs.png` copied in as `floor_slabs.png`.

**Floor (folio `World/Floor.js`, visual slice).** `src/folio/world/floor.{h,cpp}`,
`FolioFloor : Node3D`, + `material/shaders/folio/mesh_floor.gdshader`. A subdivided
192² plane whose material reads the data map and paints the island:
`base = terrain.colorNode(data)`, slab detail `mix(base, mix(slabLow,slabHigh,slabsTex),
data.r * perlinNoise)`, vertex displacement `y += data.b * -1.5 * edgeFade`. Reuses the
shared `folio_material.gdshaderinc` pipeline (custom base → `folio_shade` / `folio_light_shadow`)
with light-bounce + water OFF and a flat up normal (folio Floor params). Registered;
`make run_folio_floor` (`--phase=` day phase, `--nofog` to bypass fog framing, `--shot=`).

**Verified (Mac, real Metal Forward+):** island renders with grass/road/gradient +
visible slab-texture tiling (see `docs/folio_port/floor_nofog.png`). Validates the
`#include` refactor a second time (a fully custom base material sharing the pipeline).

**Deferred / next:**
- Physics heightfield collider + bedrock (need the physics system + player).
- Camera-follow recentring (`update()` snaps mesh to optimalArea each frame) + resize.
- `terrain.glb` macro terrain shape (folio displaces a real mesh; we only do the data-map
  micro displacement). The big island relief comes from that glb.
- `shadowNode = data.g` extra shadow term (our pipeline has no _shadowNode input yet).
- Fog near/far are tuned to FolioView's framing; a standalone far camera fogs the whole
  floor (framing artifact, not a bug).

## Tier 4 · WaterSurface  🔨 visual slice (ripples + shore)

**Source.** folio `World/WaterSurface.js`. A transparent plane at the water elevation
whose ALPHA is a "details mask" — the union of animated ripple bands + a shoreline
band, both read from the terrain data map's B channel. Where the mask is ~0 the
surface is see-through; where ~1 it paints the white water surface, lit through the
shared folio pipeline.

**Port.** `src/folio/world/water_surface.{h,cpp}` (`FolioWaterSurface : Node3D`) +
`material/shaders/folio/water_surface.gdshader`. Large plane pinned to
`folio_water_surface_elevation` in the vertex shader; transparent (writes ALPHA);
reuses `folio_material.gdshaderinc` (base = white, bounce/water/core-shadow/reveal OFF,
fog + drop shadows ON). Ripples (`ripplesNode`) animate from `folio_elapsed_scaled`
(stands in for the unported Wind.localTime); shore = `step(0.17, b)`. Registered; in
`floor_preview.tscn` (`make run_folio_floor`).

**Verified (Mac, real Metal Forward+):** water fills the island's low basins with a
rippling white surface (animated); fog + light tint apply. (Refraction added later — see below.)
Screenshot `docs/folio_port/water_view.png`.

**Refraction — DONE ✅.** The material is now opaque and, per folio's quality-0 path,
chooses colour per fragment: mask≥0.5 → white surface (lit+fogged via the pipeline),
mask<0.5 → a golden-angle blur of the screen texture (the scene already rendered behind
it) = translucent tinted deep water. Reading the screen texture puts it in the transparent
pass (after the opaque floor/bg). Verified with a forced-refraction test
(`docs/folio_port/water_forced.png`) — the floor blurs correctly through the whole plane.
In the normal masked view the refraction is subtle here because folio water is mostly
dense rippling foam and this flat-floor preview lacks real deep basins; it will read much
more once `terrain.glb` lands.

**Deferred / next:**
- **Drop shadows on the water surface** — the refraction path is `unshaded`, so no light()
  pass runs; a shadow cast directly onto the foam won't show (deep/refraction areas still
  carry the floor's baked shadows). Restore via a shaded variant if wanted.
- [x] **Ice + splashes + weather gating** ✅ — `ripples_ratio`/`ice_ratio` from
  `folio_weather_temperature`, `splashes_ratio` = `folio_weather_rain²`. Ice = voronoi-broken
  sheet growing from shore as it freezes; splashes = scattered rain rings. Verified: cold
  override thickens pool ice; splashes are faithful but sparse/animated and need deeper water
  bodies than this test map's shallow pools to read well.
- Ice physics collider, and camera-follow recentring/resize.

## Tier 2 · Weather  ✅ ported (FolioWeather)

**FolioWeather** (`src/folio/weather.{h,cpp}`, folio `Game/Weather.js`). Each tick (order 8,
before Wind's 9) it computes weather properties from a deterministic noise of a slow
"day-count" clock and publishes them as global shader uniforms, then drives the shared wind
field's strength.

Properties (folio formulas, `noise(x)=sin(x)·sin(1.678x)·sin(2.345x)`):
- `temperature` = `base_temperature` + `noise(p·0.4)·7.5`  (°C)
- `humidity`    = `base_humidity` + `noise(p·0.36)·0.2`
- `clouds`      = `noise(p·0.44)`                     (~[-1,1])
- `wind`        = `noise(p)·0.5 + 0.5`                (0..1)
- `rain`        = `remapClamp(humidity,0.65,1)·remapClamp(clouds,0,1)`
- `snow`        = `remapClamp(rain,0.05,0.3)·remapClamp(temp,0,-5)` + `remapClamp(temp,0,10,0,-1)`

Order matters: rain reads the humidity/clouds computed the same tick; snow reads rain/temp.

Globals (all `GLOBAL_VAR_TYPE_FLOAT`): `folio_weather_temperature/_humidity/_clouds/_wind/
_rain/_snow`. Consumers so far: `water_surface.gdshader` (temperature → ripples/ice, rain →
splashes) and `FolioWind::set_strength`. Future Snow/RainLines/WindLines read the same globals.

**Override** (folio `override.start`): `set_override(Dictionary, strength)` lerps any named
property toward a forced value (`{"temperature": -6.0}` etc.); `clear_override()` releases it.
The world preview exposes `--weather=cold|rain|clear` through this for testing.

**Year/day baseline stand-in:** folio blends in `YearCycles`/`DayCycles` values; only DayCycles
is ported, so `base_temperature=12`, `base_humidity=0.55` stand in for the annual baseline, and
the noise is driven by an accumulated day-count (`elapsed_scaled / 240s`) in place of
`dayCycles.absoluteProgress`. Swap both in when YearCycles lands.

**Deferred / next:** `YearCycles` (real annual baseline + its own foliage consumers), the
`electricField` property (needs Overlay/lightning), gsap-tweened override ramps (currently a
hard strength), and the Snow/RainLines/WindLines particle systems that will consume these globals.

## Tier 4 · WindLines  ✅ (FolioWindLines)

**FolioWindLines** (`src/folio/world/wind_lines.{h,cpp}` + `wind_line.gdshader`, folio
`World/WindLines.js` + `Geometries/WindLineGeometry.js`). A small pool of wavy ribbon "gust"
streaks over the world. Added by FolioWorld; ticks at order 10.

- **Geometry** (shared by the pool): a Catmull-Rom curve through 4 alternating handles (length 10,
  wavy in Y), sampled to 31 points, expanded to a ribbon strip — 2 verts/point, UV = (ratio, side).
- **Shader**: the ribbon has zero width at rest; `base_thickness` tapers the two ends and a
  travelling `progress` bulge widens one section, pushed along folio's fixed world tangent
  `(0,1,-1)`. Unshaded white, alpha fades with the bulge (soft ends). `thickness` + `progress`
  uniforms per pool slot (own ShaderMaterial each).
- **Manager**: spawns on a random 0.3–2 s interval at a random point in the view's optimal area,
  y = 2, rotated to the wind angle; drifts along the wind direction and animates `progress` 0→1
  over `duration = remapClamp(weather.wind, 0,1, 8,2)` (stronger wind → faster), then frees the
  slot. Replaces folio's gsap tweens + setTimeout with ticker-driven lerps + elapsed-time spawns.
- Tunables exposed: `pool_size`, `thickness` (+ internal interval/translation/spawn_height).

Verified (`make run_folio_wind`, `wind_preview.tscn`, thickness exaggerated for the still): the
curved streak sweeps across the sky with soft tapered ends. Subtle by design, like folio's.

**Deferred / next:** `RainLines` (same ribbon family, driven by weather.rain), `Leaves`, and the
gsap-eased position tween (currently linear lerp).

## Tier 4 · RainLines  ✅ (FolioRainLines — rain + falling snow)

**FolioRainLines** (`src/folio/world/rain_lines.{h,cpp}` + `rain_lines.gdshader`, folio
`World/RainLines.js`). A tiled field of `count` (2^11) falling line-quads in one mesh, displaced
entirely in the vertex shader; the node owns mesh+material and pushes weather-driven uniforms each
tick (order 10). Added by FolioWorld.

- **Geometry**: per line a quad (base XZ in [0,1] + a per-line random); per-vertex `offset` (UV:
  side x, top/bottom y); random in UV2.x.
- **Shader**: tiles the field around the view centre (`size` = optimal radius ×2, `center` =
  optimal-area xz), offsets thickness along tangent (0.707,-0.707), drops each line from
  `elevation` on a looping `progress = mod(local_time + random, 1)`, clamps to [0, elevation],
  hides a fraction via `step(visible_ratio, fract(random·99))`, and slants by `incline`.
- **Weather bindings** (folio): `visible_ratio = rain²`; `line_length =
  lerp(remapClamp(rain,0,1,1,3), 0.03, snowRatio)`; `speed = lerp(remapClamp(rain,0,1,0.2,0.4),
  0.05, snowRatio)`; `incline = remapClamp(wind,0,1,0.1,0.4)`; `snowRatio = 1-(1-max(snow,0))⁴`;
  `local_time += deltaScaled·speed`. Hidden when `visible_ratio ≈ 0`.
- **Snow for free**: as `weather.snow` rises (cold), the streaks shrink to slow flecks — this IS
  the atmospheric falling snow. Only folio's separate *ground-accumulation* Snow stays parked.

Verified (`make run_folio_rain --mode=rain|snow`, forced weather override): rain = long
wind-slanted white streaks tiled across the view; snow = small slow white flecks. `count` exposed
for quality scaling.

**Deferred vs folio:** the `weatherRain` achievement, the (commented) compute-shader path, and
`hasCoreShadows` on the drops (kept unshaded to keep the moving-vertex pass cheap).

## Tier 6 · UI  🔨 foundation (FolioUI)

**FolioUI** (`src/folio/ui/ui.{h,cpp}`). folio's interface is HTML/CSS DOM (`Menu.js` →
`.js-menu`, portfolio tabs) with nothing to translate 1:1, so this is the reusable Godot
foundation the later HUD/menu build on. Singleton via FolioGame (`get_ui()`); created after
FolioAudio in boot.

- **Structure**: a `CanvasLayer` (layer 10, above the 3D viewport) with a full-rect `Control`
  root (`get_root()`) that HUD widgets parent to (mouse-filter IGNORE so clicks pass through
  except where a child grabs them). A generic open/closed `State` enum stands in for folio's
  `Menu.OPEN/OPENING/CLOSED/CLOSING`.
- **Mute button** (the one concrete widget this slice): a rounded pill bottom-left, pressing it
  calls `FolioAudio::toggle_mute()`, and it re-labels itself ("Sound: On/Off") from FolioAudio's
  `mute_changed` signal, so it always reflects the real state (incl. the persisted initial value).

Verified (`make run_folio_ui`): the pill renders over the world; toggling audio flips the label
via the signal (`Sound: On → Sound: Off`), confirming the button↔audio wiring both ways.

**Menu overlay** (`src/folio/ui/menu.{h,cpp}`, `FolioMenu`): a centred panel in a full-rect
CenterContainer over a dimming backdrop, faded open/closed with a Tween on the OPENING→OPEN /
CLOSING→CLOSED state machine, toggled by Esc or the HUD "Menu" button. Rows demonstrate the
framework driving real systems — Sound (FolioAudio mute, reflects `mute_changed`), Quality
(FolioQuality.change_level, which grass/rendering react to), and Resume (close). Verified
(`make run_folio_ui ARGS="--menu"`): centred panel, dim backdrop, crisp above the DOF.

**HUD sits above post:** FolioUI is on CanvasLayer 200, above FolioRendering's cheap-DOF post
layer (100), so the tilt-shift blur (strongest at screen top/bottom) no longer smears the HUD —
matching folio, where the DOM UI is over the WebGL canvas entirely.

**Reusable UI layer extracted into `cui/`.** The overlay machinery + generic screens now live in the
general UI library, not the folio port:
- `CUIOverlay` (`cui/cui_overlay.{h,cpp}`) — base for fading full-screen overlays: OPEN/OPENING/
  CLOSING/CLOSED state machine, `open`/`close`/`toggle` with a CUITween fade that kills the prior
  one, optional dim backdrop + full-rect CenterContainer (`center`), fit-on-open, Esc → `_on_escape()`.
  A `CUI` builder is injected (`set_builder`); subclasses override `_build_content()`.
- `CUIModal` (`cui/cui_modal.{h,cpp}`) — confirm/alert dialog on the base (`open_confirm`/`open_alert`,
  `confirmed`/`cancelled` signals + on_confirm Callable). Moved out of folio, decoupled from FolioUI.
  Now renders an **accent-tinted header band** behind the title (rounded top corners) so every modal
  reads with a proper titlebar, and its buttons are `FOCUS_NONE` (no stray focus outline).
- `CUIToast` (`cui/cui_toast.{h,cpp}`) — the transient toast stack, decoupled likewise.
- `FolioMenu` / `FolioTitle` now **extend `CUIOverlay`** and only build content + game wiring (audio/
  quality/quit for the menu, branding/`started` for the title). Three hand-rolled state machines
  collapsed into one.
FolioUI owns the `CUI` builder + a `CUIModal` + `CUIToast` and injects the builder into every screen.
Old `folio/ui/modal.*` + `notifications.*` moved to `attic/folio_ui/` (delete when convenient).

**Dev tools adopt the shared UI.** The Marching Cubes help dialog was switched from a raw
`AcceptDialog` (a `Window` whose frame styleboxes ignore per-node overrides — gray body, no header
backing, focus outline on OK) to a `CUIModal` opened via `open_alert("Marching Cubes Info", …)`.
It now matches the sidebar's dark theme, gets the header band, and drops the focus outline. The MC
sidebar background was also moved off an inline stylebox override onto the shared `ui_theme.tres`
`Panel/styles/panel` (StyleBoxFlat_sidePanel).

**View / controller split (CUI reuse).** FolioUI screens no longer hand-build any widgets: FolioUI
owns one shared `CUI` builder (`get_cui()`), and FolioMenu/FolioTitle/FolioModal/FolioNotifications
construct their *entire* tree through it — `add_color_rect` (backdrop), `add_center_container`,
`add_vbox`/`add_panel_container`, `add_label`, `add_button` — keeping only state + callbacks. New CUI
factories (`add_color_rect`, `add_center_container`) were added and the container helpers' `name`
made optional (empty = unregistered) so structural nodes don't pollute the registry; all are
API-preserving for the ~10 existing callers. Fades go through a new **`CUITween`** helper
(`src/cui/cui_tween.{h,cpp}`): `fade_alpha()` and `toast()` build the tween and hand back the `Ref`,
and each screen stores it and `kill()`s the prior one before starting a new fade — which also fixes a
latent double-tween bug when open/close overlap. So CUI = view construction + tween plumbing, FolioUI
screens = behaviour, everything from the one shared `ui_theme.tres`.

**Shared theme** (`src/folio/ui/theme.{h,cpp}`, `FolioTheme`): `build()` loads the game's canonical
`res://scripts/ui/menu/ui_theme.tres` (the same resource CUI applies) and returns it; a code
fallback (`BG`/`PANEL_BG`/… constants) covers the case where the .tres is missing so the HUD never
renders unstyled. The .tres was extended with a `PanelContainer/styles/panel` StyleBoxFlat + `Label`
font/color/size so FolioUI's panels and labels render from the same resource. Net: one
editor-editable theme drives both CUI (dev HUDs) and FolioUI (game screens); tweak colours/font/
styleboxes in the Godot editor, no recompile. (CUI dev tools now also inherit the Label + panel
styling.)

**Notifications** (`src/folio/ui/notifications.{h,cpp}`, `FolioNotifications`): a top-centre
toast stack (VBoxContainer) under the root; `notify(text, duration=2.5)` drops a theme-styled
pill that fades in → holds → fades out → frees itself (per-toast Tween chain). FolioUI emits one
on `mute_changed` ("Sound muted" / "Sound on"), so it's exercised in the real flow. Verified
(`make run_folio_ui ARGS="--notify"`).

**Title / start screen** (`src/folio/ui/title.{h,cpp}`, `FolioTitle`): a full-screen start
overlay (dim backdrop + centred title/subtitle/Play, configurable text) shown ~0.1s after boot
once layout has a real size, using the same fade + state machine as FolioMenu. Play fades it out
and emits `started`; FolioUI hides the in-game HUD (mute/menu buttons) while it's up and reveals
them + toasts "Welcome to the island" on `started`. Verified (`make run_folio_ui ARGS="--play"`):
boot shows title only → Play reveals the world + HUD.

**Modals** (`src/folio/ui/modal.{h,cpp}`, `FolioModal`): one reusable centred dialog on top of the
HUD. `open_confirm(title, message, confirm_label, cancel_label, on_confirm)` and `open_alert(...)`
rebuild the button row per call, fade in on the shared state machine, emit `confirmed`/`cancelled`,
and run an optional `on_confirm` Callable; Esc cancels. Wired: the menu's "Quit to title" row opens
a confirm modal → on Quit, closes the menu, hides the HUD, and reopens the Title. FolioUI gained
`set_hud_visible()` (used at boot, on `started`, and on quit). Verified (`make run_folio_ui
ARGS="--modal"`).

**Deferred / next:** InteractivePoints, Map — plus gamepad/keyboard focus navigation, and hooking
`started` to the intro/reveal sequence. All parent onto
`FolioUI::get_root()`; overlays reuse the FolioMenu open/close pattern.

## Tier 5 · Audio  🔨 foundation (FolioAudio)

**FolioAudio** (`src/folio/audio.{h,cpp}`, folio `Game/Audio.js`). The reusable core of
folio's Howler manager, backed by Godot audio nodes. Ticks at order 14; in `FolioGame::_boot()`;
`get_audio()`.

Model (folio-faithful):
- **Groups → items.** `register_sound(Dictionary)` builds one item and returns an int handle.
  Options: `path`, `group` (default "all"), `volume`, `rate`, `loop`, `autoplay`, `anti_spam`,
  `positions` (Vector3 / PackedVector3Array), `distance_fade`. Items are filed into their group;
  `play_group(name)` round-robins the next item (folio `group.play()`).
- **play(id)** honors the init gate + anti-spam (skip if replayed within `anti_spam` s), then
  records `last_play` and the group's `last_played_id`.
- **update()** (tick 14): `global_rate = time.scale / time.default_scale`; per positional item it
  finds the nearest of its positions to the camera, moves the 3D player there, and sets volume =
  `volume · remapClamp(distance, 0, distance_fade, 1, 0)` (folio's linear fade), pitch =
  `clamp(rate·global_rate, 0.5, 4)`.
- **Backing.** Non-positional → `AudioStreamPlayer`; positional → `AudioStreamPlayer3D` with
  `ATTENUATION_DISABLED` (folio's own fade governs volume) + an `AudioListener3D` pinned to the
  view camera for correct panning.
- **Mute** (folio soundToggle): `set_mute/toggle_mute/is_muted`, mutes the Master bus, persists to
  `user://folio_audio.cfg`, emits `mute_changed`. `L` key toggles it (until FolioInputs lands).

Verified (`make run_folio_audio`, `audio_preview.tscn`): a beep registered at the camera reads
volume 0.5, an identical one 1000 u away reads 0.0 (linear fade over 20 u); mute toggles the
Master bus on/off. Test asset: `project/audio/folio/test_beep.wav` (generated sine).

**Deferred / next (folio content, per the port):** the concrete `setPlaylist` (music with
crossfade), `setAmbiants` (day/night-gated spatial loops), `setOneOffs`, and their area logic;
tab-focus pause; the `onPlaying`/`onPlay` per-item callbacks; wiring engine/tyre SFX to the
vehicle. These call `register_sound()` — the engine slice is done.

## Tier 2 · Wind  ✅ + Tier 4 · Grass  ✅ (visual slice)

**FolioWind** (`src/folio/wind.{h,cpp}`, folio `Game/Wind.js`). Publishes the shared
wind field as globals — `folio_wind_direction` (VEC2, angle π·0.6), `_position_frequency`
(0.5), `_strength` (0.5), `_time` (scrolls each tick by `deltaScaled·timeFrequency·strength`).
The per-position sway offset (2-octave perlin along the wind dir, folio `offsetNode`) is
computed in the consuming shaders. Ticks at order 9; in `FolioGame::_boot()`; `get_wind()`.
Strength is now modulated by `FolioWeather` (`set_strength(remapClamp(wind,0,1,0.15,1))`).

**FolioGrass** (`src/folio/world/grass.{h,cpp}` + `material/shaders/folio/grass.gdshader`,
folio `World/Grass.js`). A GPU blade field: one `ArrayMesh` of subdiv² (200²=40k) blade
triangles built on the CPU — each blade's ground XZ shared by its 3 verts, per-vertex
height randomness in UV.x, loop index from `VERTEX_ID % 3`. The shader builds each blade:
toroidal camera-follow wrap, triangle shape scaled by terrain grass coverage (g) + height
(base·randomness·perlin·g), billboard rotation toward the camera, wind sway on the tip
(FolioWind field), and a "hide" push (y += 100) where g ≤ 0.5. Colour = shared terrain
colour, through the folio pipeline (bounce/water off, flat up normal, drop shadows + fog).
Follows the framed area each tick (order 10). Registered; `FolioGrass` node in
`floor_preview.tscn` (`make run_folio_floor`).

**Verified (Mac, real Metal Forward+):** dense blades render only on grass-covered terrain
(bare on roads), coloured to match the ground, per-blade height variation; tips sway with
wind (animated). Screenshot `docs/folio_port/grass_view.png`.

**Tunables added.** `FolioGrass` exposes node properties: `subdivisions` (blade grid, blades=n²),
`field_size`, `blade_width`, `blade_height`, and `scale_with_quality` (low tier → ~60%
subdivisions). Changing any rebuilds the field. `floor_preview.gd` caps FPS via
`Engine.max_fps` (default 60, `--fps=N` override) so the preview isn't uncapped.

**Deferred / next:** viewport-resize regen + surfaceOverflow blade sizing, the `shadowNode`
tip/base darkening term, wheel-track flattening.

## Tier 4 · InstancedGroup + scatter  ✅ (foundation)

**FolioInstancedGroup** (`src/folio/world/instanced_group.{h,cpp}`, folio
`Game/InstancedGroup.js`). Reusable GPU-instancing helper: `build(mesh, transforms)`
creates a `MultiMeshInstance3D` that renders one mesh at many transforms in a single
draw call (Godot `MultiMesh`, TRANSFORM_3D). The foundation for Trees / Bushes /
Flowers / Foliage. Multi-surface source models deferred (call build() per source mesh).

**Scatter demo** (`scatter_preview.gd`, `make run_folio_scatter`). Samples the terrain
data map's G (grass) channel on the CPU and places ~350 bushes where g>0.6, with random
yaw + scale, then instances them via FolioInstancedGroup. Placeholder bush = a squashed
sphere with the folio `mesh_default` material (green), so it's lit/shadowed/fogged by the
shared pipeline. Verified (Mac, Metal): bushes cover the grass, avoid roads/water, cast
shadows, fade into fog. Screenshot `docs/folio_port/scatter_view.png`.

**Deferred / next:**
- **Foliage leaf material** (folio `World/Foliage.js`, ~220 lines): see-through leaf
  edges, wind sway, colour-A/B gradient — the material that makes trees/bushes read as
  foliage rather than solid blobs.
- **Real models** — folio's tree/bush GLBs (content, load as test assets like terrain.png)
  or stylized placeholders; multi-surface (trunk + leaves) via multiple MultiMeshes.
- **Flowers**, and a reusable C++ biome-scatter (the demo scatters in GDScript for now).

## Tier 4 · Foliage leaf material  ✅ (FolioFoliage)

**Source.** folio `World/Foliage.js` + `foliage/foliageSDF.png` (leaf mask, copied in as
`material/textures/folio/foliage_sdf.png`).

**FolioFoliage** (`src/folio/world/foliage.{h,cpp}` + `material/shaders/folio/foliage.gdshader`).
Builds the leaf-cluster mesh once on the CPU: ~80 small cross-quads scattered on a sphere
(radius `1-rng³` so denser toward the shell, random Z-spin), normals blended 85% toward
the outward direction so the blob shades round, merged into one ArrayMesh. `scatter(transforms)`
instances it via one MultiMesh. The shader alpha-cuts a leaf from the mask (UV rotated by the
wind field so it shimmers), 2-tones the colour by sun facing (`mix(colorA, colorB,
smoothstep(dot(N, lightDir)))`), and lights it through the shared folio pipeline (fog + drop
shadows, water off). Colours exposed as node properties. Registered.

**Verified (Mac, real Metal Forward+):** `make run_folio_scatter` now scatters convincing
leafy bushes on the grass (avoid roads/water), 2-toned, wind-shimmered, shadowed, fogged.
Screenshot `docs/folio_port/foliage_view.png`.

**Deferred / next:** near-vehicle see-through fade + per-cluster camera facing; `Trees`
(trunk body mesh + this foliage as the crown, needs the tree GLB); `Flowers`; a reusable
C++ biome scatter (still GDScript in the demo).

## Tier 4 · Trees  ✅ (FolioTrees, stylized)

**FolioTrees** (`src/folio/world/trees.{h,cpp}`, folio `World/Trees.js`). A tree =
an instanced trunk + a FolioFoliage crown, composing the two scatter helpers:
`FolioInstancedGroup` for the trunks (a tapered `CylinderMesh` with the folio
`mesh_default` material, brown) and `FolioFoliage` for the leaf crowns (each tree's
transform offset up by the trunk height and scaled up). `scatter(transforms)` plants
a tree at each. folio loads a tree GLB (treeBody + treeLeaves); we build a stylized
trunk so no content model is needed — swap in a GLB later if wanted.

**Verified (Mac, real Metal Forward+):** `make run_folio_scatter` now plants ~60 min-spaced
trees (larger, darker crowns) among the bushes on the grass; shadowed, fogged.
Screenshot `docs/folio_port/trees_view.png`.

**Deferred / next:** trunk physics colliders (folio adds cylinder bodies), real tree GLB,
`Flowers`, and folding the whole scatter into the main scene + a reusable C++ biome scatter.

## Tier 4 · FolioWorld  ✅ (environment composition + C++ biome scatter)

**FolioWorld** (`src/folio/world/world.{h,cpp}`, folio `World/World.js` slice). One node
that assembles the environment: creates FolioFloor + FolioWaterSurface + FolioGrass and
scatters Bushes (FolioFoliage) + Trees (FolioTrees) across the grassy terrain. The scatter
is now in C++ (reads `terrain_data.png` G channel, seeded RNG, min-spaced trees) — replacing
the GDScript demo. The build is deferred one frame (`call_deferred`) so FolioGame has booted
(globals / terrain data / view) first, so a FolioWorld can sit next to FolioGame in any scene.

Wired into `game.tscn` (FolioGame + FolioWorld), so `make run_folio` renders the whole
island. Also `make run_folio_world` (world_preview). Registered.

**Verified (Mac, real Metal Forward+):** full world — floor, grass, bushes, trees, water —
under day/night + fog + shadows; `game.tscn` boots clean (rc=0). Screenshot
`docs/folio_port/world_view.png`.

**Deferred / next:** physics colliders, real GLB models, `Flowers`, softening the dark tree
undersides, and per-quality scatter counts.

## Tier 4 · Flowers + polish pass  ✅

**FolioFlowers** (`src/folio/world/flowers.{h,cpp}` + `material/shaders/folio/flowers.gdshader`,
folio `World/Flowers.js`). Tiny solid-colour tufts (a few small quads per cluster) built once
on the CPU, instanced on the grass in one MultiMesh draw; upper verts sway with the wind field.
Colour exposed. Scattered by FolioWorld (~1200 on grass).

**Polish pass:**
- **Tree crowns lightened** — `FolioTrees` crown colours raised (colorA 0.42/0.50/0.22, colorB
  0.62/0.70/0.30) so the shadow-side undersides read green instead of near-black.
- **Trunk physics** — `FolioTrees` now builds a `StaticBody3D` of `CylinderShape3D` colliders
  (one per trunk) so the car will collide with trees. Toggle `collide`.
- **Per-quality scatter counts** — `FolioWorld` halves bush/tree/flower counts at the low
  quality tier (on top of FolioGrass's existing per-quality subdivisions).

**Verified (Mac, real Metal Forward+):** full world with flowers + lighter trees renders clean;
screenshot `docs/folio_port/world_polished.png`.

**Deferred / next:** `Snow`/`RainLines`/`WindLines` (need Weather), real GLB models, and
folding physics colliders for the floor.
