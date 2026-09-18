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
- [ ] Global shader clock: expose `elapsed`/`delta` (+ `scale = 2`) as Godot
      **global shader uniforms** (folio publishes these via TSL `uniform()`).

## Tier 1 — Shared visual state  ★ HIGHEST FIDELITY RISK
Analyze these together as ONE unit — they define the whole look.
- [ ] Catalog `MeshDefaultMaterial` output pipeline: light bounce → water tint →
      lambert light → core+drop shadow → fog → reveal discard.
- [~] Shared uniform owners: `FolioLighting` ✅ · `FolioFog` ✅ · `FolioReveal` ✅ · Water · Noises · Terrain.
- [ ] **Keystone:** build ONE Godot base spatial shader (an include) reproducing
      that pipeline against matching global uniforms; every material `#include`s it.
- [ ] `MeshGridMaterial`.

## Tier 2 — Render pipeline
- [ ] `Rendering`: scene pass + bloom + cheapDOF composite, quality-switched.
      Map to `WorldEnvironment` glow + custom DOF `CompositorEffect`/post shader.
- [ ] `Overlay`, day/year `Cycles`, `Weather`, `Wind`.

## Tier 3 — Car
- [ ] `PhysicsVehicle` (Rapier raycast controller) vs existing `ArcadeVehicle` —
      decide keep-ours vs match-theirs.
- [ ] `VisualVehicle`, `Player`, `Inputs/*`, `Tracks`, `Trails`.

## Tier 4 — Environment
- [ ] `Floor`, `WaterSurface`, `Grass`, `Foliage`, `Flowers`, `Trees`, `Bushes`,
      `Leaves`, `Snow`, `RainLines`, `WindLines`, `Whispers`, `Scenery`,
      `InstancedGroup` (instanced-mesh helper).

## Tier 5 — Audio
- [ ] `Audio` (Howler → `AudioStreamPlayer` + buses), music, engine/floor SFX.

## Tier 6 — UI framework
- [ ] `Menu`, `Modals`, `Notifications`, `Title`, `InteractivePoints`, `Map`
      (their `CUI` DOM system → Godot `Control` nodes).

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
