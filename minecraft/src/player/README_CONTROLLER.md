# 3D Platformer Controller (Odyssey Style)

## The Goal
To create a high-fidelity, snappy 3D platformer controller inspired by **Super Mario Odyssey** and **A Hat in Time**. 

## Why CharacterBody3D?
We are using `CharacterBody3D` (Kinematic) instead of `RigidBody3D` (Dynamic) because:
1. **Total Control**: 3D platforming relies on subtle "lies" in physics (instant acceleration, steering in mid-air, snapping to ledges).
2. **Reliability**: CharacterBody3D doesn't bounce off walls or slide down slopes unless we explicitly tell it to.
3. **Manual Physics**: By setting global gravity/damping to 0 and handling them in code, we achieve the "snappy" feel where the character stops and starts exactly when the player expects.

---

## Supported Mechanics & TODO List

### Grounded Movement
- [x] **Basic Run**: Camera-relative movement with smooth acceleration.
- [ ] **Skid/Turn Around**: If the player reverses direction quickly, the character skids and loses speed before turning.
- [ ] **Dashing**: A forward burst of speed (similar to Hat in Time's dive or Mario's roll).

### Jumping & Air Logic
- [x] **Variable Height Jump**: Jump height depends on how long the button is held.
- [ ] **Multiple Jumps**:
    - [ ] **Double Jump**: Second jump in mid-air.
    - [ ] **Triple Jump**: Higher jumps if timed correctly upon landing.
- [x] **Air Drift/Control**: Refined control in the air to allow for precise landings on 3D platforms.
- [ ] **Quickfall (Ground Pound)**: Pressing an input in mid-air to slam downward.

### Special Actions
- [ ] **Dropkick**: A high-velocity forward/downward attack with a specific arc.
- [ ] **Ledge Grab**: Automatically grabbing and hanging from edges.
- [ ] **Wall Jump**: Snapping to walls and jumping off them.

### State Machine Architecture
The controller is built using a **Hierarchical State Machine (HSM)**:
- `GroundedState`: Parent for Idle, Move, Dash.
- `AirborneState`: Parent for Jump, Fall, Multiple Jump, Quickfall.
- `ActionState`: Special states like Dropkick or Ledge Grab.

---

## Technical Details
- **Acceleration**: Uses `Math::move_toward` for linear ramp-up.
- **Gravity**: Calculated manually using `jump_height` and `time_to_peak` to allow for "floaty" jump peaks and "heavy" falls.
- **Rotation**: Uses `Math::lerp_angle` to ensure the character smoothly faces the direction of travel.
