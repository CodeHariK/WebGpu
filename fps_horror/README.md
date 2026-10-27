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
| 3 | **First-person arms** | Arms and revolver on the camera, an AnimationTree with idle low/high, raise/lower, fire, reload; weapon sway | arms react to aim and keys |
| 4 | **Shooting** | Hitscan raycast, ammo and reload, muzzle flash, pooled shell ejection and impact decals, recoil | shooting walls feels right |
| 5 | **Flashlight** | Spotlight with their cookie texture, the only shadowed light; lags behind the view | the level is dark and readable |
| 6 | **Enemy body** | Port the rolling-ball body: RigidBody sphere + NavigationAgent3D on a baked navmesh, steering toward the player, grounded check, launch impulse | a ball hunts you round the level |
| 7 | **Limb IK** (the core port) | Cultist skeleton on the ball: `TwoBoneIK3D` for 2 hands + 2 feet with pole targets, chest placement, `LookAtModifier3D` head; port the step cycle, raycast targets, Bezier step arcs, kick-off impulses, flailing in the air | the cultist crawls after you on hands and feet |
| 8 | **Enemy AI** | States: wander, chase, attack lunge, hurt, die; damage and hit reactions; cheap death (dissolve shader instead of their ragdoll) | it can kill you and you can kill it |
| 9 | **Level and lighting** | Build the mine from the env pieces; LightmapGI bake, fog, visibility ranges | the full level runs smoothly |
| 10 | **Audio** | WAV → OGG; pooled 3D sounds for footsteps, gun, enemy | it sounds like the original |
| 11 | **Mobile** | Touch controls (move stick, look drag, fire, reload, flashlight); enemy LOD (decimated Cultist, lower IK rate far away); Android export and profiling | 60 fps on a phone |

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

Step 7 is the heart of it; steps 1–6 set up what it needs (the skeleton and materials, a level
to walk on, a navmesh, a body to drive). Each step gets its own small files and a demo scene, so
we can test them one at a time.
