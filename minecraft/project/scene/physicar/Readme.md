# PhysiCar — Arcade Monster Truck Racing

> Mario Kart meets Monster Jam meets Skylanders SuperChargers — a game so silly your face hurts from grinning. Bouncy suspension, drifting with rainbow sparks, nitro that sets the sky on fire, mid-air stunts that make the crowd scream, predictive landing bullseyes, giant rubber duckies as weapons, and tracks made of candy, dinosaurs, and toy blocks. All on a RigidBody3D chassis with spherecast wheels in Godot 4.6.

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
10. [Predictive Landing System](#10-predictive-landing-system)
11. [Mid-Air Stunt Scoring (Skylanders Style)](#11-mid-air-stunt-scoring-skylanders-style)
12. [Mid-Air Controls: Double Jump & Glider](#12-mid-air-controls-double-jump--glider)
13. [Cartoonish & Arcade Features](#13-cartoonish--arcade-features)
14. [Silly Celebrations, Taunts & Driver Reactions](#14-silly-celebrations-taunts--driver-reactions)
15. [Throwable Items & Wacky Weapons](#15-throwable-items--wacky-weapons)
16. [Track Themes & Silly Environments](#16-track-themes--silly-environments)
17. [Track & Environment Systems](#17-track--environment-systems)
18. [Algorithms Reference](#18-algorithms-reference)
19. [Production Roadmap](#19-production-roadmap)

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

## 10. Predictive Landing System

### Skylanders SuperChargers-Inspired Landing Indicator

When the car is airborne, a **projected landing reticle** appears on the ground below, showing the player exactly where they'll touch down. This gives critical feedback during big jumps, ramp flips, and glider descents — the player can steer mid-air to aim for boost pads, avoid hazards, or nail a perfect landing zone.

#### Design Philosophy

In Skylanders SuperChargers, the landing shadow/reticle grows as the vehicle descends and pulses when close to impact. Our system goes further: we simulate the ballistic trajectory forward in time, accounting for gravity, current velocity, air drag, and glider lift, then project a ground marker at the predicted impact point. The reticle changes shape and color based on landing quality.

#### 10.1 Trajectory Prediction Algorithm

```gdscript
# --- Tuning constants ---
const PREDICTION_TIMESTEP: float = 0.05      # simulation step (seconds)
const PREDICTION_MAX_STEPS: int = 120         # max lookahead = 6 seconds
const PREDICTION_DRAG: float = 0.01           # simplified air drag

# --- Predict landing position by simulating ballistic arc ---
func predict_landing_position() -> Dictionary:
    if not airborne:
        return {}
    
    var sim_pos = global_position
    var sim_vel = linear_velocity
    var gravity_vec = Vector3.DOWN * 9.8 * gravity_scale
    
    # Account for glider if deployed
    var has_glider = glider_deployed
    
    for step in PREDICTION_MAX_STEPS:
        # Apply gravity
        sim_vel += gravity_vec * PREDICTION_TIMESTEP
        
        # Apply simplified air drag
        sim_vel *= (1.0 - PREDICTION_DRAG * PREDICTION_TIMESTEP)
        
        # If glider is deployed, apply simplified lift
        if has_glider:
            var speed = sim_vel.length()
            var lift = Vector3.UP * glider_lift_coefficient * speed * 0.3
            sim_vel += lift * PREDICTION_TIMESTEP
        
        # Step position forward
        sim_pos += sim_vel * PREDICTION_TIMESTEP
        
        # Raycast downward from simulated position to check ground
        var space = get_world_3d().direct_space_state
        var ray = PhysicsRayQueryParameters3D.new()
        ray.from = sim_pos
        ray.to = sim_pos + Vector3.DOWN * 2.0
        ray.exclude = [self]
        var hit = space.intersect_ray(ray)
        
        if hit and sim_pos.y <= hit.position.y + 1.0:
            var time_to_impact = step * PREDICTION_TIMESTEP
            var impact_speed = sim_vel.length()
            return {
                "position": hit.position,
                "normal": hit.normal,
                "time": time_to_impact,
                "impact_speed": impact_speed,
                "quality": evaluate_landing_quality(hit.position, hit.normal)
            }
    
    return {}  # can't predict landing within max lookahead
```

#### 10.2 Landing Quality Evaluation

The reticle communicates landing quality — Skylanders style:

```gdscript
enum LandingQuality { PERFECT, GOOD, ROUGH, DANGER }

func evaluate_landing_quality(pos: Vector3, normal: Vector3) -> LandingQuality:
    # Check surface angle — flat is best
    var flatness = normal.dot(Vector3.UP)  # 1.0 = perfectly flat
    
    # Check if landing on a boost pad, hazard, or normal surface
    var surface = get_surface_type_at(pos)
    
    # Check if there's a designated "landing zone" nearby
    var on_landing_zone = is_near_landing_zone(pos)
    
    if on_landing_zone and flatness > 0.95:
        return LandingQuality.PERFECT    # bullseye!
    elif flatness > 0.85 and surface != SurfaceType.OIL_SLICK:
        return LandingQuality.GOOD       # clean landing
    elif flatness > 0.6:
        return LandingQuality.ROUGH      # bumpy but survivable
    else:
        return LandingQuality.DANGER     # steep slope or hazard
```

#### 10.3 Visual Reticle Rendering

```gdscript
# Landing reticle: Decal3D or MeshInstance3D projected on ground
var landing_reticle: Decal3D
var reticle_ring: MeshInstance3D  # outer ring pulse animation

const RETICLE_COLORS = {
    LandingQuality.PERFECT: Color(0.2, 1.0, 0.4, 0.9),   # green + glow
    LandingQuality.GOOD:    Color(1.0, 1.0, 0.3, 0.7),   # yellow
    LandingQuality.ROUGH:   Color(1.0, 0.5, 0.1, 0.6),   # orange
    LandingQuality.DANGER:  Color(1.0, 0.1, 0.1, 0.8),   # red + pulse
}

func update_landing_reticle(delta):
    if not airborne:
        landing_reticle.visible = false
        return
    
    var prediction = predict_landing_position()
    if prediction.is_empty():
        landing_reticle.visible = false
        return
    
    landing_reticle.visible = true
    
    # Position reticle at predicted landing point
    landing_reticle.global_position = prediction.position + prediction.normal * 0.05
    
    # Align reticle to ground normal
    var up = prediction.normal
    var right = up.cross(Vector3.FORWARD).normalized()
    var forward = right.cross(up).normalized()
    landing_reticle.global_transform.basis = Basis(right, up, forward)
    
    # Scale reticle: grows as car descends (closer to impact)
    var time_factor = clamp(1.0 - prediction.time / 3.0, 0.3, 1.0)
    var reticle_scale = lerp(2.5, 1.0, time_factor)  # shrinks to bullseye
    landing_reticle.scale = Vector3.ONE * reticle_scale
    
    # Color based on landing quality
    var quality = prediction.quality
    var target_color = RETICLE_COLORS[quality]
    landing_reticle.modulate = landing_reticle.modulate.lerp(target_color, 8.0 * delta)
    
    # Pulse animation when close to landing
    if prediction.time < 0.5:
        var pulse = 1.0 + sin(Time.get_ticks_msec() * 0.02) * 0.15
        landing_reticle.scale *= pulse
    
    # PERFECT landing zone: add particle ring effect
    if quality == LandingQuality.PERFECT:
        reticle_ring.visible = true
        reticle_ring.global_position = prediction.position
        # Rotating ring animation
        reticle_ring.rotate_y(3.0 * delta)
    else:
        reticle_ring.visible = false
```

#### 10.4 Trajectory Trail (Optional — Hot Wheels Style)

Optionally draw a dotted arc showing the predicted flight path:

```gdscript
var trajectory_points: PackedVector3Array
const TRAIL_DOT_COUNT: int = 20

func update_trajectory_trail():
    if not airborne:
        trajectory_line.visible = false
        return
    
    trajectory_points.clear()
    var sim_pos = global_position
    var sim_vel = linear_velocity
    var gravity_vec = Vector3.DOWN * 9.8 * gravity_scale
    var step_interval = max(1, PREDICTION_MAX_STEPS / TRAIL_DOT_COUNT)
    
    for step in PREDICTION_MAX_STEPS:
        sim_vel += gravity_vec * PREDICTION_TIMESTEP
        sim_vel *= (1.0 - PREDICTION_DRAG * PREDICTION_TIMESTEP)
        sim_pos += sim_vel * PREDICTION_TIMESTEP
        
        if step % step_interval == 0:
            trajectory_points.append(sim_pos)
        
        # Stop at ground
        if sim_pos.y < 0:  # simplified; use raycast for accuracy
            break
    
    trajectory_line.visible = true
    trajectory_line.points = trajectory_points
```

#### 10.5 Perfect Landing Bonus

Landing on a **designated landing zone** (marked area on the track after ramps) with good alignment earns bonus points and nitro:

```gdscript
func on_landing(impact_velocity: float):
    # ... existing landing logic (squash, particles, shake) ...
    
    # Check for perfect landing bonus
    var landing_pos = global_position
    if is_near_landing_zone(landing_pos):
        var flatness = get_ground_normal().dot(Vector3.UP)
        if flatness > 0.95:
            # PERFECT LANDING!
            award_points("Perfect Landing!", 200)
            nitro_current += 30.0
            spawn_perfect_landing_vfx()  # golden shockwave ring
            play_sound("perfect_landing")  # satisfying chime
            camera_slow_mo(0.3, 0.5)  # brief slow-motion for drama
        elif flatness > 0.8:
            award_points("Clean Landing", 75)
            nitro_current += 10.0
```

---

## 11. Mid-Air Stunt Scoring (Skylanders Style)

### Skylanders SuperChargers Point System

Inspired by Skylanders SuperChargers' vehicle stunt system: every aerial trick earns **Stunt Points** that accumulate during a single air session. Points are only **banked** (confirmed) upon a clean landing. Crash or bail = lose all pending points. This creates risk/reward tension — do one more flip for more points, or play it safe and land?

#### 11.1 Point Architecture

```
STUNT SCORING FLOW:

  AIRBORNE                                    LANDING
  ┌──────────────────────────────────────┐    ┌──────────────┐
  │  Perform stunts → accumulate         │    │ Clean land?  │
  │  PENDING points + combo multiplier   │───▶│  YES → Bank! │
  │                                      │    │  NO  → Lose  │
  │  Front Flip 360°    = 100 pts        │    │  all pending │
  │  360 Spin           = 80 pts         │    └──────────────┘
  │  Barrel Roll        = 120 pts        │
  │  Combo Multiplier   = x1.5, x2, x3  │    Score: 4,320
  │  Air Time Bonus     = 10 pts/sec     │    ───────────────
  │  Height Bonus       = 5 pts/meter    │    TOTAL: 12,850
  └──────────────────────────────────────┘
```

#### 11.2 Stunt Point Values

```gdscript
# --- Base point values per trick ---
const STUNT_POINTS = {
    "Front Flip 360":    100,
    "Double Flip":       250,
    "Triple Flip":       500,
    "Quad+ Flip":        800,
    "360 Spin":          80,
    "720 Spin":          200,
    "1080 Spin":         400,
    "1440+ Spin":        700,
    "Barrel Roll":       120,    # diagonal roll (X+Z combined rotation)
    "Cork Screw":        300,    # simultaneous flip + spin
    "Air Time Bonus":    10,     # per second airborne
    "Height Bonus":      5,      # per meter above launch point
    "Perfect Landing":   200,    # land on designated zone
    "Clean Landing":     75,     # land flat on any surface
    "Nitro Flip":        150,    # flip while nitro is active
    "Glider Trick":      60,     # deploy/retract glider mid-stunt
}
```

#### 11.3 Combo Multiplier System

Chaining different tricks in a single air session builds a **combo multiplier**, Skylanders-style:

```gdscript
var pending_points: int = 0
var combo_count: int = 0
var combo_multiplier: float = 1.0
var unique_tricks_this_air: Array[String] = []
var air_session_active: bool = false
var launch_height: float = 0.0
var air_time_accumulated: float = 0.0

# Combo multiplier tiers
const COMBO_TIERS = {
    1: 1.0,    # single trick
    2: 1.5,    # two different tricks
    3: 2.0,    # three different tricks  
    4: 2.5,    # four different tricks
    5: 3.0,    # five+ different tricks = MEGA COMBO
}

func start_air_session():
    air_session_active = true
    pending_points = 0
    combo_count = 0
    combo_multiplier = 1.0
    unique_tricks_this_air.clear()
    launch_height = global_position.y
    air_time_accumulated = 0.0

func add_stunt_points(trick_name: String, base_points: int):
    # Track unique tricks for combo
    if trick_name not in unique_tricks_this_air:
        unique_tricks_this_air.append(trick_name)
        combo_count = unique_tricks_this_air.size()
        
        # Update multiplier based on variety
        for tier in COMBO_TIERS:
            if combo_count >= tier:
                combo_multiplier = COMBO_TIERS[tier]
    
    # Add points with current multiplier
    var earned = int(base_points * combo_multiplier)
    pending_points += earned
    
    # Show floating point text
    show_stunt_popup(trick_name, earned, combo_multiplier)
    
    # Combo announce at thresholds
    if combo_count == 3:
        show_announcement("COMBO x2!")
    elif combo_count == 5:
        show_announcement("MEGA COMBO x3!")
```

#### 11.4 Air Session Tracking

```gdscript
func process_air_session(delta):
    if not airborne:
        return
    
    if not air_session_active:
        start_air_session()
    
    # Accumulate air time points
    air_time_accumulated += delta
    if int(air_time_accumulated) > int(air_time_accumulated - delta):
        # Award air time bonus every full second
        add_stunt_points("Air Time Bonus", STUNT_POINTS["Air Time Bonus"])
    
    # Height bonus (compared to launch height)
    var current_height = global_position.y - launch_height
    if current_height > 0:
        # Award height bonus periodically
        var height_points = int(current_height) * STUNT_POINTS["Height Bonus"]
        # (tracked separately to avoid re-awarding)
```

#### 11.5 Landing — Bank or Bail

The critical moment: points are only confirmed on a **clean landing**.

```gdscript
func finalize_air_session(landing_quality: LandingQuality):
    if not air_session_active:
        return
    
    air_session_active = false
    
    match landing_quality:
        LandingQuality.PERFECT:
            # Bank ALL points + perfect landing bonus
            add_stunt_points("Perfect Landing", STUNT_POINTS["Perfect Landing"])
            bank_points(pending_points)
            show_landing_result("PERFECT!", pending_points, Color.GREEN)
            nitro_current += 30.0
            camera_slow_mo(0.3, 0.5)
        
        LandingQuality.GOOD:
            # Bank all points + small landing bonus
            add_stunt_points("Clean Landing", STUNT_POINTS["Clean Landing"])
            bank_points(pending_points)
            show_landing_result("NICE!", pending_points, Color.YELLOW)
            nitro_current += 15.0
        
        LandingQuality.ROUGH:
            # Bank 50% of points — rough landing penalty
            var banked = int(pending_points * 0.5)
            bank_points(banked)
            show_landing_result("ROUGH...", banked, Color.ORANGE)
            nitro_current += 5.0
        
        LandingQuality.DANGER:
            # BAIL! Lose all pending points
            bank_points(0)
            show_landing_result("BAIL!", 0, Color.RED)
            # Penalty: brief stun / slow-down
            apply_bail_penalty()

func bank_points(points: int):
    total_score += points
    # Update persistent HUD score display
    update_score_display(total_score)
    # SFX: coin/point collection sound scaled to amount
    if points > 500:
        play_sound("big_score")
    elif points > 0:
        play_sound("score_ding")
```

#### 11.6 Stunt Detector — Expanded Trick Recognition

Expand the existing trick rotation accumulator (§9.6) with richer trick detection:

```gdscript
var rotation_accumulator: Vector3 = Vector3.ZERO  # radians per axis
var last_trick_rotation: Vector3 = Vector3.ZERO

func detect_stunts(delta):
    if not airborne:
        return
    
    # Accumulate rotation per axis
    rotation_accumulator.x += abs(angular_velocity.x) * delta
    rotation_accumulator.y += abs(angular_velocity.y) * delta
    rotation_accumulator.z += abs(angular_velocity.z) * delta
    
    # --- Front Flip detection (X-axis) ---
    var flip_count = int(rotation_accumulator.x / TAU)
    var last_flip_count = int(last_trick_rotation.x / TAU)
    if flip_count > last_flip_count:
        match flip_count:
            1: add_stunt_points("Front Flip 360", STUNT_POINTS["Front Flip 360"])
            2: add_stunt_points("Double Flip", STUNT_POINTS["Double Flip"])
            3: add_stunt_points("Triple Flip", STUNT_POINTS["Triple Flip"])
            _: add_stunt_points("Quad+ Flip", STUNT_POINTS["Quad+ Flip"])
    
    # --- Spin detection (Y-axis) ---
    var spin_count = int(rotation_accumulator.y / TAU)
    var last_spin_count = int(last_trick_rotation.y / TAU)
    if spin_count > last_spin_count:
        var spin_deg = spin_count * 360
        match spin_count:
            1: add_stunt_points("360 Spin", STUNT_POINTS["360 Spin"])
            2: add_stunt_points("720 Spin", STUNT_POINTS["720 Spin"])
            3: add_stunt_points("1080 Spin", STUNT_POINTS["1080 Spin"])
            _: add_stunt_points("1440+ Spin", STUNT_POINTS["1440+ Spin"])
    
    # --- Barrel Roll detection (Z-axis) ---
    var roll_count = int(rotation_accumulator.z / TAU)
    var last_roll_count = int(last_trick_rotation.z / TAU)
    if roll_count > last_roll_count:
        add_stunt_points("Barrel Roll", STUNT_POINTS["Barrel Roll"])
    
    # --- Cork Screw detection (simultaneous X + Y rotation) ---
    if abs(angular_velocity.x) > 3.0 and abs(angular_velocity.y) > 3.0:
        # Significant rotation on both axes simultaneously
        var combined_rotation = Vector2(angular_velocity.x, angular_velocity.y).length()
        if combined_rotation * delta > TAU * 0.25:  # quarter turn in combined space
            add_stunt_points("Cork Screw", STUNT_POINTS["Cork Screw"])
    
    last_trick_rotation = rotation_accumulator

func reset_stunt_tracking():
    rotation_accumulator = Vector3.ZERO
    last_trick_rotation = Vector3.ZERO
```

#### 11.7 Stunt Score HUD

```
┌─────────────────────────────────────────────────────────────┐
│                                                             │
│   ┌──────────────────┐                    SCORE: 12,850     │
│   │  FRONT FLIP 360! │  ← trick name flies up               │
│   │    +150 (x1.5)   │  ← points with multiplier            │
│   └──────────────────┘                                      │
│                                                             │
│          ╔═══════════════════════╗                           │
│          ║  PENDING: 1,340 pts  ║  ← at risk until landing  │
│          ║  COMBO: x2.0 (3 tricks) ║                         │
│          ╚═══════════════════════╝                           │
│                                                             │
│   ◎ ←── landing reticle on ground (green = PERFECT zone)    │
│                                                             │
│   [████████████░░░░░] NITRO                                 │
└─────────────────────────────────────────────────────────────┘
```

```gdscript
# HUD elements for stunt scoring
var pending_label: Label  # shows pending points (yellow, pulsing)
var combo_label: Label    # shows combo multiplier
var score_label: Label    # shows total banked score
var trick_popup_queue: Array[Dictionary] = []  # queued trick name popups

func show_stunt_popup(trick_name: String, points: int, multiplier: float):
    var popup = {
        "text": "%s\n+%d (x%.1f)" % [trick_name, points, multiplier],
        "timer": 1.5,
        "start_pos": get_viewport().get_visible_rect().size / 2,
    }
    trick_popup_queue.append(popup)
    
    # Animate: text flies upward and fades out
    # Scale pulse on appear
    # Color matches trick tier (white → blue → gold)

func show_landing_result(text: String, points: int, color: Color):
    # Large centered text: "PERFECT!" or "BAIL!"
    # Points total below
    # Stays on screen for 2 seconds
    # Screen flash matching color
    pass
```

#### 11.8 Score Rewards — What Points Buy

Points aren't just cosmetic — they unlock rewards during the race:

```gdscript
# Score milestone rewards (checked after each bank)
const SCORE_MILESTONES = {
    1000:  {"reward": "nitro_refill",    "amount": 25,  "announce": "Nitro Boost!"},
    3000:  {"reward": "speed_bonus",     "amount": 1.05, "announce": "Speed Up!"},
    5000:  {"reward": "nitro_refill",    "amount": 50,  "announce": "Nitro Surge!"},
    8000:  {"reward": "double_jump",     "amount": 1,   "announce": "Extra Jump!"},
    12000: {"reward": "nitro_max_up",    "amount": 25,  "announce": "Nitro Tank+!"},
    20000: {"reward": "star_power",      "amount": 5.0, "announce": "STAR POWER!"},
}

func check_score_milestones():
    for threshold in SCORE_MILESTONES:
        if total_score >= threshold and not milestones_claimed.has(threshold):
            milestones_claimed.append(threshold)
            var reward = SCORE_MILESTONES[threshold]
            apply_reward(reward)
            show_milestone_announcement(reward.announce)
```

---

## 12. Mid-Air Controls: Double Jump & Glider

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

## 13. Cartoonish & Arcade Features

### 13.1 Visual Juice

| Feature | Description | Algorithm/Approach |
|---|---|---|
| **Squash & Stretch** | Chassis deforms on landing, acceleration, braking | Lerped scale on mesh child (§8.2) |
| **Wheel Spin Blur** | Wheels become blurred disc at high RPM | Swap mesh to flat disc + radial blur material |
| **Speed Lines** | Screen-space manga-style lines at high speed | Post-process shader, intensity = speed/max_speed |
| **Cartoon Dust Cloud** | Poof cloud on hard direction changes | Billboard sprite sheet particle |
| **Eye Pop on Boost** | Headlights bulge out during nitro like a cartoon character seeing a ghost | Morph target or scale tween on headlight meshes |
| **Wobble on Idle** | Truck sways slightly when stopped, like it's breathing | Sine wave on chassis mesh rotation |
| **Exhaust Pops** | Backfire flames on gear shift / deceleration — POP POP POP! | GPUParticles3D burst + point light flash |
| **Tire Deformation** | Tires squash flat at contact point like marshmallows | Shader-based vertex displacement or morph targets |
| **Impact Stars** | Cartoon stars + tweety birds circle around truck on wall collision | Billboard animated sprite3D |
| **Rainbow Trail** | Trail behind car at max speed — taste the rainbow | Ribbon trail mesh with gradient texture |
| **Character Expression** | Driver reacts to events (screaming on big air, maniacal grin on boost, dizzy spiral eyes after spinning) | Sprite swap or morph targets on driver mesh |
| **Confetti Cannon** | Confetti explodes from the truck on mega combos and race wins | Burst GPUParticles3D, multicolor |
| **Googly Eyes** | Truck headlights become googly eyes that wobble with physics | Separate RigidBody3D eyes on springs, or procedural sine offset |
| **Silly Hat Physics** | Optional silly hats on top of truck (propeller beanie, crown, party hat) that flop around with inertia | Child Node3D with spring physics |
| **Stretch Wheels** | Wheels stretch like rubber bands when airborne, snap back on land | Lerp wheel visual offset away from chassis |
| **Tongue Out** | Truck "sticks out its tongue" (a flapping red mesh from the grille) during nitro boost | Animated MeshInstance3D with cloth-like sway |
| **Dizzy Spiral Eyes** | After 3+ consecutive spins, headlights become spinning spirals for 2 seconds | Animated texture swap with spiral rotation |

### 13.2 Drift Flair — Making Drifts Feel AMAZING

| Feature | Description |
|---|---|
| **Musical Drift Sparks** | Spark sounds play ascending musical notes as drift level increases (do-re-mi → full chord at level 3) |
| **Rainbow Tire Marks** | Level 3 drift leaves permanent rainbow skid marks on the track |
| **Drift Lean** | Truck leans dramatically into the drift — wheels on one side nearly lift off the ground |
| **Drift Ghost** | Translucent echo/afterimage trails behind the truck during long drifts |
| **Drift Crown** | After a 5+ second drift chain, a golden "DRIFT KING" crown pops onto the truck |
| **Drift Chain Bonus** | Drifting through multiple consecutive corners without breaking = chain multiplier. Each corner adds +50% to drift boost reward |
| **Counter-Drift Swing** | Flicking from a left drift to a right drift without stopping gives a "Pendulum Swing!" bonus (150 pts + nitro burst) |
| **Smoke Angel** | During ultra-long drifts, the tire smoke behind you forms silly shapes (hearts, stars, skulls) |
| **Crowd Reaction** | Nearby crowd NPCs start chanting and waving faster the longer your drift |

### 13.3 Stunt Flair — Making Stunts Feel RIDICULOUS

| Feature | Description |
|---|---|
| **Silly Stunt Names** | Instead of boring names: "BELLY FLOP!" (failed flip), "DIZZY DONUT!" (5+ spins), "MOON GRAVITY TWIRL!" (glider + spin), "HELICOPTER VIBES!" (rapid Y-spin) |
| **Crowd Goes Wild** | Crowd NPCs shout "OOOOH!", "WHEEE!", "WHAT THE—?!" with escalating intensity per combo level |
| **Slow-Mo Stunt Cam** | On 3+ combo tricks, camera enters brief cinematic slow-mo with dramatic zoom |
| **Stunt Confetti** | Confetti and streamers explode from the truck on every completed 360° rotation |
| **Air Guitar** | If the player does nothing but glide for 3+ seconds, the driver starts playing air guitar |
| **Victory Wiggle** | After landing a MEGA COMBO, the truck does an automatic happy wiggle dance (left-right-left shimmy) |
| **Announcer Hype** | Voice lines escalate: "Nice!" → "AWESOME!" → "UNBELIEVABLE!" → "IS THIS LEGAL?!" → "SOMEBODY CALL THE POLICE!!!" |
| **Replay Ghost** | Your best stunt of the race gets saved and replayed as a transparent ghost for other racers to see |
| **Stunt Ripple** | Landing a big stunt creates a visible shockwave ripple on the ground that other racers can see |

### 13.4 Jump & Glide Flair

| Feature | Description |
|---|---|
| **Silly Glider Options** | Instead of just wings: giant umbrella/parasol, cardboard airplane, bedsheet cape, inflatable duck, propeller beanie | 
| **Cloud Bounce** | Special bouncy clouds scattered in the sky — hit one while gliding for a bonus bounce upward |
| **Rainbow Rings** | Fly through rainbow rings while airborne for +100 pts and a nitro burst per ring |
| **Boing Sound** | Exaggerated cartoon spring "BOING!" on every jump — pitch scales with jump height |
| **Cartoon Rocket Boost** | When double-jumping, tiny cartoon rockets appear on the wheels, sputter, and pop off |
| **Feather Float** | After deploying the glider, giant feathers drift downward around the truck |
| **Sky Writing** | While gliding, the exhaust pipe writes "WHEEE" in smoke letters behind the truck |
| **Bird Friends** | A flock of cartoon birds flies alongside you while gliding, peeling off one by one as you descend |
| **Trampoline Zones** | Bouncy surfaces on the track that launch you extra high with a satisfying "BOING-OING-OING" |
| **Belly Flop Landing** | If you land completely flat from max height, special "BELLY FLOP!" scoring with a pancake squash animation |

### 13.5 Camera Effects

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

### 13.6 Wacky Track Mechanics

| Feature | Description |
|---|---|
| **Gravity Flip Zones** | Areas where gravity inverts — drive on ceiling! Everyone screams! |
| **Bouncy Mushrooms** | Giant cartoon mushrooms that launch player sky-high with a BOING |
| **Cannon Pipes** | Enter a warp pipe, get shot across the map in a cannon — you see your truck tumbling through the air |
| **Spring Loaded Bumper** | Pinball-style bumpers along walls — DING DING DING! as you bounce off everything |
| **Turbo Start** | Timed button press at race countdown for initial boost — mess it up and your engine stalls (comedy smoke) |
| **Drafting / Slipstream** | Speed bonus when following close behind another racer — visible wind tunnel effect |
| **Destruction Derby** | Cars shed parts on big hits — bumpers, doors, spoilers fly off (cosmetic only). By end of race trucks look hilariously wrecked |
| **Hamster Ball Mode** | Special zone encases your truck in a transparent hamster ball — bounces off everything |
| **Size-Change Tunnels** | Some tunnels shrink you, others make you GIANT — big truck crushes obstacles but can't fit through shortcuts |
| **Conveyor Belt Roads** | Road segments that slide you sideways or speed you up/slow you down |
| **Rotating Platforms** | Spinning disc platforms you have to drive across — centrifugal drift or fly off! |

---

## 14. Silly Celebrations, Taunts & Driver Reactions

### 14.1 Driver Character System

Every truck has a visible driver with exaggerated expressions. The driver is the comedy engine of the game — constantly reacting to everything:

```gdscript
enum DriverExpression {
    HAPPY,          # normal driving, slight smile
    GRINNING,       # drifting or boosting — ear-to-ear grin
    SCREAMING,      # big air, high speed — mouth wide open, arms up
    TERRIFIED,      # falling, about to crash — eyes huge, gripping wheel
    DIZZY,          # after 3+ spins — spiral eyes, tongue out
    ANGRY,          # got hit by item — fist shaking, red face
    LAUGHING,       # hit someone else with item — pointing and cackling
    CRYING,         # fell to last place — tears streaming sideways
    SLEEPING,       # idle for 5+ seconds — snoring with ZZZ bubbles
    CELEBRATING,    # finished a mega combo — arms in the air, party hat appears
    COOL,           # wearing sunglasses after a perfect landing
    SHOCKED,        # someone just passed you — jaw drops to the floor (literally stretches down)
}

var expression_timer: float = 0.0
var current_expression: DriverExpression = DriverExpression.HAPPY

func update_driver_expression():
    if airborne and linear_velocity.y > 5.0:
        set_expression(DriverExpression.SCREAMING, 999.0)  # until landing
    elif just_got_hit:
        set_expression(DriverExpression.ANGRY, 3.0)
    elif just_hit_someone:
        set_expression(DriverExpression.LAUGHING, 2.5)
    elif combo_count >= 3:
        set_expression(DriverExpression.CELEBRATING, 2.0)
    elif drifting:
        set_expression(DriverExpression.GRINNING, 0.5)
    elif position == last_place:
        set_expression(DriverExpression.CRYING, 999.0)
    elif idle_time > 5.0:
        set_expression(DriverExpression.SLEEPING, 999.0)
```

### 14.2 Taunt System

```gdscript
# Player can taunt with a dedicated button. Taunts are silly and harmless.
var taunt_list = [
    {"name": "Honk Honk",    "anim": "horn_squeeze",    "sound": "cartoon_honk"},
    {"name": "Raspberry",    "anim": "tongue_out",      "sound": "pbbbbbt"},
    {"name": "Victory Dance", "anim": "truck_shimmy",    "sound": "party_horn"},
    {"name": "Flex",         "anim": "suspension_bounce", "sound": "boing_boing"},
    {"name": "Clown Horn",   "anim": "horn_bulb",       "sound": "awooga"},
    {"name": "Spin Wheels",  "anim": "burnout",         "sound": "tire_screech"},
    {"name": "Bye Bye!",     "anim": "wave",            "sound": "cheerful_bye"},
]

# Special contextual taunts:
# - Taunt while passing someone: "SEE YA!" text bubble
# - Taunt after hitting someone with item: evil laugh + horns pop out of truck
# - Taunt in 1st place: truck does a wheelie
# - Taunt in last place: sad trombone plays
```

### 14.3 Victory & Podium Celebrations

```
PODIUM CELEBRATIONS:

  1st Place: Confetti cannon, fireworks, truck does donuts, driver crowd-surfs
             on tiny crowd NPCs, golden trophy descends from sky on a parachute

  2nd Place: Silver confetti, driver claps politely, truck bounces happily,
             a slightly smaller trophy appears

  3rd Place: Bronze confetti, driver does a thumbs up, truck does a little hop,
             trophy pops out of a jack-in-the-box

  Last Place: Sad trombone, truck deflates like a balloon (squash animation),
              driver pulls a paper bag over their head, a tiny rain cloud
              appears directly above the truck
```

### 14.4 Race Start Shenanigans

| Event | Description |
|---|---|
| **Countdown Revving** | All trucks rev engines during 3-2-1, exhaust smoke fills the start line |
| **False Start Penalty** | Go before "GO!" = engine stalls with comedy black smoke, 2-second delay |
| **Perfect Start** | Hit the gas at exactly "GO!" = turbo launch with rainbow trail |
| **Start Line Taunts** | Trucks bump each other playfully during countdown, drivers exchange funny looks |
| **Bird Flyover** | A V-formation of cartoon birds flies over the start line at "GO!" |
| **Announcer Intro** | "LADIES, GENTLEMEN, AND MONSTERS... START YOUR RIDICULOUS ENGINES!" |

---

## 15. Throwable Items & Wacky Weapons

### 15.1 Item Box System

```gdscript
# Floating "?" boxes scattered along the track
# Mario Kart rules: boxes respawn after 10 seconds
# Rubber-banding: players further behind get better items

enum ItemRarity { COMMON, UNCOMMON, RARE, LEGENDARY }

# Position-based probability table:
# 1st place: mostly defensive items (shield, banana)
# Middle:    balanced mix
# Last place: powerful offensive + catch-up items (golden mushroom, swap-a-roo, mega horn)

func get_item_from_box(player_position: int, total_racers: int) -> Item:
    var position_ratio = float(player_position) / float(total_racers)
    # 0.0 = 1st place, 1.0 = last place
    
    if position_ratio < 0.3:  # front of pack
        return pick_weighted([banana, shield, single_mushroom, stink_bomb])
    elif position_ratio < 0.7:  # middle
        return pick_weighted([pie, boxing_glove, rubber_ducky, triple_banana,
                              sticky_goo, mushroom, tickle_feather])
    else:  # back of pack (catch-up items!)
        return pick_weighted([golden_mushroom, giant_anvil, mega_horn,
                              swap_a_roo, chicken_curse, bubble_trap, giant_banana])
```

### 15.2 Complete Item Catalog

#### Throwable Forward (Aim at racers ahead)

| Item | Rarity | Effect | Visual | Sound |
|---|---|---|---|---|
| **Rubber Ducky** | Common | Bounces forward 3 times, bonks first car it hits sideways, victim spins 360° | Giant yellow duck with googly eyes, bounces with BOING | "QUACK!" on each bounce, "BONK!" on hit |
| **Pie Launcher** | Uncommon | Splats on victim's windshield/camera, obscures 60% of screen for 3 seconds | Cartoon cream pie, spiraling through air | Whistle → SPLAT! Victim hears gloopy sound |
| **Boxing Glove** | Uncommon | Spring-loaded glove punches target car sideways off course, brief stun | Red boxing glove on extending spring, cartoon style | SPROING! → POW! with impact stars |
| **Bubble Trap** | Rare | Encases opponent in a giant soap bubble, floats upward for 3 seconds then pops — drops them back on track | Iridescent rainbow bubble, victim visible inside, flailing | Bubbly sound → POP! with sparkles |
| **Giant Anvil** | Rare | Falls from sky onto the racer in 1st place — BONK! Squashes them flat for 2 seconds (squash animation, speed = 0) | ACME-style cartoon anvil with shadow growing on target | Whistling fall → CLANG! Screen shake for victim |
| **Mega Horn** | Legendary | Sonic blast pushes ALL nearby cars away in a radius, breaks shields | Giant brass horn appears on truck roof, visible shockwave ring | "AAAA-OOOOO-GA!" with bass boom |

#### Drop Behind (Leave traps for followers)

| Item | Rarity | Effect | Visual | Sound |
|---|---|---|---|---|
| **Banana Peel** | Common | Single banana. Drive over it = spin out (360° Y-axis spin, lose 1.5 seconds) | Classic yellow banana peel, glistening | "Wheee—WHOOPS!" tire screech + slip sound |
| **Triple Banana** | Uncommon | 3 bananas orbit your truck as shield, drop individually | Three bananas circling truck like moons | Orbiting hum, each drop = plop sound |
| **Stink Bomb** | Common | Green cloud persists on track for 8 seconds, anyone driving through it gets -40% speed for 3 seconds | Bubbling green cloud with cartoon flies and wavy stink lines | "PFF" deploy, victims hear "BLEUGH" with gagging |
| **Sticky Goo Bomb** | Uncommon | Purple sticky puddle on ground, cars that drive over it get -60% speed for 2 seconds + goo particles on wheels | Bubbling purple/pink goo puddle, stretchy strings when driving through | SPLORCH on deploy, SQUELCH when driven over |
| **Oil Slick** | Common | Rainbow-shimmer puddle, near-zero grip for 1.5 seconds — victim slides uncontrollably | Dark iridescent puddle, rainbow light reflections | Splash sound → sliding tire screech |
| **Giant Banana** | Rare | ENORMOUS banana blocks half the road — breaks into 3 regular bananas on hit | Comically oversized banana, as tall as a truck | Deep PLONK on deploy, CRACK when broken |

#### Targeting / Homing

| Item | Rarity | Effect | Visual | Sound |
|---|---|---|---|---|
| **Homing Pigeon** | Uncommon | A pigeon that locks onto next racer ahead, bonks them on the head. Can be outrun or dodged with sharp turns | Cartoon pigeon with angry eyebrows, flapping furiously | Accelerating flapping → "BONK" + bird tweet stars |
| **Tickle Feather** | Uncommon | Giant feather floats to nearest opponent, makes their truck wiggle uncontrollably left-right for 2.5 seconds (random steering inputs) | Giant pink feather, sparkle trail, giggly aura | Float sound → victim's truck giggles and honks randomly |
| **Swap-a-Roo** | Rare | INSTANTLY swap positions with the racer directly ahead of you — teleport swap with a poof of smoke | Lightning bolt connects both cars, POOF purple smoke | "BAZOING!" + teleport zap. Both drivers look shocked |
| **Chicken Curse** | Legendary | Target racer's truck transforms into a chicken truck for 5 seconds — slow, can't use items, clucks instead of honking | Truck morphs into a giant chicken with wheels, bobbing head | Magical poof → "BAWK BAWK BAWK BAWK!" constantly |

#### Self-Boost / Defensive

| Item | Rarity | Effect | Visual | Sound |
|---|---|---|---|---|
| **Speed Mushroom** | Common | Instant +50% speed burst for 2 seconds | Giant red mushroom with white spots appears on roof, then poofs | Mario-style "WHOMP" boost sound |
| **Golden Mushroom** | Rare | 3 speed boosts you can use within 8 seconds | Shiny golden mushroom, sparkle aura | More dramatic boost sound, ascending pitch per use |
| **Shield Bubble** | Common | Absorbs one hit from any item. If a projectile hits it, it reflects BACK at the attacker | Translucent force-field sphere around truck, slight shimmer | Hum while active, "DING" on absorb, "BOING" on reflect |
| **Ghost Mode** | Uncommon | Phase through obstacles AND other racers for 5 seconds, also steals an item from anyone you phase through | Truck becomes translucent ghost-white, floats slightly above ground | Spooky "wooooo" + sinister giggle on item steal |
| **Magnet Power** | Uncommon | Attracts all nearby collectibles (coins, nitro pickups) for 8 seconds + gives slight pull toward nearest racer ahead | Giant horseshoe magnet appears on truck front, attracts particles | CRT-TV magnetic buzz, items "ding" as they're absorbed |
| **Shrink Potion** | Rare | Drink to shrink YOUR truck to 40% size — you become TINY, much faster, can fit through secret shortcuts, but one hit sends you flying | Truck gulps from a bottle, shrinks with cartoon raspberry sound | Squeaky engine sound while small, "FWEEEE" deflating |
| **Grow Potion** | Rare | GROW to 200% size — you're HUGE, crush obstacles and shove other racers, but slow and can't fit in tunnels | Truck inflates like a balloon, oversized and wobbly | Deep rumbling engine, CRASH sounds on every obstacle |

### 15.3 Item Physics & Trajectories

```gdscript
# All thrown items are RigidBody3D with cartoon physics

func throw_item_forward(item: ItemProjectile):
    var throw_dir = -global_transform.basis.z + Vector3.UP * 0.15
    item.linear_velocity = throw_dir * (30.0 + current_speed * 0.5)
    item.gravity_scale = 0.4  # floaty cartoon arc
    item.angular_velocity = Vector3(0, 8, 0)  # comedic spin
    item.bounce_factor = 0.7  # rubber ducky bounces!

func drop_item_behind(item: ItemProjectile):
    item.global_position = global_position + global_transform.basis.z * 2.0
    item.linear_velocity = Vector3.ZERO  # just sits there menacingly

# Homing items use simplified steering:
func home_toward_target(delta):
    var to_target = (target.global_position - global_position).normalized()
    var steer = linear_velocity.normalized().lerp(to_target, homing_strength * delta)
    linear_velocity = steer * linear_velocity.length()
```

### 15.4 Getting Hit — Comedy Reactions

```gdscript
# When hit by any item:
func on_item_hit(item_type: String, from_racer: Node):
    match item_type:
        "rubber_ducky":
            # Spin 360°, brief stun, quacking sound
            angular_velocity.y = 12.0 * sign(randf() - 0.5)
            stun_timer = 0.8
        "pie":
            # Splat shader on camera, -20% speed for 3 seconds
            apply_screen_splat("pie_splat_texture", 3.0)
            speed_modifier = 0.8
        "boxing_glove":
            # Launch sideways, tumble
            apply_central_impulse(from_racer.global_transform.basis.x * 800.0)
            stun_timer = 1.2
        "bubble_trap":
            # Float upward in bubble
            gravity_scale = -0.5
            bubble_timer = 3.0
            disable_controls = true
        "giant_anvil":
            # Stop dead, squash flat
            linear_velocity = Vector3.ZERO
            chassis_mesh.scale.y = 0.2  # PANCAKE
            squash_timer = 2.0
            camera_shake(1.0, 0.5)
        "chicken_curse":
            # Transform into chicken
            swap_mesh_to_chicken()
            max_speed *= 0.4
            can_use_items = false
            chicken_timer = 5.0
    
    # Universal reaction:
    show_hit_popup(item_type)
    set_driver_expression(DriverExpression.ANGRY)
    from_racer.set_driver_expression(DriverExpression.LAUGHING)
```

---

## 16. Track Themes & Silly Environments

### 16.1 Theme Overview

Every track is a toybox fever dream. Each theme has unique surface types, obstacles, environmental hazards, ramp styles, and crowd NPCs. All environments lean into childish absurdity.

### Theme 1: "Candy Kingdom"

```
SETTING: A magical land made entirely of sweets

  ╭─────────────────────────────────────────────────────────╮
  │  🍫 Road:     Chocolate bar pavement (brown, glossy)    │
  │  🍭 Trees:    Giant lollipops + candy cane poles         │
  │  🧁 Buildings: Cupcake houses, wafer walls, cookie gates │
  │  ☁️  Sky:      Cotton candy clouds (pink, fluffy)        │
  │  🏔️  Mountains: Stacked ice cream scoops                │
  │  🌊 Water:    Caramel rivers + chocolate waterfalls      │
  ╰─────────────────────────────────────────────────────────╯

  SURFACES:
  - Chocolate Road:      friction 1.0, normal grip, brown tire marks
  - Caramel Sticky Zone: friction 0.4, -30% speed, gooey stretch particles
  - Ice Cream Slick:     friction 0.2, melted ice cream = slippery rainbow
  - Gummy Bear Bounce:   bouncy surface, cars launched upward on contact
  - Sprinkle Gravel:     friction 0.7, rainbow sprinkle particle spray

  HAZARDS:
  - Gummy bear spectators wobble onto the track randomly
  - Chocolate fountain geysers launch cars into the air
  - Jawbreaker boulders roll across the track
  - Licorice vines grab and slow your truck (break free by mashing jump)

  RAMPS:
  - Candy cane half-pipe (curved, striped red/white)
  - Stacked cookie ramp (crumbles visually on launch)
  - Cotton candy cloud launcher (extra air time, floaty gravity zone)

  SOUNDTRACK: Playful xylophone + glockenspiel, sugar-rush tempo
```

### Theme 2: "Dino Playground"

```
SETTING: Prehistoric jungle + volcano, like Jurassic Park as imagined by a 7-year-old

  ╭──────────────────────────────────────────────────────────╮
  │  🌿 Road:     Dirt paths through dense jungle             │
  │  🦕 Scenery:  Friendly cartoon dinosaurs roaming around   │
  │  🌋 Landmark: Active volcano in the center of the track   │
  │  🥚 Pickups:  Dino eggs that hatch into power-ups         │
  │  🦖 Boss:     T-Rex chases the last-place racer!          │
  │  🌴 Flora:    Oversized ferns, palm trees, carnivorous plants   │
  ╰──────────────────────────────────────────────────────────╯

  SURFACES:
  - Jungle Dirt:     friction 0.7, mud splash, brown dust particles
  - Volcanic Rock:   friction 1.0, hot glow, ember particles under tires
  - Tar Pit:         friction 0.15, ultra-sticky, black bubble particles
  - Vine Bridge:     friction 0.5, bouncy/wobbly, creaking wood sounds
  - Lava Flow:       friction 1.0, BUT deals damage — push you sideways if you linger

  HAZARDS:
  - Stampeding baby dinos run across the track in groups
  - Volcano erupts every 30 seconds — lava rocks rain down (dodge!)
  - Carnivorous plants snap at passing trucks (brief stun if caught)
  - Pterodactyl swoops down and steals your held item!
  - T-REX CHASE: On final lap, a giant T-Rex stomps behind the pack —
    if it catches the last-place racer, it EATS their truck (respawn with boost)

  RAMPS:
  - Dinosaur spine ramp (ride along a brontosaurus back)
  - Volcanic eruption launcher (geyser launch, fire particles)
  - Pterodactyl glider zone (automatic glider deploy + air current boost)

  SOUNDTRACK: Tribal drums + silly roaring sound effects, tempo increases on final lap
```

### Theme 3: "Toy Box Mayhem"

```
SETTING: A giant kid's bedroom, racing on a track made of toys

  ╭──────────────────────────────────────────────────────────╮
  │  🧱 Road:     LEGO brick road segments (bumpy, colorful)  │
  │  🏗️ Ramps:    Hot Wheels orange track loops and launchers  │
  │  🧸 Crowd:    Teddy bears, action figures, toy soldiers     │
  │  📏 Landmarks: Ruler bridges, pencil guardrails             │
  │  🎲 Obstacles: Scattered dice, marbles, toy cars            │
  │  🛏️ Scale:    Everything is HUGE — bed legs are skyscrapers │
  ╰──────────────────────────────────────────────────────────╯

  SURFACES:
  - LEGO Road:       friction 1.0, clickety-clack sound, stud bumps
  - Carpet Terrain:  friction 0.6, -20% speed, fluffy fiber particles
  - Hardwood Floor:  friction 0.8, squeaky tire sounds, reflection shader
  - Plastic Track:   friction 1.2, boost lanes, orange Hot Wheels classic
  - Pillow Fort Zone: bouncy, low grip, feather particles everywhere

  HAZARDS:
  - Marbles roll across the floor randomly (hit one = spin out)
  - RC helicopter buzzes overhead and drops items on the track
  - Toy soldier parade blocks the road (drive through them, they scatter comically)
  - Jack-in-the-box pops up from the track — launches you to the ceiling
  - A REAL KID'S HAND reaches down and rearranges track pieces every lap

  RAMPS:
  - Book stack staircase ramp (increasingly tall textbooks)
  - Hot Wheels loop-de-loop (360° orange track loop, classic!)
  - Toy catapult launcher (medieval toy catapult flings you across the room)
  - Pillow mountain (soft landing area, mega air time)

  SOUNDTRACK: Playful music box melody + toy sound effects, squeaky and clicky
```

### Theme 4: "Pirate Splash Bay"

```
SETTING: Caribbean-style pirate cove, beach racing with splashing and cannons

  ╭──────────────────────────────────────────────────────────╮
  │  🏖️ Road:     Wooden dock planks + sandy beach paths       │
  │  🏴‍☠️ Scenery:  Pirate ships, skull caves, treasure chests    │
  │  🐙 Boss:     Kraken tentacles whip across the track!       │
  │  💰 Pickups:  Gold coins scattered everywhere (bonus points) │
  │  🦜 NPCs:    Parrots repeat your taunt back at you          │
  │  💧 Water:    Splashable shallow water sections               │
  ╰──────────────────────────────────────────────────────────╯

  SURFACES:
  - Dock Planks:    friction 1.0, clattery wood sound, planks bounce
  - Wet Sand:       friction 0.6, splashy spray, tire tracks visible
  - Shallow Water:  friction 0.5, -25% speed, massive rooster tail splash
  - Seaweed Patch:  friction 0.3, slimy green, tangled particles
  - Treasure Road:  friction 1.0, gold coins fly up as you drive over (cha-ching!)

  HAZARDS:
  - Pirate ship cannons fire cannonballs across the track (dodge or get bonked!)
  - Kraken tentacles slam down onto the road — timing-based obstacle
  - Crab mobs scuttle across the beach and pinch your tires (slow for 1s)
  - Tidal waves wash across beach sections periodically (push you sideways)
  - Parrot thieves — colored parrots swoop down and steal your item (have to taunt to scare them off!)

  RAMPS:
  - Pirate ship gangplank launch (off the bow of a tilting ship)
  - Tidal wave ramp (ride the crest of a wave!)
  - Cannon launch (drive into a cannon, aim trajectory, fire!)

  SOUNDTRACK: Sea shanty ukulele + accordion, kraken attacks get dramatic orchestral
```

### Theme 5: "Space Junkyard"

```
SETTING: An orbital junkyard in outer space — low gravity and wacky physics

  ╭──────────────────────────────────────────────────────────╮
  │  🛸 Road:     Metal scrap platforms, magnetic roads         │
  │  🌑 Gravity:  Low-G zones (50% gravity, mega air time!)     │
  │  🪐 Scenery:  Asteroid chunks, broken satellites, space junk │
  │  👽 Crowd:    Alien spectators in small UFOs                 │
  │  🌌 Sky:      Starfield + nebulas + a goofy cartoon sun      │
  │  🕳️ Hazards:  Wormholes, asteroid showers, laser fences      │
  ╰──────────────────────────────────────────────────────────╯

  SURFACES:
  - Metal Plating:    friction 1.0, metallic clang sounds, spark particles
  - Magnetic Road:    friction 1.5, truck sticks to surface even upside-down!
  - Moon Dust:        friction 0.5, grey poof clouds, low gravity bounce
  - Solar Panel:      friction 0.8, glowing orange, gives nitro boost on contact
  - Zero-G Zone:      friction 0.0, truck floats — use thrusters/glider to navigate!

  HAZARDS:
  - Asteroid shower — rocks rain down with cartoon "INCOMING!" warning
  - Laser fences flicker on/off — time your pass or get zapped (spin + stun)
  - Wormholes — drive through one, pop out somewhere random on the track (SURPRISE!)
  - Space debris slowly drifts across the track — big chunks block the road
  - Alien tractor beam — a UFO tries to abduct you if you're in last place (actually a boost — it drops you ahead!)

  RAMPS:
  - Satellite dish launcher (curved parabola, massive air)
  - Asteroid hop chain (jump between floating rocks)
  - Wormhole cannon (enter one end, shoot out the other side of the map)

  SOUNDTRACK: Retro synth + theremin, wobbly space sounds, epic during zero-G sections
```

### Theme 6: "Haunted Funhouse"

```
SETTING: A spooky (but silly!) haunted carnival funhouse — more giggles than scares

  ╭──────────────────────────────────────────────────────────╮
  │  🎃 Road:     Creaky wooden funhouse floor, spinning discs  │
  │  👻 NPCs:     Friendly ghosts that photobomb the screen      │
  │  🪞 Shortcuts: Mirror maze with working reflections          │
  │  🎪 Scenery:  Jack-o-lanterns, bats, cobwebs, silly skeletons │
  │  🧛 Boss:     Vampire host announcer with dramatic voice      │
  │  💀 Aesthetic: Scooby-Doo meets Wario Ware                   │
  ╰──────────────────────────────────────────────────────────╯

  SURFACES:
  - Creaky Floorboards: friction 1.0, squeaky creak sounds, planks wobble
  - Spinning Disc:      friction 0.8, rotates your truck on contact (centrifuge!)
  - Spider Web:         friction 0.2, stretchy slow-down, silvery particles
  - Slime Floor:        friction 0.3, green goo, squelchy sounds
  - Ghost Rail:         friction 1.0, invisible track — trust the ghosts! (they're helpful)

  HAZARDS:
  - Jump scares! Silly pop-ups (nothing scary, but your DRIVER screams hilariously)
  - Mirror duplicates — your reflection drives out of a mirror as a ghost racer (harmless NPC)
  - Skeleton hands grab at your tires from the floor (brief slow-down)
  - Pumpkin head launcher — jack-o-lanterns roll across the road
  - THE FLOOR DISAPPEARS — sections of track go invisible for 3 seconds (they're still there, trust!)

  RAMPS:
  - Coffin catapult (a coffin lid slams open and launches you)
  - Bat wing glider zone (bats carry you across a gap)
  - Roller coaster section through the funhouse (on-rails, big drops!)

  SOUNDTRACK: Spooky organ + theremin, Danny Elfman vibes, silly "BOO!" stingers
```

### Theme 7: "Cloud Nine Skyway"

```
SETTING: Racing above the clouds on rainbow bridges and floating islands

  ╭──────────────────────────────────────────────────────────╮
  │  🌈 Road:     Rainbow bridges, cloud platforms, sky rails   │
  │  ☁️  Surface:  Bouncy clouds you can sink into and jump from │
  │  ⚡ Hazards:  Thunderstorm zones with lightning strikes      │
  │  ☀️  Boost:    Sunbeam lanes give permanent speed boost       │
  │  🌤️ Scenery:  Floating castles, sky whales, bird flocks      │
  │  🎵 Vibes:    Dreamy, magical, like driving through a lullaby │
  ╰──────────────────────────────────────────────────────────╯

  SURFACES:
  - Rainbow Road:    friction 1.0, prismatic shimmer particles, +5% speed
  - Cloud Platform:  friction 0.5, bouncy, sink down then spring up
  - Thundercloud:    friction 0.3, lightning zaps nearby cars randomly (stun!)
  - Sunbeam Lane:    friction 1.2, golden glow, sustained speed boost
  - Star Bridge:     friction 1.0, sparkling, crumbles behind you (last car falls!)

  HAZARDS:
  - Lightning bolts strike random positions on the track (1s warning flash)
  - Wind gusts push trucks sideways — visible wind streaks
  - Cloud gaps — holes in the cloud road, fall through = respawn with delay
  - Sky whales swim across the track — drive over their backs!
  - Tornado funnels suck trucks in and spit them out spinning

  RAMPS:
  - Rainbow arc ramp (massive air, prismatic trail behind you)
  - Cloud trampoline (extra bouncy, combo with stunts)
  - Wind updraft launch (invisible elevator, get pushed straight up)

  SOUNDTRACK: Ethereal harp + chimes + gentle synth, gets intense in thunderstorm zones
```

### Theme 8: "Farm Frenzy"

```
SETTING: A chaotic cartoon farm — tractors, mud, animals, and absolute mayhem

  ╭──────────────────────────────────────────────────────────╮
  │  🚜 Road:     Dirt paths, hay-lined, through fields + barns │
  │  🐄 Animals:  Cows, pigs, chickens as NPC obstacles          │
  │  🌽 Scenery:  Cornfields, silos, windmills, scarecrows       │
  │  🐷 Mud Pits: Pig pens with deep mud = ultra slow + funny    │
  │  🐮 Boss:     COW CATAPULT — a trebuchet that launches cows  │
  │  🥕 Pickups:  Vegetable items instead of boxes (carrot crate) │
  ╰──────────────────────────────────────────────────────────╯

  SURFACES:
  - Dirt Path:       friction 0.7, brown dust, crunchy sound
  - Deep Mud:        friction 0.3, -40% speed, brown splat particles, PIG NOISES
  - Hay Road:        friction 0.6, hay flies everywhere, sneezy sounds
  - Barn Floor:      friction 1.0, wood planks, barn door shortcuts
  - Crop Field:      friction 0.5, corn stalks break and fly everywhere (can't see!)

  HAZARDS:
  - Chickens scatter across the road in panic (harmless but LOUD)
  - Tractor NPC drives slowly on the main road — go around or CRASH
  - Cow catapult launches cows onto the track — dodge the incoming MOO
  - Pig stampede across a mud section (pushed sideways if hit)
  - Scarecrows fall over onto the road (destructible, slight slow-down)
  - Corn maze shortcut (faster but you can't see where you're going!)

  RAMPS:
  - Hay bale staircase ramp (bales collapse after you launch)
  - Silo jump (drive up the curved silo wall, launch off the top)
  - Cow catapult launch (optional — drive into the catapult for MEGA AIR)

  SOUNDTRACK: Banjo + fiddle + silly animal sounds, square dance energy
```

---

## 17. Track & Environment Systems

### 17.1 Track Design Elements

```
TRACK PIECES (modular — mix and match across any theme):
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
├── Final Lap Shortcut (opens on last lap only)
├── Trampoline Zones (BOING! extra height stunts)
├── Conveyor Belt Segments (push sideways or accelerate/decelerate)
├── Size-Change Tunnels (shrink or grow your truck!)
├── Rainbow Ring Gates (fly through for bonus points + nitro)
├── Cloud Bounce Platforms (soft bouncy platforms in the sky)
└── Cannon Launch Points (aim + fire across the map)
```

### 17.2 Checkpoint & Lap System

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

### 17.3 Respawn System

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

## 18. Algorithms Reference

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
| **Predictive Landing** | Ballistic trajectory simulation | O(n steps) | Simulates ~120 steps at 0.05s each, raycast per step, §10 |
| **Landing Reticle** | Decal projection at predicted impact | O(1) | Color-coded quality (green/yellow/orange/red), §10.3 |
| **Landing Quality** | Surface normal dot + zone detection | O(1) | PERFECT/GOOD/ROUGH/DANGER tiers, §10.2 |
| **Stunt Scoring** | Rotation accumulator per axis | O(1) | Points banked on clean landing, lost on bail, §11 |
| **Combo Multiplier** | Unique trick count → tier lookup | O(1) | x1.0 → x3.0 based on trick variety, §11.3 |
| **Score Milestones** | Threshold check on bank | O(n milestones) | Unlocks nitro, speed, extra jumps, §11.8 |
| **Item Roulette** | Position-weighted random selection | O(1) | Rubber-banding: last place gets best items, §15.1 |
| **Projectile Homing** | Lerp-based velocity steering | O(1) | `vel.lerp(to_target, strength * dt)`, §15.3 |
| **Projectile Physics** | RigidBody3D with cartoon gravity | O(1) | 0.4× gravity, high bounce factor, §15.3 |
| **Item Hit Reactions** | Per-item state modification | O(1) | Spin, stun, squash, bubble, chicken, §15.4 |
| **Driver Expressions** | Priority-based expression FSM | O(1) | 12 expression states, timer-based, §14.1 |
| **Drift Chain** | Consecutive corner detection | O(1) | +50% boost per chained corner, §13.2 |
| **Theme Surfaces** | Per-theme friction + particle table | O(1) | 8 themes × 5 surfaces each, §16 |

---

## 19. Production Roadmap

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
- [ ] **5.3** Implement expanded stunt detector — rotation accumulator per axis with barrel roll + corkscrew detection (§11.6)
- [ ] **5.4** Implement Skylanders-style stunt point system — base points per trick, pending until landing (§11.2, §11.5)
- [ ] **5.5** Add combo multiplier system — unique trick variety builds x1.0→x3.0 multiplier (§11.3)
- [ ] **5.6** Implement predictive landing system — ballistic trajectory simulation + ground reticle (§10.1, §10.3)
- [ ] **5.7** Add landing quality evaluation — PERFECT/GOOD/ROUGH/DANGER tiers with point banking (§10.2, §11.5)
- [ ] **5.8** Add landing zone designators on track after ramps — PERFECT landing targets (§10.5)
- [ ] **5.9** Implement stunt scoring HUD — pending points, combo multiplier, trick popups, banked score (§11.7)
- [ ] **5.10** Add score milestone rewards — nitro refills, speed bonuses, extra jumps at thresholds (§11.8)
- [ ] **5.11** Tune angular damping per trick state (0.5 during tricks, 6+ grounded) (§9.5)
- [ ] **5.12** Implement mid-air double jump (§12.1)
- [ ] **5.13** Implement glider deployment and retraction (§12.2)
- [ ] **5.14** Add glider aerodynamics (lift + drag + pitch/yaw control)
- [ ] **5.15** Implement ground pound / dive (§12.3)
- [ ] **5.16** Add optional trajectory trail visualization — dotted arc showing flight path (§10.4)
- [ ] **5.17** Implement silly stunt names system — escalating ridiculous names for combos (§13.3)
- [ ] **5.18** Add stunt confetti and crowd reaction system — visual/audio escalation per combo tier (§13.3)
- [ ] **5.19** Add announcer voice line triggers — "NICE!" → "IS THIS LEGAL?!" escalation (§13.3)
- [ ] **5.20** Implement drift chain detection — consecutive corner drifts with +50% multiplier per chain (§13.2)
- [ ] **5.21** Add drift flair VFX — rainbow tire marks, drift crown, smoke shapes (§13.2)
- [ ] **5.22** Add jump/glide flair — cloud bounces, rainbow rings, boing sounds, bird friends (§13.4)

**Deliverable:** Full Skylanders-style aerial trick & scoring system — predictive landing reticle, point-based stunt rewards with combo multipliers, risk/reward landing mechanics, drift chains, silly announcer, confetti, and childish joy everywhere.

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

### Phase 7 — Cartoon Visual Juice & Silly Features (Week 8–9)

```
Status: 🔴 Not Started
```

- [ ] **7.1** Implement squash & stretch on chassis mesh (§8.2)
- [ ] **7.2** Add wheel spin blur (mesh swap at high RPM)
- [ ] **7.3** Add exhaust backfire particles on deceleration — POP POP POP!
- [ ] **7.4** Add cartoon dust poof on sharp direction changes
- [ ] **7.5** Add screen shake system (impacts, boosts, explosions)
- [ ] **7.6** Add impact stars + tweety birds on wall collision (billboard sprite)
- [ ] **7.7** Add rainbow trail at max speed
- [ ] **7.8** Implement dynamic camera (speed FOV, drift offset, shake)
- [ ] **7.9** Implement driver expression system — 12 expressions, priority-based FSM (§14.1)
- [ ] **7.10** Add wobble/idle animation when truck is stopped
- [ ] **7.11** Add googly eye headlights with physics wobble (§13.1)
- [ ] **7.12** Add silly hat system — propeller beanie, crown, party hat with spring physics (§13.1)
- [ ] **7.13** Add tongue-out mesh during nitro boost (§13.1)
- [ ] **7.14** Add dizzy spiral eyes after 3+ consecutive spins (§13.1)
- [ ] **7.15** Implement taunt system — 7 taunts with contextual variants (§14.2)
- [ ] **7.16** Add victory/podium celebration animations — 1st/2nd/3rd/last place reactions (§14.3)
- [ ] **7.17** Add race start shenanigans — false start penalty, perfect start rainbow boost (§14.4)
- [ ] **7.18** Add confetti cannon on mega combos and race wins

**Deliverable:** The game FEELS like a Saturday morning cartoon. Every action has ridiculous, giggle-inducing feedback. Drivers scream, trucks wiggle, confetti flies.

---

### Phase 8 — Throwable Items & Wacky Weapons (Week 10–12)

```
Status: 🔴 Not Started
```

- [ ] **8.1** Create item box system (floating ? boxes on track, respawn after 10s) (§15.1)
- [ ] **8.2** Implement item inventory (hold one item at a time)
- [ ] **8.3** Implement position-based rubber-banding item roulette (§15.1)
- [ ] **8.4** Add Rubber Ducky — bouncing forward projectile, bonks + spins target (§15.2)
- [ ] **8.5** Add Pie Launcher — splats on victim's camera, obscures screen for 3s (§15.2)
- [ ] **8.6** Add Boxing Glove — spring-loaded punch, knocks car sideways (§15.2)
- [ ] **8.7** Add Banana Peel + Triple Banana — drop behind, orbiting shield (§15.2)
- [ ] **8.8** Add Stink Bomb — green cloud on track, slows anyone driving through (§15.2)
- [ ] **8.9** Add Sticky Goo Bomb — purple puddle, speed reduction (§15.2)
- [ ] **8.10** Add Shield Bubble — absorbs hit, reflects projectiles back at attacker (§15.2)
- [ ] **8.11** Add Speed Mushroom + Golden Mushroom — instant boost / 3 boosts (§15.2)
- [ ] **8.12** Add Homing Pigeon — angry bird locks onto next racer, bonks their head (§15.2)
- [ ] **8.13** Add Tickle Feather — makes opponent's truck wiggle uncontrollably (§15.2)
- [ ] **8.14** Add Bubble Trap — encases opponent in bubble, floats them upward for 3s (§15.2)
- [ ] **8.15** Add Giant Anvil — ACME anvil drops from sky onto 1st place (§15.2)
- [ ] **8.16** Add Mega Horn — sonic blast pushes ALL nearby cars away (§15.2)
- [ ] **8.17** Add Swap-a-Roo — teleport position swap with racer ahead (§15.2)
- [ ] **8.18** Add Chicken Curse — transforms opponent into a chicken truck for 5s (§15.2)
- [ ] **8.19** Add Ghost Mode — phase through everything, steal items from other racers (§15.2)
- [ ] **8.20** Add Shrink/Grow Potions — tiny speedy or giant crushing (§15.2)
- [ ] **8.21** Implement item hit reactions — per-item comedy animations and effects (§15.4)
- [ ] **8.22** Add Oil Slick + Giant Banana drop-behind traps (§15.2)
- [ ] **8.23** Balance item distribution table — test rubber-banding feel

**Deliverable:** Hilariously chaotic item system — rubber duckies bonking, pies splatting, chickens clucking, bubbles floating, anvils falling. Pure silly mayhem.

---

### Phase 9 — AI Opponents (Week 13–14)

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

### Phase 10 — Audio & Music (Week 15)

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

### Phase 11 — Track Themes & Environment Design (Week 15–18)

```
Status: 🔴 Not Started
```

- [ ] **11.1** Build "Candy Kingdom" track — chocolate road, gummy bear spectators, caramel rivers, candy cane ramps (§16)
- [ ] **11.2** Build "Dino Playground" track — jungle dirt, volcanic eruptions, T-Rex chase on final lap, pterodactyl glider zones (§16)
- [ ] **11.3** Build "Toy Box Mayhem" track — LEGO road, Hot Wheels loops, toy soldier parade, jack-in-the-box launchers, giant bedroom scale (§16)
- [ ] **11.4** Build "Pirate Splash Bay" track — dock planks, cannon fire, kraken tentacles, tidal wave ramps (§16)
- [ ] **11.5** Build "Space Junkyard" track — low-G zones, asteroid showers, wormhole shortcuts, magnetic roads (§16)
- [ ] **11.6** Build "Haunted Funhouse" track — creaky floors, jump scares, mirror maze, disappearing floor sections (§16)
- [ ] **11.7** Build "Cloud Nine Skyway" track — rainbow bridges, bouncy clouds, lightning hazards, sunbeam boost lanes (§16)
- [ ] **11.8** Build "Farm Frenzy" track — dirt paths, cow catapult, corn maze shortcut, tractor traffic, pig stampede (§16)
- [ ] **11.9** Implement per-theme surface types, friction tables, particle effects, and sound palettes
- [ ] **11.10** Add theme-specific crowd NPCs (gummy bears, toy soldiers, aliens, ghosts, etc.)
- [ ] **11.11** Add theme-specific environmental hazards with timing-based patterns
- [ ] **11.12** Add destructible scenery per theme (candy gates, LEGO walls, pirate barrels, etc.)
- [ ] **11.13** Implement checkpoint and lap counting system
- [ ] **11.14** Add respawn system with theme-appropriate animation
- [ ] **11.15** Add race countdown and finish sequence with theme-specific announcer lines
- [ ] **11.16** Add position tracking (1st, 2nd, 3rd... display)
- [ ] **11.17** Add post-race results screen with trick stats + silly superlatives ("Most Chickens Survived", "Best Belly Flop")

**Deliverable:** 8 wildly unique themed tracks, each a toybox fever dream with silly hazards, unique surfaces, and theme-specific chaos.

---

### Phase 12 — UI & Game Flow (Week 20)

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

### Phase 13 — Performance & Optimization (Week 21)

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

### Phase 14 — Playtesting & Tuning (Week 22–24)

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
| 5 | Air Controls & Scoring | Week 6–7 | Predictive landing, stunt points, combos, drift chains, glider |
| 6 | Nitro | Week 8 | Charge-and-burn boost system |
| 7 | Cartoon Juice & Silly Features | Week 9–10 | Driver expressions, taunts, celebrations, googly eyes, confetti |
| 8 | Wacky Weapons | Week 10–12 | 20+ silly throwable items, rubber duckies, pie launchers, chicken curses |
| 9 | AI | Week 13–14 | Fun AI opponents |
| 10 | Audio | Week 15 | Complete soundscape + announcer voice lines |
| 11 | Track Themes | Week 16–19 | 8 themed tracks (candy, dinos, toys, pirates, space, haunted, clouds, farm) |
| 12 | UI & Flow | Week 20 | Full game loop |
| 13 | Optimization | Week 21 | Stable 60 FPS |
| 14 | Polish | Week 22–24 | Tuned, tested, and SUPREMELY silly |

**Total estimated development time: ~24 weeks (solo developer)**

---

### Key Design Principles

1. **Fun > Realism** — Every mechanic should make the player smile. If physics says no but fun says yes, override the physics.
2. **Responsive Controls** — Zero input lag. The truck should respond instantly. Use force + direct velocity blending.
3. **Reward Skill** — Drifting, stunts, and boost management should reward skilled players with faster times. Mid-air tricks earn points that unlock tangible in-race rewards — Skylanders style.
4. **Rubber Banding** — Keep races close. Being in last place should feel recoverable, not hopeless. Last place gets the best items. A UFO might abduct you and drop you ahead. Hope is always alive.
5. **Juice Everything** — Every action needs visual and audio feedback. No silent, invisible mechanics. If a truck jumps, the driver SCREAMS. If a pie hits, it SPLATS on the camera. If you drift, RAINBOW SPARKS.
6. **Monster Truck Identity** — Big wheels, bouncy suspension, crushing terrain. This isn't a go-kart. It should feel HUGE.
7. **Childish Joy** — This is a game for the kid in everyone. Rubber duckies as weapons. Cow catapults. Trucks that turn into chickens. Googly eye headlights. The dumber, the funnier, the better.
8. **Every Track is a Toybox** — Each themed environment should feel like a different playset. Candy roads, space junk, pirate ships, haunted funhouses — the world is absurd and wonderful.
9. **Throwing Things is ALWAYS Funny** — Items should have maximum comedy impact. The victim's reaction matters as much as the thrower's advantage. A pie to the face should make both players laugh.
