In Godot 4.3+, Godot introduced **`SkeletonModifier3D`** specifically for real-time procedural bone animations, IK, and secondary physics (jiggle / spring-damper chains).

---

### Why GDExtension C++ is Perfect for Wiggle Physics

| Factor | GDScript | GDExtension (C++) |
| :--- | :--- | :--- |
| **Performance** | Overhead when iterating over 50–200+ bones per frame across multiple characters. | **15–30× faster**. Native SIMD math (quaternions, Verlet/XPBD integration, spring solvers). |
| **Lifecycle Hook** | Must manually hook `_process` or `_physics_process` before animation tree renders. | Integrates directly into the engine's animation pipeline via **`SkeletonModifier3D::_process_modification()`**. |
| **Cache Locality** | Heap-allocated objects and variants. | Contiguous array of particle structs `struct BoneParticle { Vector3 pos, vel, prev_pos; Quaternion rest_rot; };`. |
| **Editor Gizmos** | Limited interactive gizmo tooling. | Native Godot Editor plugins with real-time spring tension visualization & collision spheres in the 3D viewport. |

---

### Core Architecture for a Godot C++ Wiggle GDExtension

```
Skeleton3D
 └── WiggleModifier3D (extends SkeletonModifier3D)
      ├── Properties: Stiffness, Damping, Gravity, Inertia, Wind
      ├── Collision: Sphere / Capsule colliders (hitbox/environment avoidance)
      └── Chains: Auto-detects bone hierarchies (e.g. antennas, tails, armor flaps, cables)
```

#### 1. Engine Integration (`SkeletonModifier3D`)
`SkeletonModifier3D` runs after `AnimationPlayer` / `AnimationTree` evaluate rest and animated poses, but before the mesh skinning transform is uploaded to the GPU:

```cpp
class WiggleModifier3D : public SkeletonModifier3D {
    GDCLASS(WiggleModifier3D, SkeletonModifier3D);

protected:
    static void _bind_methods();

    virtual void _process_modification() override {
        Skeleton3D *skel = get_skeleton();
        if (!skel) return;

        double delta = get_process_delta_time();
        // 1. Read current animated bone pose (target position)
        // 2. Step spring-damper / XPBD physics integration
        // 3. Solve distance & angular limits (twist/bend clamps)
        // 4. Solve collider penetration (spheres / capsules)
        // 5. Write back to skel->set_bone_global_pose_override(...) or local pose
    }
};
```

#### 2. Physics Model Options

1. **Spring-Damper (Hooke's Law + Velocity Damping)**:
   $$F = -k \cdot (x - x_{\text{target}}) - c \cdot v + m \cdot g$$
   - Great for antennas, mechanical joints, shock absorbers, stiff plates.
2. **XPBD / Verlet Particles (Position-Based Dynamics)**:
   - Extremely stable even at low framerates ($30\text{ fps}$ or frame drops).
   - Incompressible length constraints prevent mesh stretching.
   - Great for cables, wires, hanging ropes, hair, and soft body parts.

---

### Key Features to Include

1. **Hierarchy Propagation (Inertial Chains)**:
   - Movement from the parent mech body transfers momentum down the bone chain with natural delay and whipping effect.
2. **Axis Locking & Mechanical Limits**:
   - For mechs and hard-surface robotics, restrict jiggle to specific local axes (e.g. hinge rotation only along local $X$, or linear compression on a hydraulic shock).
3. **Collision Avoidance**:
   - Capsule and Sphere colliders attached to other bones (e.g. prevent hanging cables or armor plates from clipping inside the chassis).
4. **Wind / External Forces**:
   - Connects to Godot's world wind or impulse vectors (e.g., explosions or thruster backwash).
5. **Baked Animation Export**:
   - In-editor tool to bake simulated wiggle physics into standard Godot `Animation` tracks for static runtime playback.

---

### Next Steps

If you'd like to pursue this, we can:
1. **Set up the `godot-cpp` GDExtension project structure** (SConstruct / CMake, entry point, `register_types`).
2. **Implement the `WiggleModifier3D` class** using Verlet / Spring dynamics.
3. **Create editor inspector properties** (stiffness, damping, gravity, chain selection).
4.
