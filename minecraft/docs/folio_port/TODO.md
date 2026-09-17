# Folio-2025 → Godot C++ Port — TODO

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
- [ ] `Game` boot orchestrator — construct subsystems in folio's init() order;
      staged resource load (intro batch → full batch → physics).
- [ ] `Time`, `Viewport`, `View` (camera), `Quality`, `ResourcesLoader`.
- [ ] Global shader clock: expose `elapsed`/`delta` (+ `scale = 2`) as Godot
      **global shader uniforms** (folio publishes these via TSL `uniform()`).

## Tier 1 — Shared visual state  ★ HIGHEST FIDELITY RISK
Analyze these together as ONE unit — they define the whole look.
- [ ] Catalog `MeshDefaultMaterial` output pipeline: light bounce → water tint →
      lambert light → core+drop shadow → fog → reveal discard.
- [ ] Shared uniform owners: `Lighting`, `Fog`, `Reveal`, `Water`, `Noises`, `Terrain`.
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
