- `camera_fly.gd` is a Godot GDScript camera controller that supports two modes: **FLY** (free orbit/pan/zoom around a pivot) and **TARGET** (orbit around a target Node3D using a SpringArm3D + Camera3D).  
- Input: mouse wheel for zoom, middle‑mouse drag for orbit/pan, Shift+MMB for pan; pitch is clamped to avoid flipping.  
- Lightweight and functional for editor/prototypes but missing many systems expected in an Uncharted‑style AAA camera.

---

## Major gaps for AAA (why they matter) ⚠️
- No collision/occlusion handling → camera will clip through level geometry and break immersion.  
- No smoothing/damping or state blending → camera motion feels mechanical, not cinematic.  
- No gamepad/aim input polish, camera shake, or dynamic framing → poor player feel and weak cinematic moments.  
- Zoom/limits & input scaling are hardcoded / frame-rate dependent → inconsistent across platforms.

---

## Concrete improvements I would make (prioritized) 🎯

1. Collision & occlusion handling (top priority)  
   - Raycast/probe between target and camera pivot, smoothly shorten camera distance on hit.  
   - Fade or silhouette player when obstructing view.

2. Smooth follow & rotation damping  
   - Use critically‑damped spring / exponential smoothing for position & rotation to remove jitter and add weight.

3. Input polish & controller support  
   - Separate sensitivities for mouse vs. gamepad, add deadzones and input smoothing.

4. Camera state machine + blending for cinematic moments  
   - Named states (Default, Aiming, Cover, Cutscene) with blendable targets and offsets.

5. Camera shake / impulse system and dynamic FOV  
   - Procedural shake + per‑event impulses; adapt FOV for speed or dramatic moments.

6. Config / tuning for designers  
   - Export all tuning parameters (smoothing, distances, masks, offsets) and document them.

7. Performance & robustness  
   - Do collision checks in _physics_process, cache space queries, respect collision layers.

---

## Minimal, high‑impact code changes (example) 🔧
- Replace instant moves with smoothed interpolation.
- Add collision probe to prevent clipping.
- Add exported tuning params and gamepad support.

Example (concept — insert into your `camera_fly.gd`):
```gdscript
@export var smooth_speed := 8.0
@export var min_distance := 1.5
@export var max_distance := 12.0
@export var collision_mask := 1

var desired_distance := 6.0
var current_distance := 6.0
var rotation_velocity := Vector2.ZERO

func _physics_process(delta):
    _apply_input(delta)           # read mouse + gamepad into desired values
    _resolve_collision(delta)     # raycast from target/pivot to desired camera pos
    _apply_smoothing(delta)       # smooth position & rotation (lerp / damp)
```
- Use PhysicsDirectSpaceState.intersect_ray to clamp `desired_distance` and then smooth `current_distance = lerp(current_distance, desired_distance, 1 - exp(-smooth_speed * delta))`.
- Smooth rotation with slerp/quaternion or exponential smoothing; move heavy logic from _input to _process/_physics_process for consistent behavior.

---

## UX / design features to add for “Uncharted” feel 💡
- Shoulder offset and adaptive camera framing.
- Auto-centering with aim-assist when the player slows.
- Seamless blending into scripted cutscene cameras (timelines/spline paths).
- Per-surface occlusion fading (fade character mesh/material when blocking).
- Dedicated cinematic camera component/API for designers to trigger.

---

## Performance & testing notes 🔍
- Run collision probes at physics tick only; minimize allocations in hot path.  
- Expose metrics for QA (camera distance, occlusion counts).  
- Profile on target platforms (60/30/120 Hz) and tune smoothing constants.

---

## Next step — which changes should I implement first?  
Tell me which of these to do now and I’ll update `camera_fly.gd` with a focused PR:
- Collision + occlusion (recommended)  
- Smooth follow + damping  
- Controller & input polish  
- Full cinematic state machine

