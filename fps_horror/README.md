# FPS Horror (mobile)

A mobile-friendly rebuild of Bonkahe's procedural-animation FPS horror project
(github.com/Bonkahe/FPSHorrorGDScript, MIT — see `reference/LICENSE_original.txt`).
We reuse their 3D assets and their procedural limb (IK) idea; everything else is rebuilt small
and cheap, one step at a time. Godot 4.7, Mobile renderer.

## What came from the original

| Here | From their `CurrentProject` (= Episode 20) | Change |
|---|---|---|
| `assets/enemy/` | `Meshes/Enemies/Cultist.gltf` + `Textures/Enemies` | textures 4096 → 1024 px |
| `assets/player/` | `Meshes/Arms/*` (arms + revolver, 8 animations, bullet, shell, speed loader, flashlight) + `Textures/Arms`, `Textures/Revolver` | textures → 1024 px |
| `assets/env/` | `Meshes/Enviroment/*` + `Textures/Enviroment` + the `_albedo/_normal/_orm` level set | textures → 512 px |
| `assets/effects/` | `Meshes/Effects/*` + `Textures/Effects` | textures → 512 px |
| `reference/original_code/` | `Code/` (all their GDScript, ~2000 lines) | untouched, read-only, ignored by Godot |

`tools/copy_from_original.py` does the copy and can be re-run. Textures went from 540 MB to 57 MB
before Godot's own ETC2/ASTC compression.

**Left behind (what made it heavy):** the 67 MB occlusion bake, the 8 MB VoxelGI bake, the 27 MB
level scene, 137 MB of WAV audio, 4096 px textures, `SkeletonIK3D` (deprecated), and the scenes and
materials. We make our own materials, scenes and lighting.

## How their IK works (what we're porting)

- The enemy body is a **rolling RigidBody ball** (`BasicEnemyNavigationAgent`). The legs and arms
  don't carry it; they are placed around it and **kick it** each time a limb steps
  (`KickOffVelocity`): that impulse is what moves it, which is why it feels creepy and physical.
- `LimbPlacementController`: every `MinimumLimbStepDelay` seconds the next limb (hands and feet in
  turn) takes a step. A limb's target is found with a **raycast** from beside the body, down and
  out, pushed ahead by the body's velocity.
- `Limb`: the step travels along a **cubic Bezier arc** (origin → control points lifted along the
  surface normals → target). Off the ground (launched or falling), limbs **flail** with sin/cos.
- Each limb uses an IK chain (upper arm → hand, thigh → foot) with a magnet (pole) target. The
  chest bone is placed directly; the head turns toward the target.
- Enemies can **launch** (jump) between surfaces, so they climb walls and ceilings.

## Mobile rules for everything we build

- Mobile renderer, no SDFGI/VoxelGI/SSR/SSAO. Lighting: one shadowed light at most (the
  flashlight), otherwise baked (LightmapGI) or unshadowed.
- Textures ≤ 1024 px (characters) / ≤ 512 px (environment), compressed. Simple
  StandardMaterial3D (albedo + normal + ORM) — no subsurface scattering, no custom heavy shaders.
- Built-in C++ nodes over script loops: `TwoBoneIK3D` / `LookAtModifier3D` instead of solving IK in
  GDScript; NavigationAgent3D; physics queries only when needed (a limb raycasts when it steps,
  not every frame).
- Pool things that spawn (shells, impacts, decals, sounds). Audio as OGG.
- Enemies off-screen or far away update less often (or not at all).
- Small single-purpose files with documentation at the top.

## Steps

Each step ends with something you can run and look at.

| # | Step | What we build | Done when |
|---|---|---|---|
| 0 | **Setup** ✅ | This folder, the copied assets and code, `project.godot` (Mobile, Jolt, ETC2/ASTC) | assets import with no errors |
| 1 | **Asset viewer** ✅ | Cultist and arms with our own mobile materials, an orbit camera, play each arms animation, show triangle and draw-call counts | both models look right and cheap |
| 2 | **Test level and player** ✅ | A greybox level from a few env pieces; `CharacterBody3D` FPS movement (walk, look, jump, crouch), camera bob | you can walk around |
| 3 | **First-person arms** ✅ | Arms and revolver on the camera, an AnimationTree with idle low/high, raise/lower, fire, reload; weapon sway | arms react to aim and keys |
| 4 | **Shooting** ✅ | Hitscan raycast, ammo and reload, muzzle flash, pooled shell ejection and impact decals, recoil | shooting walls feels right |
| 5 ✅ | **Flashlight** | Spotlight with their cookie texture, the only shadowed light; lags behind the view | the level is dark and readable |
| 6 ✅ | **Enemy body** | Port the rolling-ball body: RigidBody sphere + NavigationAgent3D on a baked navmesh, steering toward the player, grounded check, launch impulse | a ball hunts you round the level |
| 7 ✅ | **Limb IK** (the core port) | Cultist skeleton on the ball: `TwoBoneIK3D` for 2 hands + 2 feet with pole targets, chest placement, `LookAtModifier3D` head; port the step cycle, raycast targets, Bezier step arcs, kick-off impulses, flailing in the air | the cultist crawls after you on hands and feet |
| 8 ✅ | **Enemy AI** | States: wander, chase, attack lunge, hurt, die; damage and hit reactions; cheap death (dissolve shader instead of their ragdoll) | it can kill you and you can kill it |
| 9 ⏭️ | ~~**Level and lighting**~~ | Skipped: no light baking. The test level stays, lit by live unshadowed bulbs; its cheap fixes moved to step 11 | — |
| 10 ⏭️ | ~~**Audio**~~ | WAV → OGG; pooled 3D sounds for footsteps, gun, enemy | it sounds like the original |
| 11 ⏭️ | ~~**Mobile**~~ | Touch controls (move stick, look drag, fire, reload, flashlight); enemy LOD (decimated Cultist, lower IK rate far away); lighter corridor walls (theirs are 15k triangles each) and visibility ranges; Android export and profiling | 60 fps on a phone |

### Step 1 notes

`make run` opens `demos/step01_viewer.tscn`. Materials live in `materials/` and are put on the
meshes by name in `src/look/character_look.gd` (their glTFs carry no materials). Costs as
measured: cultist 23.8k tris / 2 surfaces / 63 bones, arms + revolver 16.9k tris / 13 surfaces /
53 bones, props 3.9k tris. The cultist is the expensive one: with several on screen we'll want a
decimated version (~6k tris) in step 11. Normal maps are flagged as normal maps in their
`.import` files (better ETC2/ASTC quality).

### Step 2 notes

`make run` opens `demos/step02_walk.tscn`; `make shots` runs an autopilot through every move and
prints where the player ended up. Player: `scenes/player.tscn` with `src/player/player_body.gd`
(walk 4 m/s, sprint 6.5, crouch 2; jump; step-up 0.35 m; crouch keeps you down under low
ceilings), `player_look.gd` (mouse look), `camera_motion.gd` (bob, landing dip, smoothed steps,
sprint FOV). Input actions are created in code (`src/input/input_actions.gd`). The level
(`src/level/test_level.gd`) is boxes with box colliders, triplanar materials from their textures
(`src/look/env_look.gd`), their corridor wall meshes and rail cart, unshadowed lights only.

### Step 3 notes

`make run` opens `demos/step03_arms.tscn` (right mouse aim, left fire, R reload). The arms
(`src/player/fp_arms.gd`, on the player's Head) use the 8 clips inside `ArmsAnimations.gltf`; the
original's separate `.res` animation files don't load in 4.7, so they aren't used.
`src/player/arms_animator.gd` builds the same state machine as their `ArmsPrefab.tscn` in code
(idle low/high, raise/lower, fire, reload; reloading while aiming lowers the gun). The arms lag
the view slightly, sway in a figure-8 while walking, tip down while sprinting, never cast shadows.
Not ported yet: their hand IK on top of the animation (aim spread, flashlight hand pointing,
pulling the gun back from walls); revisit after step 7 once we know the IK cost.

### Step 4 notes

`make run` opens `demos/step04_shoot.tscn`. `src/weapon/revolver.gd`: 6 rounds, 0.38 s between
shots, a ray from the camera through the screen centre (spread 0.4° aimed / 2.5° hip, plus bloom
per shot), recoil that tips the view up and shakes the camera, pushes RigidBodies, emits `hit` for
damage later. Empty trigger auto-reloads; rounds refill when the reload clip ends. Effects, all
made once and reused: `muzzle_flash.gd` (two crossed flame cards from their 6-frame sheet, a glow,
a short unshadowed light, on a BoneAttachment at the barrel), `impacts.gd` (32 bullet-hole quads
that stick to what they hit, 6 dust/spark bursts), `shell_drop.gd` (12 pooled brass casings that
fall out during the reload). `src/ui/weapon_hud.gd`: crosshair dot and the six chambers. Bullet
hole and particle textures are gradients made in code.

### Step 5 notes

`make run` opens `demos/step05_flashlight.tscn` (F flashlight, L hall/mine lamps). The level
starts with the lamps off and almost no ambient light. `src/player/flashlight.gd` is a SpotLight3D
with their cookie texture, the only light that casts shadows (the shadow atlas is 2048 with one
quadrant, soft-shadow quality low on mobile, set in `project.godot`). It starts 0.5 m in front of
the camera, past the hands, because a light right next to the arms blew them out. The beam lags
behind the view (up to 10°) and flickers briefly when switched. With the lamps off the arms are a
dark silhouette; the muzzle flash still lights them for a frame.

### Step 6 notes

`make run` opens `demos/step06_enemy.tscn` (P route lines, L lamps). `scenes/enemy.tscn` is the
enemy's body only: a RigidBody ball (radius 0.83, mass 2, like theirs) and four limb points shown
as markers (pale hands, red feet). `src/enemy/`:
- `enemy_path.gd`: the route. Navmesh path; now and then a stretch is lifted onto a wall or the
  ceiling with a ray (their "up path"). The last waypoint always stays on the floor.
- `enemy_body.gd`: follows the route. Desired velocity, grounded state (floor, wall/ceiling, air),
  no gravity on walls, jump requests. Re-plans along the floor when it stops getting closer.
- `limb_gait.gd`: the step cycle. One limb every 0.12 s re-plants and kicks the ball; jumps are one
  big impulse after a 0.25 s crouch. Step 7 hangs the cultist's IK limbs on these points.
- `path_debug.gd`: route lines for testing.

`src/level/nav_bake.gd` bakes the navmesh when the level loads (only up to 4 m high, so wall and
roof tops don't become unreachable islands). Physics layers: 1 world, 2 player, 4 enemy.

Changes from the original, each found while testing:
- Limbs also lead by the desired velocity, and a ball that wants to move but is standing still
  steps anyway. Theirs only started moving because IK error made the limbs re-step now and then.
- Getting stuck is measured by progress. Their per-waypoint timer was reset every frame and never
  fired.
- Speed is capped at 25 m/s, and the wall press can't build up speed.
- The grounded query is built once, not every tick.
- The test level's mine floor now runs under the doorway (there was a 1 m hole that split the
  navmesh).

### Step 7 notes

`make run` opens `demos/step07_cultist.tscn` (B shows the ball). `scenes/cultist.tscn` is step 6's
enemy plus a `Rig` node, with the limb markers off. `src/enemy/`:
- `limb.gd`: one hand or foot point. Steps follow a cubic Bezier (their control-point rules: lift
  along the surface normal, arc round the body). The point stays on its own side of the torso
  and flails in the air.
- `torso.gd`: the body frame. It follows the ball, dips while limbs are mid-step, faces the way it
  moves, and its up is the surface it's on.
- `limb_gait.gd`: now drives four `limb.gd` points (arcs instead of jumps) and flailing.
- `cultist_rig.gd`: the model takes the torso frame, with the chest (root bone) laid face down.
  - Limbs use one `TwoBoneIK3D` with 4 settings (targets just above the points; poles from their
    magnet offsets).
  - The head uses a `LookAtModifier3D` on the neck, aimed at the player, eased, limited to 70°.
  - The ball is hidden.

Checked against theirs afterwards and matched:
- **Re-step:** a hand or foot the IK can't get within 0.2 m of its point steps again from where it
  really is (`cultist_rig.gd` reads the solved positions back into `limb.tip`).
- **Hands and feet:** `end_bone_aim.gd` turns them like their `override_tip_basis`. Palms and soles
  lie flat on the surface facing away from the body; in the air they point along the bend.
- **Head:** like their 2-bone head IK, the neck aims at a point 1.66 m ahead that swings toward you
  at 2/s, the head takes its rotation, and both fade out as you go behind it.
- **Small fixes:** kicks come from the lifted IK point and only off a surface, and the side vector
  is no longer normalised.

The original used deprecated `SkeletonIK3D` (FABRIK, one node per limb). `TwoBoneIK3D` solves
each limb directly, which is cheaper. It works on walls and the ceiling with no extra code,
because the torso's up is the surface normal.

### Step 8 notes

`make run` opens `demos/step08_ai.tscn`. `scenes/cultist.tscn` now has the AI on its root.

- `src/enemy/enemy_ai.gd` cycles through wander, chase, attack, retreat and dead.
  - **Wander:** walks to random spots near home, pausing between them.
  - **Chase:** re-plans to you every 3 m you move.
  - **Attack:** within 6 m and with its timer ready, it lunges (`limb_gait.attack()`). Within 2 m it
    hits you (`take_hit`).
  - **Retreat:** after a hit it bounces off you, backs away 8 m, then chases again.
  - **Noticing you:** being shot (like theirs), plus sight (an 18 m cone, or 5 m in any direction,
    one ray every 0.2 s) and hearing gunshots within 30 m.
  - **Health:** it takes 3 shots (theirs died to one), and each hit makes it stagger.
- **Death:** the limbs drop and the ball rolls to rest; a body on the ceiling falls. Then
  `dissolve.gd` swaps in `materials/cultist_dissolve.gdshader`, where noise holes with glowing blue
  edges eat through the body over 2.5 s while blue sparks rise, and the enemy is freed. This
  replaces their ragdoll: no physics bones. The shader only runs while a body dissolves.
- **Player:** `src/player/player_health.gd` gives you 3 hits. Each hit shoves you and shakes the
  camera, and you heal after 10 s without a hit. `src/ui/damage_overlay.gd` draws their vein
  texture as a red overlay that grows as you get hurt. When you die, the demo respawns you after
  3 s.
- **Hits:** anything with `take_hit(point, force, attacker)` can be hurt: the revolver calls it on
  what it hits, and the enemy calls it on you.

Step 7 is the heart of it; steps 1–6 set up what it needs (the skeleton and materials, a level
to walk on, a navmesh, a body to drive). Each step gets its own small files and a demo scene, so
we can test them one at a time.
