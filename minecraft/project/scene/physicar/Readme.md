# PhysiCar — Arcade Monster Truck Racing

> Mario Kart meets Monster Jam. Bouncy suspension, drifting, nitro boost, mid-air stunts, and cartoonish fun on a RigidBody3D chassis with spherecast wheels in Godot 4.6.

---

## Table of Contents

1. [Tire Casting: Raycast vs SphereCast](#1-tire-casting-raycast-vs-spherecast)
2. [Core Physics Architecture](#2-core-physics-architecture)
3. [Suspension System](#3-suspension-system)
4. [Steering & Drift Model](#4-steering--drift-model)
5. [Traction & Surface Friction](#5-traction--surface-friction)
6. [Arcade Acceleration & Speed Model](#6-arcade-acceleration--speed-model)
7. [Nitro Boost System](#7-nitro-boost-system)
8. [Monster Truck Bouncy Suspension](#8-monster-truck-bouncy-suspension)
9. [Ramps, High Jumps & 360 Spins](#9-ramps-high-jumps--360-spins)
10. [Mid-Air Controls: Double Jump & Glider](#10-mid-air-controls-double-jump--glider)
11. [Cartoonish & Arcade Features](#11-cartoonish--arcade-features)
12. [Track & Environment Systems](#12-track--environment-systems)
13. [Algorithms Reference](#13-algorithms-reference)
14. [Production Roadmap](#14-production-roadmap)

---

## 1. Tire Casting: Raycast vs SphereCast

### Recommendation: **Use SphereCast from the start**

| Aspect | Raycast | SphereCast |
|---|---|---|
| **Performance** | Cheapest. 1 line test per wheel | ~3–5x more per query, but 4 casts total = negligible for any modern machine |
| **Edge handling** | Can snag on sharp edges, curbs, small rocks — truck gets "stuck" | Rolls smoothly over edges and curbs — essential for monster truck terrain |
| **Ground normal** | Point sample — jittery on bumpy/curved terrain | Averaged over sphere contact area — smoother normals, smoother ride |
| **Curved ramps** | Works but can jitter on tight curvature | Naturally conforms to curved ramp surfaces |
| **360° Loops** | Works fine — local -Y ray points at track when gravity override + alignment is active | Same, but smoother on tight loop radii (< 5m) |
| **Monster truck big tires** | Tire visual is decoupled from thin ray — feels "wrong" | Sphere radius represents the fat tire contact patch — physically honest |
| **Debugging** | Easy — draw a line | Slightly harder — draw a swept sphere path |
| **Implementation** | `intersect_ray()` — 3 lines | `PhysicsShapeQueryParameters3D` + `cast_motion` — ~10 lines |

### Verdict

**Use SphereCast from the start.** The performance difference is meaningless for 4 wheels in a single-player game (we're talking microseconds). SphereCast gives you:

- Smooth rolling over rocky terrain, curbs, debris — **critical for monster truck**
- Better ground normals on curved ramps and loops — less chassis jitter
- Contact patch that actually represents a fat tire — not a thin laser beam
- No "snagging" on geometry edges that a thin ray would miss or clip through

The only real advantage of raycast is slightly simpler debugging (a line vs a swept sphere). That's not worth the tradeoffs for a monster truck game.

> **Why do many tutorials recommend raycast?** Because they're simpler to teach and "good enough" for flat-track go-kart games. For a monster truck that drives over uneven terrain, crushes obstacles, and does loops — spherecast is the right tool.

### Ray Count: 1 SphereCast Per Tire

**1 cast per tire, 4 tires = 4 casts total.** That's it. No multi-ray needed.

Each spherecast fires from the **wheel hardpoint** (where the suspension attaches to the chassis) downward along the car's **local -Y axis**:

```
    ┌─────────────────────┐  ← Chassis (RigidBody3D)
    │  ●               ●  │  ← Wheel hardpoints (local offsets)
    │  │               │  │     
    │  ▼ sphere        ▼  │  ← SphereCast direction: local -Y
    │  ◯ r=0.25       ◯  │  ← Sphere radius = ~40% of tire visual radius
    │  │               │  │     
    │  ╳ contact       ╳  │  ← Hit point on ground
    │                     │
    │  ●               ●  │  ← Rear wheel hardpoints
    │  │               │  │
    │  ▼               ▼  │
    │  ◯               ◯  │
    │  │               │  │
    │  ╳               ╳  │
    └─────────────────────┘
```

**Sphere radius** = `tire_visual_radius * 0.3–0.5` (the "contact patch ratio"). This is NOT the full tire radius — it's smaller, representing the contact patch:
- Too small (< 0.2) → acts like a raycast, misses edges
- Too large (> 0.6) → can clip through walls, tunnels, narrow geometry
- Sweet spot: **0.3–0.4 × tire_visual_radius** for monster truck

**Cast length** = `suspension_rest_length + tire_visual_radius + margin`  
The margin (0.1–0.2m) ensures we detect ground even at full extension.

### SphereCast Implementation

```gdscript
# Pre-create the shape once in _ready() — don't allocate every frame
var tire_cast_shape: SphereShape3D

func _ready():
    tire_cast_shape = SphereShape3D.new()
    tire_cast_shape.radius = wheel_radius * 0.4  # contact patch ratio

# Cast one wheel. Returns {position, normal} or empty dict if no hit.
func cast_wheel(space: PhysicsDirectSpaceState3D, start: Vector3, 
                direction: Vector3, max_dist: float) -> Dictionary:
    var params = PhysicsShapeQueryParameters3D.new()
    params.shape = tire_cast_shape
    params.transform = Transform3D(Basis.IDENTITY, start)
    params.motion = direction * max_dist
    params.exclude = [self]
    params.margin = 0.01  # small skin for stability
    
    # cast_motion returns [safe_fraction, unsafe_fraction]
    # safe_fraction = how far the sphere traveled before first contact (0.0–1.0)
    var result = space.cast_motion(params)
    
    if result[0] < 1.0:  # hit something
        var contact_point = start + direction * max_dist * result[0]
        
        # Move shape to contact position and get detailed contact info
        params.transform.origin = contact_point
        var rest_info = space.get_rest_info(params)
        
        if rest_info.size() > 0:
            return {
                "position": rest_info["point"],
                "normal": rest_info["normal"],
                "collider": rest_info.get("collider_id", 0),
                "distance": max_dist * result[0]
            }
        else:
            # Fallback: use cast direction as inverse normal
            return {
                "position": contact_point,
                "normal": -direction,
                "distance": max_dist * result[0]
            }
    
    return {}  # no hit — wheel is in the air
```

### Why Not Multi-Ray?

The multi-ray approach (5 rays in a cross pattern) was an alternative to get smoother normals with raycast performance. **With spherecast, it's unnecessary** — the sphere already gives you averaged contact over its surface area. Multi-ray is a workaround for a problem spherecast solves natively.

### Fallback to Raycast (Debug Mode)

Keep a raycast fallback for debugging — it's easier to visualize:

```gdscript
@export var use_spherecast: bool = true  # toggle in inspector for debugging

func cast_wheel_auto(space, start, direction, max_dist) -> Dictionary:
    if use_spherecast:
        return cast_wheel(space, start, direction, max_dist)
    else:
        # Simple raycast fallback
        var query = PhysicsRayQueryParameters3D.new()
        query.from = start
        query.to = start + direction * max_dist
        query.exclude = [self]
        return space.intersect_ray(query)
```

---

## 2. Core Physics Architecture

### Entity Structure

```
PhysiCar (RigidBody3D)
├── ChassisCollider (CollisionShape3D — BoxShape3D)
├── ChassisMesh (MeshInstance3D — monster truck body)
├── WheelFL (Node3D — visual only, positioned by raycast)
├── WheelFR (Node3D)
├── WheelRL (Node3D)
├── WheelRR (Node3D)
├── NitroVFX (GPUParticles3D)
├── DriftTrailL (GPUParticles3D / MeshInstance3D)
├── DriftTrailR (GPUParticles3D / MeshInstance3D)
├── GliderMesh (MeshInstance3D — hidden until deployed)
├── SquashStretchController (script — drives chassis deformation)
├── Camera3D (spring arm chase cam)
└── AudioStreamPlayer3D (engine sound)
```

### State Machine

```
                    ┌──────────┐
          ┌────────│  GROUNDED │────────┐
          │        └──────────┘         │
          │ ramp launch    │ drift key  │ fall off edge
          ▼                ▼            ▼
    ┌──────────┐   ┌──────────┐   ┌──────────┐
    │ AIRBORNE │   │ DRIFTING │   │ AIRBORNE │
    └──────────┘   └──────────┘   └──────────┘
          │              │              │
     glider key     release key    mid-air jump
          ▼              ▼              ▼
    ┌──────────┐   ┌──────────┐   ┌──────────┐
    │ GLIDING  │   │ BOOST!   │   │ AIRBORNE │
    └──────────┘   │(drift    │   │ (higher) │
          │        │ reward)  │   └──────────┘
          │        └──────────┘         │
          └──────────────┴──────────────┘
                         │
                    ┌──────────┐
                    │  LANDED  │ ← squash-stretch + particles
                    └──────────┘
```

### Core Loop (every `_physics_process`)

```
1. Read input (throttle, brake, steer, drift, jump, nitro, glider)
2. Update state machine
3. For each wheel:
   a. Compute wheel world position from chassis transform
   b. SphereCast downward (local -Y)
   c. Compute suspension force (spring-damper)
   d. Compute longitudinal force (drive / brake)
   e. Compute lateral force (grip / drift)
   f. Apply forces to RigidBody3D at contact point
   g. Position wheel visual at contact + normal * radius
4. Apply steering torque (yaw)
5. Apply arcade assists (anti-roll, auto-align, speed cap)
6. Apply nitro force if active
7. Apply air controls if airborne
8. Update VFX, SFX, camera
```

---

## 3. Suspension System

### Algorithm: Spring-Damper with Monster Truck Tuning

We implement a classic **spring-damper model** from scratch. For monster truck feel, we extend it with:

#### 3.1 Base Spring-Damper

```
F_suspension = (k * compression) - (c * v_rel)

where:
  k    = spring stiffness (N/m)
  c    = damping coefficient (N·s/m)
  compression = rest_length - current_length  (meters, clamped ≥ 0)
  v_rel = velocity of contact point along suspension axis (m/s)
```

**Critical damping** calculation:
```
c_critical = 2 * sqrt(k * m_wheel)
c = damping_ratio * c_critical
```

#### 3.2 Monster Truck Extensions

**Asymmetric Damping** — Fast rebound (bouncy!), slow compression (absorbs):
```gdscript
var damping_coeff: float
if rel_vel > 0:  # compression (wheel moving up toward chassis)
    damping_coeff = compression_damping  # higher = stiffer on bumps
else:  # rebound (wheel extending back down)
    damping_coeff = rebound_damping      # lower = BOUNCIER
```

**Progressive Spring Rate** — Gets stiffer as compression increases:
```gdscript
# Linear spring feels dead. Progressive feels alive.
var progressive_k = suspension_stiffness * (1.0 + compression_ratio * progressive_factor)
# progressive_factor = 2.0 gives a nice monster truck ramp-up
```

**Recommended Monster Truck Values:**
```
suspension_rest_length   = 0.6 – 0.8 m     (long travel!)
suspension_stiffness     = 15000 – 25000    (soft base, progressive builds)
compression_damping_ratio = 0.5 – 0.7       (moderate compression damping)
rebound_damping_ratio    = 0.15 – 0.25      (LOW = bouncy rebound)
progressive_factor       = 1.5 – 3.0        (higher = more progressive)
max_compression          = 0.4 – 0.5 m      (lots of travel range)
wheel_radius             = 0.5 – 0.7 m      (BIG monster truck tires)
```

#### 3.3 Anti-Roll Bar (Arcade Stability)

Prevents excessive body roll during cornering. Transfers spring force from inside wheel to outside wheel:

```gdscript
# For each axle (front pair, rear pair):
var compression_diff = left_compression - right_compression
var anti_roll_force = anti_roll_stiffness * compression_diff
# Add to left wheel force, subtract from right wheel force
```

`anti_roll_stiffness`: 3000–8000 for monster truck (keep it moderate — some roll looks cool).

---

## 4. Steering & Drift Model

### 4.1 Ackermann-ish Steering (Simplified Arcade)

Real Ackermann geometry turns inner and outer wheels at different angles. For arcade, we simplify:

```gdscript
var max_steer_angle = deg_to_rad(35.0)

# Speed-sensitive steering: less angle at high speed
var speed_factor = clamp(1.0 - speed / max_speed, 0.3, 1.0)
var steer_angle = steering_input * max_steer_angle * speed_factor

# Apply as yaw torque on the RigidBody3D
# Also rotate front wheel forward vectors for lateral force calc
var front_forward = forward_dir.rotated(up_dir, steer_angle)
```

### 4.2 Drift System

**Algorithm: Rear Grip Reduction + Drift Angle Maintenance**

Mario Kart style drift: player holds drift button, rear grip drops, car slides at an angle, releasing drift button gives a speed boost proportional to drift duration.

```
DRIFT MECHANICS:
1. Player holds drift trigger while turning
2. Rear tire grip multiplied by drift_grip_factor (0.2 – 0.4)
3. Counter-steer assist keeps drift angle stable
4. Drift timer accumulates
5. On release: speed boost = f(drift_time)
   - Level 1 (< 1.5s):  small blue sparks,  +10% speed for 0.5s
   - Level 2 (1.5–3.0s): orange sparks,      +20% speed for 0.8s
   - Level 3 (> 3.0s):   purple sparks,      +35% speed for 1.2s
```

```gdscript
# During drift:
var drift_grip_factor = 0.25  # rear tires are slippery
var counter_steer_strength = 0.6  # auto counter-steer assist

# Lateral force modification during drift
if drifting:
    if i >= 2:  # rear wheels
        grip_factor *= drift_grip_factor
    # Add artificial yaw torque to maintain drift angle
    var desired_drift_angle = sign(steering_input) * deg_to_rad(30)
    var current_drift_angle = atan2(local_velocity.x, -local_velocity.z)
    var angle_error = desired_drift_angle - current_drift_angle
    apply_torque(Vector3.UP * angle_error * drift_correction_torque)
```

### 4.3 Tire Smoke & Trail During Drift

- Spawn `GPUParticles3D` at rear wheel contact points
- Particle color matches drift level (blue → orange → purple)
- Draw tire marks with `ImmediateMesh` or decal projector on ground

---

## 5. Traction & Surface Friction

### 5.1 Pacejka-Lite Tire Model (Arcade Simplified)

The full Pacejka "Magic Formula" is overkill. We use a **simplified slip-curve** approach via `Curve` resources (exported as `front_grip_curve` / `rear_grip_curve`).

```
Lateral Force = grip_curve.sample(slip_ratio) * normal_load * surface_friction

where:
  slip_ratio = lateral_speed / max_slip_speed  (clamped 0–1)
  normal_load = suspension_force magnitude
  surface_friction = material-dependent multiplier
```

**Grip Curve Shape (Curve resource):**
```
1.0 ┤    ╭──────────╮
    │   ╱            ╲
0.8 ┤  ╱              ╲
    │ ╱                 ╲──── gradual falloff
0.4 ┤╱                      
    │                        
0.0 ┼──────────────────────
    0    0.2   0.5   0.8  1.0
         slip ratio →
```

Peak grip around slip = 0.1–0.2, then drops off. Lower the peak for more arcade sliding.

### 5.2 Surface Friction System

Use **physics material layers** or **raycast collision layer + metadata** to detect surface type:

```gdscript
# Surface types and their friction multipliers
enum SurfaceType { ASPHALT, DIRT, ICE, GRASS, MUD, SAND, BOOST_PAD, OIL_SLICK }

var surface_friction_table = {
    SurfaceType.ASPHALT:   1.0,    # baseline
    SurfaceType.DIRT:      0.7,    # loose, kicks up dust
    SurfaceType.ICE:       0.15,   # super slippery, sparks
    SurfaceType.GRASS:     0.6,    # slightly slow, green particles
    SurfaceType.MUD:       0.4,    # slow + splash particles
    SurfaceType.SAND:      0.5,    # slow + sand spray
    SurfaceType.BOOST_PAD: 1.2,    # extra grip + speed boost
    SurfaceType.OIL_SLICK: 0.05,   # near zero grip, spin out!
}

# Detection via collision layer groups or surface material metadata
func get_surface_type(raycast_result: Dictionary) -> SurfaceType:
    var collider = raycast_result.get("collider")
    if collider and collider.has_meta("surface_type"):
        return collider.get_meta("surface_type") as SurfaceType
    return SurfaceType.ASPHALT
```

### 5.3 Per-Surface Effects

| Surface | Friction | Particle Effect | Sound | Speed Mod |
|---|---|---|---|---|
| Asphalt | 1.0 | Tire smoke on drift | Screech | Normal |
| Dirt | 0.7 | Brown dust clouds | Gravel crunch | -10% |
| Ice | 0.15 | Blue sparkles | Crystalline slide | Normal |
| Grass | 0.6 | Green bits + grass blades | Swish | -15% |
| Mud | 0.4 | Brown splat particles | Squelch | -25% |
| Sand | 0.5 | Sand spray | Hiss | -20% |
| Boost Pad | 1.2 | Speed lines + glow | Whoosh | +50% temporary |
| Oil Slick | 0.05 | Dark rainbow shimmer | Slide sound | Normal |
| Rainbow Road | 1.0 | Prismatic sparkles | Magical hum | +5% |

---

## 6. Arcade Acceleration & Speed Model

### 6.1 Non-Realistic Acceleration Curve

Real cars: F=ma with complex drivetrain. Arcade: **direct velocity manipulation** blended with physics forces.

```gdscript
# Arcade acceleration: fast initial response, smooth top speed approach
var target_speed = max_speed * throttle_input
var speed_diff = target_speed - current_forward_speed
var accel_force: float

if speed_diff > 0:  # accelerating
    # High initial acceleration, tapering near top speed
    var speed_ratio = current_forward_speed / max_speed
    var accel_curve = 1.0 - speed_ratio * speed_ratio  # quadratic falloff
    accel_force = max_accel * accel_curve * throttle_input
else:  # braking / engine braking
    accel_force = -brake_decel * brake_input

# Blend: 70% force-based, 30% direct velocity nudge (arcade assist)
apply_force(forward_dir * accel_force * 0.7, Vector3.ZERO)
linear_velocity += forward_dir * speed_diff * arcade_assist * delta * 0.3
```

### 6.2 Speed Parameters

```
max_speed            = 30 m/s  (~108 km/h, feels fast with monster truck scale)
max_accel_force      = 8000 N  (snappy acceleration)
brake_decel          = 12000 N (strong brakes)
reverse_max_speed    = 10 m/s
arcade_assist        = 3.0     (how aggressively we nudge toward target speed)
```

### 6.3 Gear System (Cosmetic + Feel)

Not a real transmission — just for engine sound pitch and visual RPM gauge:

```gdscript
var gear_ratios = [3.5, 2.5, 1.8, 1.3, 1.0, 0.8]  # 6 gears
var current_gear = 0

func update_gear():
    var speed_ratio = current_forward_speed / max_speed
    current_gear = int(clamp(speed_ratio * gear_ratios.size(), 0, gear_ratios.size() - 1))
    engine_rpm = lerp(engine_rpm_idle, engine_rpm_max, 
                      fmod(speed_ratio * gear_ratios.size(), 1.0))
```

---

## 7. Nitro Boost System

### Algorithm: Charge → Activate → Deplete → Recharge

```
NITRO SYSTEM:
┌─────────────────────────────────────────┐
│  ███████████░░░░░  NITRO  [67%]         │
│  Recharge: drift, stunts, item pickups  │
│  Deplete:  active use (3-5 seconds)     │
└─────────────────────────────────────────┘
```

```gdscript
var nitro_max = 100.0
var nitro_current = 50.0
var nitro_drain_rate = 20.0      # per second while active
var nitro_boost_force = 15000.0  # additional forward force
var nitro_speed_bonus = 1.4      # max_speed multiplier during nitro
var nitro_active = false

# Recharge sources:
var nitro_recharge_drift = 8.0    # per second while drifting
var nitro_recharge_airtime = 5.0  # per second while airborne
var nitro_recharge_stunt = 25.0   # flat bonus per completed stunt (360 spin etc)
var nitro_recharge_pickup = 30.0  # flat bonus from item pickup

func process_nitro(delta):
    if nitro_active and nitro_current > 0:
        nitro_current -= nitro_drain_rate * delta
        var boost_dir = -global_transform.basis.z.normalized()
        apply_central_force(boost_dir * nitro_boost_force)
        # VFX: flame particles from exhaust, speed lines, FOV increase
        # SFX: roaring boost sound
    else:
        nitro_active = false
    
    # Recharge
    if drifting:
        nitro_current += nitro_recharge_drift * delta
    if airborne:
        nitro_current += nitro_recharge_airtime * delta
    nitro_current = clamp(nitro_current, 0, nitro_max)
```

### Nitro VFX

- Exhaust pipes emit blue → orange flame particles (scale with nitro level)
- Screen-space speed lines (post-process shader)
- Camera FOV increases from 70° → 85° (lerped)
- Chromatic aberration post-process effect
- Chassis leaves afterimage trail (motion blur on mesh)

---

## 8. Monster Truck Bouncy Suspension

### 8.1 Visual Bounce Amplification

Physics suspension is tuned for gameplay. **Visual bounce is amplified** for cartoon effect:

```gdscript
# Chassis visual offset (child MeshInstance3D, not the RigidBody)
var visual_bounce_multiplier = 1.5  # exaggerate bounce visually
var visual_roll_multiplier = 1.3    # exaggerate body roll

# Each frame: compute average suspension compression
var avg_compression = total_compression / wheel_count
var visual_offset = avg_compression * visual_bounce_multiplier

# Apply to chassis mesh (not physics body)
chassis_mesh.position.y = -visual_offset

# Roll exaggeration
var left_avg = (compression_fl + compression_rl) / 2.0
var right_avg = (compression_fr + compression_rr) / 2.0
var roll_angle = (left_avg - right_avg) * visual_roll_multiplier
chassis_mesh.rotation.z = roll_angle
```

### 8.2 Squash & Stretch

Classic animation principle applied to the chassis:

```gdscript
# On landing: squash (wide + short)
# During jump: stretch (narrow + tall)
# While accelerating: slight stretch forward

func update_squash_stretch():
    var target_scale = Vector3.ONE
    
    if just_landed:
        # Squash: wider and shorter
        target_scale = Vector3(1.15, 0.8, 1.1)
        # Recover over 0.3 seconds with overshoot (spring animation)
    elif airborne and linear_velocity.y > 2.0:
        # Stretch upward
        target_scale = Vector3(0.9, 1.15, 0.95)
    elif nitro_active:
        # Slight forward stretch
        target_scale = Vector3(0.95, 0.97, 1.08)
    
    chassis_mesh.scale = chassis_mesh.scale.lerp(target_scale, 8.0 * delta)
```

### 8.3 Landing Impact

```gdscript
func on_landing(impact_velocity: float):
    # Particles: dust cloud, small rocks flying out
    # Screen shake proportional to impact
    # Squash animation
    # Sound: heavy thud
    # If impact > threshold: small crater decal on ground
    
    var impact_ratio = clamp(impact_velocity / 20.0, 0.0, 1.0)
    camera_shake(impact_ratio * 0.5, 0.3)  # intensity, duration
    spawn_landing_particles(impact_ratio)
    play_landing_sound(impact_ratio)
```

---

## 9. Ramps, High Jumps & 360 Spins

### 9.1 Ramp Launch Physics

When exiting a ramp, the car should get a consistent, satisfying launch regardless of exact physics frame timing:

```gdscript
# Ramp detection: Area3D trigger at ramp lip
func _on_ramp_trigger_entered(body):
    if body == self:
        var ramp_normal = ramp_surface_normal  # direction ramp points
        var speed = linear_velocity.length()
        
        # Minimum launch velocity (arcade guarantee)
        var min_launch_speed = 15.0
        var launch_speed = max(speed, min_launch_speed)
        
        # Launch direction: blend between current velocity and ramp normal
        var launch_dir = (linear_velocity.normalized() * 0.6 + ramp_normal * 0.4).normalized()
        linear_velocity = launch_dir * launch_speed
        
        # Add extra upward kick for drama
        apply_central_impulse(Vector3.UP * ramp_bonus_impulse * mass)
```

### 9.2 Circular Ramp — 360° Loop

**Algorithm: Gravity Override in Loop Zones**

> **Do raycasts work for 360° loops?** YES. The raycast fires along the car's **local -Y axis** (`-global_transform.basis.y`), not world down. When the car is upside-down inside a loop, the alignment system keeps the car's belly facing the track, so local -Y still points at the track surface. The cast type doesn't matter for loops — what matters is the **gravity override** and **orientation alignment** below. SphereCast only helps if the loop radius is very tight (< 5m), where a single point ray might jitter on the curved surface.

Inside a loop/circular ramp, override gravity to point toward the loop center:

```gdscript
# Loop zone: Area3D covering the inside of the circular ramp
# The Area3D stores the loop center position and axis

func _on_loop_zone_body_entered(body):
    in_loop = true
    loop_center = loop_area.global_position
    loop_axis = loop_area.global_transform.basis.z  # axis of rotation

func process_loop_physics(delta):
    if in_loop:
        # Centripetal force toward loop center
        var to_center = (loop_center - global_position).normalized()
        var centripetal_force = mass * (linear_velocity.length() ** 2) / loop_radius
        
        # Override gravity: point toward center instead of world down
        gravity_scale = 0  # disable world gravity
        apply_central_force(to_center * centripetal_force)
        
        # Minimum speed to complete loop (arcade assist)
        var min_loop_speed = sqrt(loop_radius * 9.8)  # real physics minimum
        if linear_velocity.length() < min_loop_speed * 1.2:
            # Arcade assist: boost speed to prevent embarrassing fall
            linear_velocity = linear_velocity.normalized() * min_loop_speed * 1.3
        
        # Align car up-vector to point away from center (feet on track)
        var away_from_center = -to_center
        var target_up = away_from_center
        var current_up = global_transform.basis.y
        var correction = current_up.cross(target_up) * 5.0  # alignment torque
        apply_torque(correction)
```

### 9.3 Ramp Launch — Automatic X-Axis Front Flips

**When the car launches off a circular ramp, it should automatically do front flips (rotation around the car's local X-axis). The number of 360° flips depends on how high the car goes.** The car must auto-align to land flat on the ground before touching down.

#### Design Philosophy

This is NOT a player-input trick. The ramp **imparts** the flip. Think Hot Wheels loop ramps — the car naturally tumbles forward based on exit velocity. Higher ramps = more flips. The system must guarantee a clean flat landing regardless of how many flips were done.

#### State Machine for Ramp Flips

```
GROUNDED → [exits ramp, launch_velocity.y > threshold] → RAMP_FLIP
RAMP_FLIP → [descending + height < landing_threshold] → LANDING_ALIGN
LANDING_ALIGN → [wheels touch ground] → GROUNDED
```

#### Algorithm

```gdscript
# --- State variables ---
var trick_state: StringName = "none"  # "none" | "ramp_flip" | "jump_spin" | "landing"
var launch_velocity_y: float = 0.0
var was_grounded: bool = true
var airborne_time: float = 0.0

# --- Tuning constants ---
const RAMP_LAUNCH_THRESHOLD: float = 5.0     # min upward velocity to trigger flip
const FLIP_BASE_SPEED: float = 8.0           # rad/s base rotation speed
const FLIP_VELOCITY_SCALE: float = 0.3       # extra rad/s per m/s of upward velocity
const FLIP_MAX_SPEED: float = 14.0           # cap flip speed
const LANDING_HEIGHT: float = 5.0            # start aligning when this close to ground
const LANDING_ALIGN_STRENGTH: float = 100.0  # torque multiplier for landing alignment
const LANDING_DAMPING: float = 0.85          # angular velocity damping during alignment

# --- Detection: Ground → Airborne transition ---
func detect_ramp_launch():
    var grounded = wheel_contact_count >= 2
    
    if not grounded and was_grounded:
        # Just left the ground
        launch_velocity_y = linear_velocity.y
        airborne_time = 0.0
        
        if launch_velocity_y > RAMP_LAUNCH_THRESHOLD:
            trick_state = "ramp_flip"
    
    if grounded and not was_grounded:
        # Just landed
        trick_state = "none"
    
    was_grounded = grounded

# --- Process ramp flip (called every _physics_process while airborne) ---
func process_ramp_flip(delta):
    if trick_state != "ramp_flip":
        return
    
    airborne_time += delta
    
    # Check height above ground via downward raycast
    var height = cast_ray_down_distance()  # world -Y raycast from car center
    
    # Transition to landing alignment when descending + close to ground
    if linear_velocity.y < 0 and height < LANDING_HEIGHT:
        trick_state = "landing"
        return
    
    # Apply continuous X-axis rotation (front flip)
    # Speed scales with launch velocity — higher ramp = faster flip = more flips
    var flip_speed = clamp(
        FLIP_BASE_SPEED + launch_velocity_y * FLIP_VELOCITY_SCALE,
        FLIP_BASE_SPEED,
        FLIP_MAX_SPEED
    )
    var local_x = global_transform.basis.x.normalized()
    angular_velocity = local_x * flip_speed
    # Override angular velocity directly — no fighting with damping
```

#### How Number of Flips Scales with Height

```
Launch Velocity Y    Approx Air Time    Flip Speed    Flips Completed
─────────────────────────────────────────────────────────────────────
  5 m/s (low ramp)      ~1.0s           9.5 rad/s       ~1.5
 10 m/s (med ramp)      ~2.0s          11.0 rad/s       ~3.5
 15 m/s (big ramp)      ~3.0s          12.5 rad/s       ~6.0
 20 m/s (mega ramp)     ~4.0s          14.0 rad/s       ~8.9

Air time ≈ 2 * launch_velocity_y / gravity
Flips ≈ flip_speed * air_time / TAU
```

The car doesn't _target_ a specific number of flips. It spins continuously at a rate proportional to launch speed. The landing alignment system takes over near the ground and snaps it flat. This feels natural — like real physics but exaggerated.

#### Landing Alignment (Auto-Flatten Before Touchdown)

```gdscript
func process_landing_alignment(delta):
    if trick_state != "landing":
        return
    
    # Current car down direction vs world down
    var car_down = -global_transform.basis.y
    var world_down = Vector3.DOWN
    
    # Compute error angles: how tilted is the car?
    var angle_x = atan2(-car_down.z, -car_down.y)  # pitch error
    var angle_z = atan2(-car_down.x, -car_down.y)  # roll error
    
    # Apply corrective torque (quadratic for snappy response)
    var torque_x = -angle_x * abs(angle_x) * LANDING_ALIGN_STRENGTH
    var torque_z =  angle_z * abs(angle_z) * LANDING_ALIGN_STRENGTH
    apply_torque_impulse(Vector3(torque_x, 0, torque_z))
    
    # Dampen angular velocity to settle flat
    angular_velocity *= LANDING_DAMPING
```

#### Key Implementation Notes

- **Disable normal airborne auto-alignment** while `trick_state == "ramp_flip"`. The flip IS the rotation — don't fight it.
- **Reduce angular damping to ~0.5** during flips (normally 6+). The car must spin freely.
- **Restore angular damping** when entering `"landing"` state so alignment torque can work.
- The landing alignment uses the same math as the existing airborne stabilization but with **much stronger torque** (100 vs 40) to snap flat quickly from any orientation.

---

### 9.4 Jump — Y-Axis 360° Spins (Mario Kart Style)

**When the player presses Jump and goes airborne, the car does continuous 360° Y-axis spins (flat spins).** The player can steer the spin direction with Left/Right. The car auto-aligns to land flat.

#### Design Philosophy

This is the classic Mario Kart hop-spin. Player jumps, car does a stylish flat spin in the air, lands cleanly. Unlike ramp flips (automatic), jump spins are **player-initiated** via the jump input and **player-directed** via Left/Right steering. The spin is around the world Y-axis (vertical), so the car stays level while rotating — like a helicopter.

#### State Machine for Jump Spins

```
GROUNDED + [Jump pressed] → apply upward impulse, set jump_triggered = true
→ [leaves ground] → JUMP_SPIN (because jump_triggered was set)
JUMP_SPIN → [descending + height < landing_threshold] → LANDING_ALIGN  
LANDING_ALIGN → [wheels touch ground] → GROUNDED
```

#### Algorithm

```gdscript
# --- Additional state variables ---
var jump_triggered: bool = false
var spin_direction: float = 1.0  # +1 = left/CCW, -1 = right/CW

# --- Tuning constants ---
const JUMP_SPIN_SPEED: float = 9.0       # rad/s — one full 360° in ~0.7s
const JUMP_FORCE_MULTIPLIER: float = 16.0 # upward impulse = mass * gravity * this

# --- Jump input (while grounded) ---
func handle_jump():
    if is_action_pressed("jump") and wheel_contact_count >= 2:
        jump_triggered = true
        
        # Determine initial spin direction from steering at jump moment
        if is_action_pressed("steer_left"):
            spin_direction = 1.0
        elif is_action_pressed("steer_right"):
            spin_direction = -1.0
        else:
            spin_direction = 1.0  # default: spin left/CCW
        
        # Apply upward impulse
        apply_central_force(Vector3.UP * mass * gravity * JUMP_FORCE_MULTIPLIER)

# --- Detection: when car leaves ground after jump ---
func detect_jump_spin():
    var grounded = wheel_contact_count >= 2
    
    if not grounded and was_grounded:
        if jump_triggered:
            trick_state = "jump_spin"
            jump_triggered = false
    
    if grounded and not was_grounded:
        trick_state = "none"
        jump_triggered = false
    
    was_grounded = grounded

# --- Process jump spin (called every _physics_process while airborne) ---
func process_jump_spin(delta):
    if trick_state != "jump_spin":
        return
    
    airborne_time += delta
    
    # Allow player to change spin direction mid-air
    if is_action_pressed("steer_left"):
        spin_direction = 1.0
    elif is_action_pressed("steer_right"):
        spin_direction = -1.0
    
    # Check height for landing transition
    var height = cast_ray_down_distance()
    if linear_velocity.y < 0 and height < LANDING_HEIGHT:
        trick_state = "landing"
        return
    
    # Apply Y-axis spin while keeping car level (dampen X and Z wobble)
    var angvel = angular_velocity
    angular_velocity = Vector3(
        angvel.x * 0.9,                    # dampen pitch wobble
        JUMP_SPIN_SPEED * spin_direction,   # controlled Y spin
        angvel.z * 0.9                      # dampen roll wobble
    )
```

#### How It Feels

```
Jump Height        Approx Air Time    Spin Speed    Spins Completed
─────────────────────────────────────────────────────────────────────
Small hop (1m)        ~0.9s           9.0 rad/s       ~1.3
Normal jump (3m)      ~1.6s           9.0 rad/s       ~2.3
Big jump (6m)         ~2.2s           9.0 rad/s       ~3.1
Mega jump (10m)       ~2.9s           9.0 rad/s       ~4.1
```

The spin speed is constant — more air time = more spins. The landing alignment system handles snapping to flat regardless of where in the rotation the car is when it starts descending.

#### Key Implementation Notes

- **Y-axis spin keeps the car level.** Unlike ramp flips (which tumble end-over-end), jump spins rotate around the vertical axis. The X and Z angular velocities are actively dampened to prevent wobble.
- **Player controls spin direction.** Left/Right input changes `spin_direction` at any point during the spin. This gives the player agency and feels responsive.
- **Same landing alignment** as ramp flips (§9.3) handles the touchdown. The car auto-flattens regardless of current yaw angle.

---

### 9.5 Unified Airborne Trick System — Complete State Machine

Both trick types (ramp flip and jump spin) share the same state machine and landing system:

```
                        ┌───────────────────────────┐
                        │        GROUNDED           │
                        │  (wheel_contact >= 2)     │
                        └───────────┬───────────────┘
                                    │
                    ┌───────────────┼───────────────┐
                    │ launch_vel.y  │ jump_triggered │
                    │ > threshold   │ == true        │
                    ▼               ▼                │
            ┌──────────────┐ ┌──────────────┐       │
            │  RAMP_FLIP   │ │  JUMP_SPIN   │       │
            │              │ │              │       │
            │ Auto X-axis  │ │ Player Y-axis│       │
            │ rotation     │ │ rotation     │       │
            │ (front flip) │ │ (flat spin)  │       │
            └──────┬───────┘ └──────┬───────┘       │
                   │                │               │
                   │ descending +   │               │
                   │ height < 5m    │               │
                   ▼                ▼               │
            ┌───────────────────────────┐           │
            │     LANDING_ALIGN         │           │
            │                           │           │
            │ Strong corrective torque  │           │
            │ Snap car flat to ground   │           │
            │ Dampen all angular vel    │           │
            └───────────────┬───────────┘           │
                            │ wheels touch           │
                            │ ground                 │
                            └───────────────────────┘
```

#### Angular Damping by State

| State | Angular Damping | Reason |
|---|---|---|
| GROUNDED | 6.0 + wheelContact/2 | Stabilize driving |
| RAMP_FLIP | 0.5 | Let it spin freely |
| JUMP_SPIN | 0.5 | Let it spin freely |
| LANDING | 3.0 | Allow alignment torque but not too loose |
| AIRBORNE (no trick) | Normal (6+) | Gentle auto-stabilize |

#### Disable Forward Air Movement During Tricks

When a trick is active (`ramp_flip` or `jump_spin`), disable the normal airborne forward/backward forces. The car should follow its ballistic trajectory — no steering mid-flip. Air movement is only for the `"none"` trick state (normal airborne without a trick).

---

### 9.6 Stunt Completion & Rewards

```
TRICK            TRIGGER                   ROTATION AXIS   NITRO REWARD   TIME
────────────────────────────────────────────────────────────────────────────────
Front Flip       Ramp launch (automatic)   Local X         +20 per 360°   auto
Double Flip      2+ flips from big ramp    Local X         +50            auto
Triple+ Flip     3+ flips from mega ramp   Local X         +100           auto
360 Spin         Jump + L/R (player)       World Y         +25 per 360°   ~0.7s
720 Spin         Jump + 2 full rotations   World Y         +60            ~1.4s
1080+ Spin       Jump + 3+ rotations       World Y         +120           ~2.1s
Combo Bonus      Multiple tricks in 1 air  Mixed           +50%           -
```

```gdscript
var trick_rotation_accumulated: Vector3 = Vector3.ZERO  # track total rotation per axis

func track_trick_completion(delta):
    if trick_state == "ramp_flip":
        trick_rotation_accumulated.x += abs(angular_velocity.x) * delta
        if trick_rotation_accumulated.x > TAU:
            complete_trick("Front Flip 360!")
            trick_rotation_accumulated.x -= TAU
    
    elif trick_state == "jump_spin":
        trick_rotation_accumulated.y += abs(angular_velocity.y) * delta
        if trick_rotation_accumulated.y > TAU:
            complete_trick("360 Spin!")
            trick_rotation_accumulated.y -= TAU
    
    elif trick_state == "none" or trick_state == "landing":
        trick_rotation_accumulated = Vector3.ZERO  # reset on land

func complete_trick(name: String):
    # Nitro reward
    nitro_current += nitro_recharge_stunt
    # UI popup: trick name text flies up from car
    show_trick_popup(name)
    # SFX: satisfying "ding" or whoosh
    play_trick_sound()
```

---

## 10. Mid-Air Controls: Double Jump & Glider

### 10.1 Mid-Air Jump (Double Jump)

```gdscript
var air_jumps_remaining = 1  # reset on ground contact
var air_jump_force = 8.0     # impulse multiplier

func try_air_jump():
    if airborne and air_jumps_remaining > 0:
        air_jumps_remaining -= 1
        
        # Upward impulse
        linear_velocity.y = max(linear_velocity.y, 0)  # cancel downward velocity first
        apply_central_impulse(Vector3.UP * air_jump_force * mass)
        
        # VFX: burst ring particles downward, shockwave shader
        spawn_air_jump_vfx()
        # SFX: whoosh + boing
```

### 10.2 Glider / Paraglider

**Algorithm: Lift + Drag aerodynamic model (simplified)**

```gdscript
var glider_deployed = false
var glider_lift_coefficient = 0.6
var glider_drag_coefficient = 0.3
var glider_control_authority = 2.0  # pitch/yaw sensitivity while gliding

func process_glider(delta):
    if not glider_deployed or not airborne:
        return
    
    var air_velocity = linear_velocity
    var speed = air_velocity.length()
    var forward = -global_transform.basis.z
    var up = global_transform.basis.y
    
    # Lift: perpendicular to velocity, proportional to speed²
    var lift_dir = air_velocity.cross(global_transform.basis.x).normalized()
    if lift_dir.dot(Vector3.UP) < 0:
        lift_dir = -lift_dir  # always lift upward-ish
    var lift_force = lift_dir * glider_lift_coefficient * speed * speed * 0.5
    
    # Drag: opposite to velocity, proportional to speed²
    var drag_force = -air_velocity.normalized() * glider_drag_coefficient * speed * speed * 0.5
    
    # Reduce gravity while gliding
    gravity_scale = 0.3  # floaty!
    
    apply_central_force(lift_force + drag_force)
    
    # Pitch and yaw control
    var pitch_input = Input.get_axis("accelerate", "brake")
    var yaw_input = Input.get_axis("steer_left", "steer_right")
    apply_torque(global_transform.basis * Vector3(
        pitch_input * glider_control_authority,
        yaw_input * glider_control_authority * 0.5,
        -yaw_input * glider_control_authority * 0.3  # slight roll with yaw
    ))

func deploy_glider():
    if airborne and not glider_deployed:
        glider_deployed = true
        glider_mesh.visible = true
        # Animation: glider unfolds with squash-stretch
        # Sound: fabric whoosh
        
func retract_glider():
    glider_deployed = false
    glider_mesh.visible = false
    gravity_scale = 1.0
```

### 10.3 Ground Pound (Air → Slam Down)

```gdscript
func ground_pound():
    if airborne:
        linear_velocity = Vector3(linear_velocity.x * 0.3, -ground_pound_speed, linear_velocity.z * 0.3)
        retract_glider()
        # On impact: big shockwave, damages nearby racers, huge bounce
```

---

## 11. Cartoonish & Arcade Features

### 11.1 Visual Juice

| Feature | Description | Algorithm/Approach |
|---|---|---|
| **Squash & Stretch** | Chassis deforms on landing, acceleration, braking | Lerped scale on mesh child (§8.2) |
| **Wheel Spin Blur** | Wheels become blurred disc at high RPM | Swap mesh to flat disc + radial blur material |
| **Speed Lines** | Screen-space manga-style lines at high speed | Post-process shader, intensity = speed/max_speed |
| **Cartoon Dust Cloud** | Poof cloud on hard direction changes | Billboard sprite sheet particle |
| **Eye Pop on Boost** | Headlights bulge out during nitro | Morph target or scale tween on headlight meshes |
| **Wobble on Idle** | Truck sways slightly when stopped | Sine wave on chassis mesh rotation |
| **Exhaust Pops** | Backfire flames on gear shift / deceleration | GPUParticles3D burst + point light flash |
| **Tire Deformation** | Tires squash at contact point | Shader-based vertex displacement or morph targets |
| **Impact Stars** | Cartoon stars on wall collision | Billboard animated sprite3D |
| **Rainbow Trail** | Trail behind car at max speed | Ribbon trail mesh with gradient texture |
| **Character Expression** | Driver reacts to events (scared on big air, happy on boost) | Sprite swap or morph targets on driver mesh |

### 11.2 Camera Effects

```gdscript
# Dynamic FOV
var base_fov = 65.0
var speed_fov_add = 20.0      # at max speed
var nitro_fov_add = 15.0      # during nitro
var drift_fov_offset = 5.0    # slight widen during drift

func update_camera(delta):
    var target_fov = base_fov
    target_fov += (speed / max_speed) * speed_fov_add
    if nitro_active:
        target_fov += nitro_fov_add
    if drifting:
        target_fov += drift_fov_offset
        # Offset camera to the side during drift for dramatic angle
        camera_offset.x = lerp(camera_offset.x, drift_direction * 1.5, 3.0 * delta)
    else:
        camera_offset.x = lerp(camera_offset.x, 0.0, 5.0 * delta)
    
    camera.fov = lerp(camera.fov, target_fov, 5.0 * delta)
    
    # Camera shake
    if shake_timer > 0:
        shake_timer -= delta
        camera.h_offset = randf_range(-shake_intensity, shake_intensity)
        camera.v_offset = randf_range(-shake_intensity, shake_intensity)
```

### 11.3 Fun Arcade Additions

| Feature | Description |
|---|---|
| **Gravity Flip Zones** | Areas where gravity inverts — drive on ceiling! |
| **Bouncy Mushrooms** | Giant mushrooms that launch player sky-high |
| **Cannon Pipes** | Enter a pipe, get shot across the map on a predetermined trajectory |
| **Shrink / Grow Power-ups** | Change vehicle scale — small = fast & fragile, big = slow & crushing |
| **Magnet Power-up** | Attract nearby collectibles and stick to any surface |
| **Ghost Mode** | Phase through obstacles for 5 seconds |
| **Grapple Hook** | Latch onto grapple points, swing around corners |
| **Banana Peel / Oil Slick** | Drop behind to spin out opponents |
| **Missile Lock-On** | Homing projectile toward nearest racer ahead |
| **Shield Bubble** | Absorb one hit, reflects projectiles |
| **Lightning Strike** | Shrinks all opponents temporarily |
| **Spring Loaded Bumper** | Car launches sideways on contact with walls (pinball style) |
| **Turbo Start** | Timed button press at race start for initial boost |
| **Drafting / Slipstream** | Speed bonus when following close behind another racer |
| **Destruction Derby** | Cars take damage, parts fly off (cosmetic, doesn't affect gameplay) |
| **Horn / Taunt** | Honk at opponents, purely cosmetic fun |

---

## 12. Track & Environment Systems

### 12.1 Track Design Elements

```
TRACK PIECES (modular):
├── Straight Road (flat, banked, crowned)
├── Curve (gentle, hairpin, S-curve)
├── Ramp (small, large, mega-launch)
├── Loop (360° vertical loop)
├── Corkscrew (360° spiral)
├── Half-Pipe Section
├── Bridge / Overpass
├── Split Path (shortcut risk/reward)
├── Water Splash Zone (slow + VFX)
├── Sand Dune (climb + jump opportunity)
├── Ice Bridge (low friction + narrow)
├── Boost Pad Strip
├── Destructible Barriers (break through!)
├── Moving Platforms
├── Rotating Obstacles (windmill, spinning pillars)
└── Final Lap Shortcut (opens on last lap only)
```

### 12.2 Checkpoint & Lap System

```gdscript
# Anti-cheat lap counting
var checkpoints_hit: Array[bool] = []
var current_lap = 0
var total_laps = 3

func _on_checkpoint_entered(checkpoint_index: int):
    checkpoints_hit[checkpoint_index] = true

func _on_finish_line_crossed():
    if checkpoints_hit.all(func(v): return v):
        current_lap += 1
        checkpoints_hit.fill(false)
        if current_lap >= total_laps:
            finish_race()
```

### 12.3 Respawn System

```gdscript
# If car falls off track, is stuck, or player presses respawn
func respawn():
    var spawn_point = get_nearest_checkpoint_position()
    var spawn_dir = get_checkpoint_forward_direction()
    
    global_position = spawn_point + Vector3.UP * 2.0  # above track
    linear_velocity = spawn_dir * 5.0  # gentle forward push
    angular_velocity = Vector3.ZERO
    global_transform.basis = Basis.looking_at(spawn_dir)
    
    # Brief invulnerability
    invulnerable_timer = 2.0
    # Fade-in VFX
```

---

## 13. Algorithms Reference

### Summary of All Algorithms Used

| System | Algorithm | Complexity | Notes |
|---|---|---|---|
| **Tire Contact** | SphereCast (1 per wheel) | O(1) per wheel | `cast_motion` + `get_rest_info`, sphere radius = 0.4 × tire radius |
| **Suspension** | Spring-Damper (Hooke + viscous) | O(1) per wheel | Asymmetric damping for bounce feel |
| **Progressive Spring** | Quadratic stiffness ramp | O(1) | `k * (1 + ratio * factor)` |
| **Anti-Roll Bar** | Compression differential transfer | O(1) per axle | Stabilizes cornering |
| **Steering** | Speed-sensitive yaw torque | O(1) | Ackermann simplified |
| **Tire Grip** | Curve-sampled slip model | O(1) lookup | Pacejka-lite via `Curve.sample()` |
| **Drift** | Rear grip reduction + yaw maintenance | O(1) | PD controller on drift angle |
| **Surface Friction** | Metadata lookup on collider | O(1) | Per-surface multiplier table |
| **Acceleration** | Quadratic falloff force curve | O(1) | `1 - (v/vmax)²` |
| **Nitro** | Resource pool (charge/drain) | O(1) | Recharged by gameplay actions |
| **Ramp Launch** | Velocity redirect + impulse | O(1) | Area3D trigger |
| **Ramp Flip (X-axis)** | Auto angular velocity from launch speed | O(1) | `flip_speed = base + launch_vel_y * scale`, set angvel directly, §9.3 |
| **Jump Spin (Y-axis)** | Player-directed Y-axis angvel override | O(1) | Constant spin speed, L/R controls direction, §9.4 |
| **Landing Alignment** | Quadratic corrective torque + damping | O(1) | Triggers when descending + height < 5m, snaps car flat, §9.3 |
| **Trick State Machine** | 4-state FSM per airborne session | O(1) | none → ramp_flip/jump_spin → landing → none, §9.5 |
| **360 Loop** | Centripetal force + gravity override | O(1) | `F = mv²/r` toward center |
| **Air Stunts** | Rotation accumulator + threshold detect | O(1) | Track total rotation per axis, §9.6 |
| **Double Jump** | Impulse with velocity cancel | O(1) | Cancel downward vel, then impulse up |
| **Glider** | Simplified lift-drag aero model | O(1) | `F_lift ∝ v²`, `F_drag ∝ v²` |
| **Squash-Stretch** | Lerped scale on mesh node | O(1) | Spring animation with overshoot |
| **Camera** | Spring-arm + dynamic FOV + shake | O(1) | Lerp-based smoothing |
| **Checkpoint** | Boolean array + finish-line gate | O(n checkpoints) | Anti-cheat validity |
| **Respawn** | Nearest checkpoint teleport | O(n checkpoints) | With gentle forward push |

---

## 14. Production Roadmap

### Phase 0 — Foundation & Project Setup (Week 1)

```
Status: � Not Started
```

- [ ] **0.1** Set up scene tree structure (RigidBody3D + children as in §2)
- [ ] **0.2** Create monster truck mesh (placeholder box → import .glb later)
- [ ] **0.3** Create big wheel meshes (high-radius spheres → proper tire .glb later)
- [ ] **0.4** Set up input map: accelerate, brake, steer_left, steer_right, drift, jump, nitro, glider, respawn
- [ ] **0.5** Create flat test track with ground plane + walls
- [ ] **0.6** Implement spherecast wheel system from scratch (4 wheels, 1 spherecast per wheel + force application)
- [ ] **0.7** Implement basic spring-damper suspension (Hooke's law + viscous damping)
- [ ] **0.8** Implement basic forward drive force and steering torque
- [ ] **0.9** Add debug visualization (ray lines, contact points, force vectors)

**Deliverable:** Monster truck box drives on flat ground with spherecast suspension, built entirely from scratch.

---

### Phase 1 — Core Driving Feel (Week 2)

```
Status: 🔴 Not Started
```

- [ ] **1.1** Implement asymmetric damping (bouncy rebound, stiff compression)
- [ ] **1.2** Implement progressive spring rate for monster truck feel
- [ ] **1.3** Tune suspension values (rest length 0.7m, wheel radius 0.6m)
- [ ] **1.4** Add anti-roll bar to prevent excessive tipover
- [ ] **1.5** Implement speed-sensitive steering (less angle at high speed)
- [ ] **1.6** Create grip curves (Curve resources) for front/rear tires
- [ ] **1.7** Implement arcade acceleration model (quadratic falloff)
- [ ] **1.8** Add engine braking and active braking
- [ ] **1.9** Tune mass, center of mass, angular damping for stable driving
- [ ] **1.10** Create debug HUD: speed, RPM, suspension compression bars, state

**Deliverable:** Monster truck that feels bouncy, responsive, and fun to drive in circles.

---

### Phase 2 — Drift System (Week 3)

```
Status: 🔴 Not Started
```

- [ ] **2.1** Add drift state machine (NORMAL → DRIFTING → BOOST)
- [ ] **2.2** Implement rear grip reduction when drift button held
- [ ] **2.3** Add drift angle maintenance (PD controller on yaw)
- [ ] **2.4** Implement counter-steer assist for stable drifts
- [ ] **2.5** Add drift timer with 3 boost levels (blue/orange/purple)
- [ ] **2.6** Implement drift release boost (speed impulse proportional to drift time)
- [ ] **2.7** Add tire smoke particles at rear wheels during drift
- [ ] **2.8** Add tire mark trail (ImmediateMesh or decal projector)
- [ ] **2.9** Add drift spark particles (color matches boost level)
- [ ] **2.10** Add drift sound effects (tire screech, pitch varies with slip angle)

**Deliverable:** Mario Kart-style drift with visual feedback and boost reward.

---

### Phase 3 — Surface Friction & Track (Week 4)

```
Status: 🔴 Not Started
```

- [ ] **3.1** Implement surface type detection via collider metadata
- [ ] **3.2** Create friction multiplier table (asphalt, dirt, ice, grass, mud, sand)
- [ ] **3.3** Apply surface friction to tire grip calculations
- [ ] **3.4** Add per-surface particle effects (dust, mud splash, ice sparkles)
- [ ] **3.5** Add per-surface sound effects
- [ ] **3.6** Build modular track pieces: straight, curve, banked turn
- [ ] **3.7** Build ramp pieces: small, large, mega-launch
- [ ] **3.8** Add boost pad strips (speed + grip bonus)
- [ ] **3.9** Add oil slick hazards (near-zero grip)
- [ ] **3.10** Assemble first complete test circuit with variety of surfaces

**Deliverable:** Complete circuit with varied surfaces, each feeling distinct.

---

### Phase 4 — Jumps, Ramps & Loops (Week 5)

```
Status: 🔴 Not Started
```

- [ ] **4.1** Implement ramp launch system (Area3D trigger + velocity redirect) (§9.1)
- [ ] **4.2** Add minimum launch speed guarantee (arcade assist)
- [ ] **4.3** Implement automatic X-axis front flips on ramp launch — flip speed scales with launch velocity (§9.3)
- [ ] **4.4** Implement landing alignment system — auto-flatten car before touchdown with strong corrective torque (§9.3)
- [ ] **4.5** Implement 360° loop zones with centripetal force + gravity override (§9.2)
- [ ] **4.6** Add loop minimum speed assist (prevent falling off loop)
- [ ] **4.7** Implement car alignment in loops (up-vector toward track surface)
- [ ] **4.8** Build loop track piece (mesh + collision + Area3D zones)
- [ ] **4.9** Add half-pipe section for wall rides
- [ ] **4.10** Add landing impact system (squash, particles, screen shake, sound)

**Deliverable:** Monster truck flying through the air with automatic ramp flips, completing loops, and landing flat.

---

### Phase 5 — Air Controls & Stunts (Week 6)

```
Status: 🔴 Not Started
```

- [ ] **5.1** Implement airborne state detection and trick state machine (grounded → ramp_flip/jump_spin → landing → grounded) (§9.5)
- [ ] **5.2** Implement Y-axis 360° spins on jump — Mario Kart style flat spins with Left/Right direction control (§9.4)
- [ ] **5.3** Implement stunt rotation accumulator — track completed 360°s per axis (§9.6)
- [ ] **5.4** Add stunt completion rewards (nitro recharge, score popup, trick name UI)
- [ ] **5.5** Add stunt combo system (multiplier for chaining flips + spins in one air session)
- [ ] **5.6** Tune angular damping per trick state (0.5 during tricks, 6+ grounded) (§9.5)
- [ ] **5.7** Implement mid-air double jump (§10.1)
- [ ] **5.8** Implement glider deployment and retraction (§10.2)
- [ ] **5.9** Add glider aerodynamics (lift + drag + pitch/yaw control)
- [ ] **5.10** Implement ground pound / dive (§10.3)

**Deliverable:** Full aerial trick system — automatic ramp flips, player-controlled jump spins, glider + double jump.

---

### Phase 6 — Nitro Boost System (Week 7)

```
Status: 🔴 Not Started
```

- [ ] **6.1** Implement nitro resource pool (charge, drain, UI bar)
- [ ] **6.2** Add nitro recharge from drifting, stunts, air time, pickups
- [ ] **6.3** Implement nitro activation (force boost + speed cap increase)
- [ ] **6.4** Add nitro VFX: exhaust flames, speed lines, trail
- [ ] **6.5** Add camera FOV increase during nitro
- [ ] **6.6** Add chromatic aberration post-process during nitro
- [ ] **6.7** Add nitro sound layering (base engine + boost roar)
- [ ] **6.8** Implement turbo start (timed input at countdown)
- [ ] **6.9** Add nitro pickup items scattered on track
- [ ] **6.10** Balance nitro economy (earn rate vs drain rate vs boost power)

**Deliverable:** Complete nitro system that rewards skillful play.

---

### Phase 7 — Cartoon Visual Juice (Week 8)

```
Status: 🔴 Not Started
```

- [ ] **7.1** Implement squash & stretch on chassis mesh (§8.2)
- [ ] **7.2** Add wheel spin blur (mesh swap at high RPM)
- [ ] **7.3** Add exhaust backfire particles on deceleration
- [ ] **7.4** Add cartoon dust poof on sharp direction changes
- [ ] **7.5** Add screen shake system (impacts, boosts, explosions)
- [ ] **7.6** Add impact stars on wall collision (billboard sprite)
- [ ] **7.7** Add rainbow trail at max speed
- [ ] **7.8** Implement dynamic camera (speed FOV, drift offset, shake)
- [ ] **7.9** Add character expression system (driver face reactions)
- [ ] **7.10** Add wobble/idle animation when truck is stopped

**Deliverable:** The game FEELS cartoonish and alive. Every action has satisfying feedback.

---

### Phase 8 — Power-ups & Items (Week 9–10)

```
Status: 🔴 Not Started
```

- [ ] **8.1** Create item box system (floating ? boxes on track)
- [ ] **8.2** Implement item inventory (hold one item at a time)
- [ ] **8.3** Add forward projectile (missile / fireball)
- [ ] **8.4** Add drop-behind hazard (banana peel / oil slick)
- [ ] **8.5** Add shield bubble (absorb one hit)
- [ ] **8.6** Add speed mushroom (instant burst of speed)
- [ ] **8.7** Add shrink/grow power-up (scale change + stat modification)
- [ ] **8.8** Add homing missile (locks onto nearest racer ahead)
- [ ] **8.9** Add lightning strike (affects all opponents)
- [ ] **8.10** Balance item distribution (better items when further behind — rubber banding)

**Deliverable:** Fun competitive item system with rubber-banding for close races.

---

### Phase 9 — AI Opponents (Week 11–12)

```
Status: 🔴 Not Started
```

- [ ] **9.1** Create path/spline following AI along track centerline
- [ ] **9.2** Implement AI steering (PD controller toward next waypoint)
- [ ] **9.3** Add AI speed control (brake before curves, accelerate on straights)
- [ ] **9.4** Implement AI drift (trigger drift on sharp corners)
- [ ] **9.5** Add AI item usage (use items strategically)
- [ ] **9.6** Implement rubber-band AI (speed up when behind, slow down when ahead)
- [ ] **9.7** Add AI personality types (aggressive, defensive, erratic)
- [ ] **9.8** Implement AI collision avoidance (don't pile up)
- [ ] **9.9** Add AI trick execution on ramps (cosmetic, earns nitro)
- [ ] **9.10** Balance AI to create close, exciting races

**Deliverable:** 3–7 AI opponents that create fun, competitive races.

---

### Phase 10 — Audio & Music (Week 13)

```
Status: 🔴 Not Started
```

- [ ] **10.1** Engine sound: layered RPM-based audio (idle + low + mid + high + redline)
- [ ] **10.2** Tire sounds: screech (drift), crunch (gravel), splash (water)
- [ ] **10.3** Suspension: creak and bounce sounds, spring boing on big impacts
- [ ] **10.4** Wind sound: increases with speed, dominates during gliding
- [ ] **10.5** Impact sounds: thuds, crashes, wall hits, landing
- [ ] **10.6** Nitro: roaring flame, turbo spool-up, whine
- [ ] **10.7** Item SFX: pickup jingle, use sound, hit sound
- [ ] **10.8** UI sounds: countdown beeps, lap complete, position callout
- [ ] **10.9** Music: upbeat arcade racing track, intensity increases in final lap
- [ ] **10.10** Announcer voice lines: "MONSTER AIR!", "EPIC DRIFT!", "FINAL LAP!"

**Deliverable:** Full audio landscape that makes gameplay feel alive and punchy.

---

### Phase 11 — Track Design & Polish (Week 14–15)

```
Status: 🔴 Not Started
```

- [ ] **11.1** Design 3 complete tracks with increasing difficulty
  - Track 1: "Monster Meadow" — gentle curves, small ramps, forgiving
  - Track 2: "Volcano Valley" — lava hazards, big jumps, loop-de-loop
  - Track 3: "Sky Fortress" — floating islands, long glider sections, zero-g zones
- [ ] **11.2** Add environmental decorations (trees, rocks, crowds, signs)
- [ ] **11.3** Add animated crowd / spectators at key points
- [ ] **11.4** Add destructible scenery (fences, crates, signs)
- [ ] **11.5** Add dynamic weather (rain = wet surface friction, snow = ice patches)
- [ ] **11.6** Implement checkpoint and lap counting system
- [ ] **11.7** Add respawn system (fall-off-track recovery)
- [ ] **11.8** Add race countdown and finish sequence
- [ ] **11.9** Add position tracking (1st, 2nd, 3rd... display)
- [ ] **11.10** Add post-race results screen with stats

**Deliverable:** Multiple polished, replayable tracks with complete race flow.

---

### Phase 12 — UI & Game Flow (Week 16)

```
Status: 🔴 Not Started
```

- [ ] **12.1** Main menu (play, settings, truck select)
- [ ] **12.2** Truck selection screen (3–5 trucks with different stats)
- [ ] **12.3** Track selection screen with preview
- [ ] **12.4** In-race HUD: speedometer, position, lap, nitro bar, item slot, minimap
- [ ] **12.5** Race countdown (3… 2… 1… GO! with turbo start)
- [ ] **12.6** Pause menu
- [ ] **12.7** Results screen with trick stats, best drift, biggest air
- [ ] **12.8** Settings: controls, audio volume, graphics quality
- [ ] **12.9** Gamepad support and remapping
- [ ] **12.10** Split-screen local multiplayer (2–4 players, viewport split)

**Deliverable:** Complete game flow from menu to race to results.

---

### Phase 13 — Performance & Optimization (Week 17)

```
Status: 🔴 Not Started
```

- [ ] **13.1** Profile physics: ensure all 4 wheel raycasts + forces < 0.5ms
- [ ] **13.2** LOD system for track objects
- [ ] **13.3** Particle pooling (reuse particle systems instead of create/destroy)
- [ ] **13.4** GPU instancing for repeated track elements
- [ ] **13.5** Occlusion culling for complex tracks
- [ ] **13.6** Physics tick rate tuning (60Hz default, test 120Hz for smoother feel)
- [ ] **13.7** Audio stream pooling
- [ ] **13.8** Memory profiling and cleanup
- [ ] **13.9** Target: stable 60 FPS on mid-range hardware
- [ ] **13.10** Build and test on target platforms (desktop, web export if applicable)

**Deliverable:** Smooth, stable 60 FPS gameplay.

---

### Phase 14 — Playtesting & Tuning (Week 18–19)

```
Status: 🔴 Not Started
```

- [ ] **14.1** Tune suspension for maximum bounce-fun without losing control
- [ ] **14.2** Tune drift grip curve for satisfying slide-to-boost loop
- [ ] **14.3** Tune nitro economy for "always doing something cool" feel
- [ ] **14.4** Tune ramp launches for consistent, dramatic air time
- [ ] **14.5** Tune AI rubber banding for close finishes
- [ ] **14.6** Tune item frequency and power for exciting but fair gameplay
- [ ] **14.7** Tune camera feel (FOV transitions, shake intensity, drift offset)
- [ ] **14.8** Test all surface types feel distinct and fun
- [ ] **14.9** Full race playthrough testing (all 3 tracks, all positions, all items)
- [ ] **14.10** Bug fix pass: collision edge cases, physics glitches, visual artifacts

**Deliverable:** Tuned, polished, FUN racing game.

---

### Milestone Summary

| Phase | Name | Duration | Key Deliverable |
|---|---|---|---|
| 0 | Foundation | Week 1 | Truck drives on flat ground |
| 1 | Core Feel | Week 2 | Bouncy, responsive monster truck |
| 2 | Drift | Week 3 | Mario Kart drift + boost |
| 3 | Surfaces | Week 4 | Multi-surface track circuit |
| 4 | Jumps & Loops | Week 5 | Ramps, 360° loops, wall rides |
| 5 | Air Controls | Week 6 | Double jump, stunts, glider |
| 6 | Nitro | Week 7 | Charge-and-burn boost system |
| 7 | Visual Juice | Week 8 | Squash-stretch, particles, screen effects |
| 8 | Power-ups | Week 9–10 | Competitive item system |
| 9 | AI | Week 11–12 | Fun AI opponents |
| 10 | Audio | Week 13 | Complete soundscape |
| 11 | Tracks | Week 14–15 | 3 polished tracks |
| 12 | UI & Flow | Week 16 | Full game loop |
| 13 | Optimization | Week 17 | Stable 60 FPS |
| 14 | Polish | Week 18–19 | Tuned and fun |

**Total estimated development time: ~19 weeks (solo developer)**

---

### Key Design Principles

1. **Fun > Realism** — Every mechanic should make the player smile. If physics says no but fun says yes, override the physics.
2. **Responsive Controls** — Zero input lag. The truck should respond instantly. Use force + direct velocity blending.
3. **Reward Skill** — Drifting, stunts, and boost management should reward skilled players with faster times.
4. **Rubber Banding** — Keep races close. Being in last place should feel recoverable, not hopeless.
5. **Juice Everything** — Every action needs visual and audio feedback. No silent, invisible mechanics.
6. **Monster Truck Identity** — Big wheels, bouncy suspension, crushing terrain. This isn't a go-kart. It should feel HUGE.
