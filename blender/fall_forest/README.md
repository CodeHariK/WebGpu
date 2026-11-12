# Fall Forest

A fall asset pack (45 low-poly meshes and one texture) checked for how cheap it is, put on a
512 m Godot terrain about 20 000 times, and a grass system for the same terrain, with the ways to
keep them fast switched on and off from a debug panel. Mobile renderer, like the game. The props
and the grass are both on; the debug panel switches each off (`props`, `near grass`, `far grass`).

```
make run       # fly round it (right mouse: look, WASD, Q / E, Shift)
make capture   # time two views with each speed-up off in turn, shots/ in godot_demo/
make blend     # source/AllFallAssets.fbx + FallTex.png -> source/FallAssetsNature.blend
make assets    # source/FallAssetsNature.blend -> godot_demo/assets/fall_assets.glb
make edit      # the Godot editor
```

## Folders

| Folder | What it holds |
|---|---|
| `source/` | the pack: `AllFallAssets.fbx` (all 45 meshes), `FallTex.png`; `FallAssetsNature.blend` (every mesh textured, in metres, laid out in rows, marked as assets) and the two Blender scripts that make it and export it |
| `godot_demo/assets/` | `fall_assets.glb` (the meshes, each at its base pivot, no materials), `FallTex.png`, the materials |
| `godot_demo/terrain/` | `terrain_height.gd` (the ground's shape and the woods, from noise), `terrain_chunk.gd` (one chunk: near and far mesh, skirts, occluder), `terrain.gd` (the grid of chunks) |
| `godot_demo/props/` | `prop_kinds.gd` (what grows where, how many, how far it is drawn), `prop_scatter.gd` (placing them, the batches), `tree_billboards.gd` + `tree_billboard.gdshader` (the far trees' flat stand-ins) |
| `godot_demo/grass/` | the grass (below): `grass_field.gd` (the two rings, the pusher), `grass_sliders.gd` (its sliders in the debug panel), `grass_layer.gd` (one ring of tiles following the camera), `grass_meshes.gd` (blade, tuft), `grass_maps.gd` (the terrain as textures), `grass_near.gdshader`, `grass_far.gdshader` |
| `godot_demo/camera/`, `debug/` | the fly camera (mouse and keys, or touch: left half moves, right half looks); the debug panel (stats and switches), `capture.gd` (the timed runs) |

## The pack: is it cheap enough?

| | |
|---|---|
| Triangles | trees 1 500 – 3 300, bushes ~730, pumpkins and gourds 330 – 530, mushrooms 230 – 1 600, pinecones 830 – 1 060, acorns 430, leaves 30 – 130; 28 700 for all 45 |
| Texture | one 64 x 64 PNG (6 KB): strips of colour gradients; every mesh's UVs pick their colours from it (UV-mapped palette) |
| Vertex colours | none: the colour comes from the texture, through the UVs |
| Materials | one for all 45 meshes |

Low poly enough? For a few on screen, yes. For thousands, the trees and the small round things
are the heavy part: a 30 cm pinecone has 1 000 triangles, an acorn 430, and the fourth round
mushroom 1 600, more than a bush. Godot's mesh LOD (made when it imports the glb) takes care of
that at a distance: switched off, the same view draws 5 x the triangles (below). Up close, if the
mobile budget is tight, those few meshes are the ones to simplify (Blender: Decimate to about a
third).

The texture is about as cheap as texturing gets: 6 KB, one texture and one material for the whole
pack, so every batch of every kind keeps the same GPU state, and a colour change is a UV move or a
repaint of a strip. Vertex colours would save the one texture read but need split vertices wherever
two colours meet; at 64 x 64 the read is next to free. Keep it. (Godot imports it lossless with
mipmaps: compressed, the gradient strips would band.)

## The forest

`TerrainHeight` gives one height and one "how wooded" value for any (x, z) from noise; the terrain
and the props both ask it, so props sit exactly on the ground. `PropKinds` lists what grows where:

| Kind | Count | Drawn to | Shadow | Grows |
|---|---|---|---|---|
| trees | 1 500 | 400 m | yes | in the woods |
| bushes | 1 500 | 160 m | yes | at the woods' edges |
| pumpkins and gourds | 800 | 90 m | yes | in clearings |
| mushrooms (20 meshes) | 5 000 | 60 m | no | in the woods |
| acorns and pinecones | 3 000 | 30 m | no | in the woods |
| leaves | 8 000 | 35 m | no | anywhere, more in the woods |

## Keeping it fast

| Technique | How it is done here |
|---|---|
| Batching (MultiMesh) | every copy of one mesh in one prop chunk is one `MultiMeshInstance3D`: one draw call for all of them. 19 800 props are 2 780 batches, of which a view draws about 250 |
| Chunking | the terrain is 16 x 16 chunks of 32 m, the props 8 x 8 chunks of 64 m. Each chunk and batch has its own bounding box, so whole chunks are skipped outside the view (frustum culling, automatic), past their range, or behind a hill |
| Visibility ranges | each prop kind stops being drawn past its distance (table above); the depth fog (150 – 420 m) hides where things stop |
| Terrain LOD | each chunk has a near mesh (a vertex a metre) and a far mesh (every 4 m: 1/16 of the triangles); the far mesh's range starts at 96 m and the near mesh has it as its visibility parent (Godot's HLOD), so exactly one of them is drawn. Skirts (the chunk edges pulled 3 m down) hide the gaps where a fine chunk meets a coarse one |
| Mesh LOD | Godot makes simpler versions of every mesh when it imports the glb and picks one per batch by its size on screen (`mesh_lod_threshold`, 1 pixel) |
| Occlusion culling | each terrain chunk has an `OccluderInstance3D`: its coarse ground sunk 0.5 m (so a prop standing on it is never hidden by it). The renderer draws the occluders into a small depth buffer on the CPU and skips every chunk and batch fully behind them (Project Settings > Rendering > Occlusion Culling) |
| Shadows | only trees, bushes and pumpkins cast them, only to 80 m (2 splits); the ground casts none |

Measured with `make capture` with the props on (before the grass): 1600 x 900, mobile renderer,
Apple M2, 120 frames drawn back to back (this is the CPU and the GPU together; draw calls and
triangles are the frame's own counts).

| View | Case | ms / frame | Draw calls | Objects | M triangles |
|---|---|---|---|---|---|
| ground | all on | 2.02 | 240 | 441 | 0.34 |
| ground | no occlusion culling | 2.92 | 435 | 636 | 0.52 |
| ground | no visibility ranges | 3.32 | 721 | 922 | 1.15 |
| ground | no mesh LOD | 2.13 | 240 | 441 | 1.81 |
| ground | no shadows | 1.57 | 174 | 375 | 0.21 |
| ground | all off | 8.01 | 1 886 | 2 089 | 6.81 |
| overview | all on | 2.83 | 475 | 676 | 0.43 |
| overview | no occlusion culling | 2.83 | 475 | 676 | 0.43 |
| overview | no visibility ranges | 8.92 | 2 871 | 3 074 | 4.04 |
| overview | no mesh LOD | 3.05 | 455 | 656 | 2.60 |
| overview | no shadows | 2.32 | 418 | 619 | 0.35 |
| overview | all off | 11.58 | 2 825 | 3 029 | 10.19 |

- Visibility ranges save the most (3 x from above): most props are small, and small things far
  away aren't worth a draw call.
- Occlusion culling saves where hills hide things (on the ground: 45 % of the draw calls, 30 % of
  the time) and nothing from above, where nothing is hidden; it costs a little CPU every frame.
- Mesh LOD cuts the triangles 5 – 6 x; on the M2 that is little time, on a phone GPU it is more.
- Shadows cost about a quarter of the frame: the shadow pass draws the casters again.
- Together: 4 x faster on the ground, 4 x from above.

What would go further: fewer mesh variants per chunk (each variant is a batch: 45 meshes x 64
chunks), merging each chunk's small props into one mesh, impostors (a flat picture) for far trees,
and baking the hills' shadows into the ground's vertex colours.

## Grass

Grass covers the whole terrain but is only ever drawn round the camera, in two rings, and past
them the ground's own colour is the grass:

| Ring | Shape | Density | Distance | Does |
|---|---|---|---|---|
| near | a blade: 7 vertices, 5 triangles, 3 rows so it can curve | 28 a square metre | 0 – 26 m | wind (gusts and flutter), bends away from the pusher, full colour work, light through the blade |
| far | a tuft: 3 single-triangle blades, 9 vertices, shaded by vertex colours | 3 a square metre | 21 – 80 m | one sway a tuft, colour from the mask and its vertex colours |
| past 80 m | none: the terrain's vertex colours are the grass's colours | | | |

The debug panel has sliders for the grass (`grass/grass_sliders.gd`): near and far density
(blades / tufts a square metre; the tiles' shared pattern is rebuilt, so it changes at once),
wind strength and speed, colour variation (0: every blade alike .. 3) and dryness (-1 all green
.. 1 all dry, on top of the map's; the ground's own colour doesn't follow it).

How it is kept cheap:

| Technique | Here |
|---|---|
| Instancing | every tile is one `MultiMeshInstance3D`: one draw call for its 1 800 blades (or 770 tufts) |
| One pattern, many tiles | a ring's tiles all share one MultiMesh (the roots of one tile, jittered on a grid so there are no clumps or gaps); tiles are only moved, never rebuilt, when the camera crosses into another tile: no per-frame CPU work |
| The GPU does every blade | the root's position is all a blade has; the vertex shader reads the height, density and dryness from two textures (`GrassMaps`: the terrain baked to a texel a metre at start) and takes size, turn and shade from a hash of the root's world position, so the shared pattern doesn't look repeated |
| Culling | each tile gets a bounding box from the ground under it, so tiles outside the view or behind a hill (the terrain's occluders) are skipped |
| Density from a mask | a blade whose random number is over the mask's density (thin on slopes, patchy) folds to nothing |
| Fading by size | blades shrink to nothing over the last 5 m of their ring, and the far tufts grow in as they go: no pop, no alpha |
| No shadows cast | grass receives shadows but casts none (a shadow pass over 100 000 blades); the dark root fakes the shade inside the grass |
| Fewer, simpler things far away | the far ring has a tenth of the triangles a square metre, no pusher, one sway a tuft; past it, none |

Measured (`make capture`, 1600 x 900, mobile renderer, M2): the eye-height view draws in 1.9 ms
with the grass and 0.8 ms without it; the near ring costs about 0.5 ms, the far ring 0.7 ms. The
far ring costs more than the near one although it is a quarter as dense: its area grows with the
square of its radius (80 m: 18 000 square metres against 2 000), and a tuft that has shrunk to
nothing is still sent through the vertex shader. Density has to fall off faster than the ring
grows: the far density and far_end (`GrassField`) are the dials, then the ground's colour.

### Near and far grass: why two kinds

Most of the screen's grass is far away, but up close a blade is all you see. So games draw the
detail where it is seen: near blades with several rows (they can curve and bend), wind,
interaction and colour work in the shader; farther, fewer and simpler pieces; far away, none, the
terrain painted the grass's colour (and in some games a cheap tuft or card in between). The
switch has to be hidden: the near blades shrink as the far ones grow, over the same few metres,
with the same colours from the same mask, so the eye sees one field.

### Alpha: why none

Grass drawn as textured cards with alpha (a picture of grass on a quad, the old way) costs more
than it saves on phones. Alpha test (`discard`, Godot's alpha scissor) stops the GPU from skipping
hidden pixels early: on the tile-based GPUs phones use (Apple, Mali, PowerVR) every layer of cards
is shaded, and grass is many layers deep. Alpha blending is worse (sorted, no depth write). A
blade made of triangles is opaque: its outline is its shape, the depth test throws away whatever
is behind it, and 5 triangles cost less than the overdraw of a card. So both rings here are
opaque triangles, and they fade by shrinking, not by alpha.

### Colour variation

From big to small, each layer on top of the last:
- by place: the mask's dryness (green in the clearings, orange where the woods are), the same
  value the terrain's colours use, so grass and ground agree
- by blade: a random shade and a nudge toward dry or green (from the root's hash)
- along the blade: root colour to tip colour, darker at the root (fake ambient occlusion: the
  inside of a field is shadowed by the field)
- by light: the sun shining through from behind lights the blade (`BACKLIGHT`), and the blade's
  normal points mostly up, so the field lights softly as one surface instead of flickering
  between thousands of edge-on cards

### Interaction

Here: the near blades lean away from the pusher (a ball kept 5 m in front of the camera,
standing in for the player) and flatten under it, in the vertex shader, from one `vec4`
(position, radius). Several pushers are an array of them. Grass that stays trampled after the
player has passed needs memory: a small texture following the player (a trail map) that
footsteps paint into and that slowly fades back; the blades read their bend from it. That is the
next step if it is wanted.

### What other games do

- Ghost of Tsushima (Sucker Punch, "Procedural Grass in Ghost of Tsushima", GDC 2021): blades
  made each frame on the GPU (compute shader) for the tiles round the camera, each blade a curve
  with a high and a low detail version, their height, width, bend and clumping from textures,
  wind from a scrolling noise, and blades seen edge-on turned a little toward the camera so the
  field never looks thin. The same ideas as here, at much larger counts.
- Open-world games generally: dense blades only near, fewer and simpler farther, the terrain's
  colour beyond; grass placed and shaded from density and type maps on the GPU, not one by one.
- Older and many mobile games: textured cards with alpha test, few and big; cheap to make, but
  overdraw-heavy (above).
- Interaction: bending from the player's position in the vertex shader (as here), and trail
  textures for grass that stays flat.

## Everything together

`make capture` with the props and the grass on (1600 x 900, mobile renderer, M2; ms for the CPU
and GPU together; draw calls and objects include the debug panel's):

| View | Case | ms / frame | Draw calls | M triangles |
|---|---|---|---|---|
| eye height | all on | 3.72 | 393 | 0.73 |
| eye height | no grass | 2.49 | 322 | 0.40 |
| eye height | no props | 1.94 | 188 | 0.38 |
| eye height | no occlusion culling | 3.90 | 457 | 0.78 |
| eye height | no visibility ranges | 5.72 | 1 094 | 1.91 |
| eye height | no mesh LOD | 3.82 | 393 | 2.62 |
| eye height | no shadows | 3.26 | 331 | 0.61 |
| on the ground, a hill ahead | all on | 3.03 | 334 | 0.66 |
| on the ground, a hill ahead | no occlusion culling | 4.21 | 556 | 0.89 |
| overview | all on | 3.39 | 537 | 0.51 |
| overview | no visibility ranges | 9.62 | 2 933 | 4.12 |

The props cost more than the grass (about 1.8 ms against 1.2 at eye height): about 200 more
draw calls, since each mesh variant in each chunk is a batch, and the trees' shadows. The grass
adds about 70 draw calls (a tile each) and its cost is mostly vertices. All the speed-ups still matter with both on; the
visibility ranges most.

## On a phone (OnePlus 5)

`export_presets.cfg` has three Android presets, all on the release template (real speed), signed
with Godot's debug key (for testing, not the store): "Android" (play it), "Android capture"
(the capture table) and "Android diagnose" (what the props cost: each kind off, their shading,
the 3D resolution). The capture ones run by themselves and print the table to the log:

```
godot --headless --path godot_demo --export-release "Android diagnose" export/fall_forest_diagnose.apk
adb install -r godot_demo/export/fall_forest_diagnose.apk
adb shell am start -n com.hari.fallforest.diagnose/com.godot.game.GodotAppLauncher
adb logcat -s godot:V        # the table, a row a line, after about 2 minutes
```

(The phone has to be unlocked and stay awake while it runs.) Every view and case is drawn a few
times first: the first time a material is drawn its shader is compiled, and meanwhile Godot draws
with a slower general one, which made the first numbers of the first run nonsense.

OnePlus 5 (2017: Snapdragon 835, Adreno 540, Android 10), Vulkan mobile renderer, 1920 x 1080:

| Case | Ground, hill ahead | Overview |
|---|---|---|
| all on, 3D trees to 400 m (before) | 67 ms | 83 ms |
| all on, flat trees from 60 m | 42 ms | 27 ms |
| no props | 32 | 21 |
| no shadows | 40 | 24 |
| 3D at 0.75 x the screen | 34 | 20 |
| 3D at 0.5 x the screen | 27 | 17 |

The trees were the problem: about 1 000 of them in view, each hundreds to thousands of triangles
even after Godot's LOD, cost this GPU 30 - 60 ms. Their cost hardly changed with the resolution
or the lighting, so it was their geometry, not their pixels. Far trees are now flat stand-ins
(`TreeBillboards`): four puffy blobs (circles with a sine-wave edge and a lighter rim) and a
rectangle trunk, about 200 triangles, opaque (no alpha: the outline is the triangles'), vertex
coloured, unlit, turned to the camera (and leaning back toward it when seen from above). Each
tree batch has a stand-in batch with the same copies; the 3D batch has it as its visibility
parent, so from 60 m a chunk switches from 3D trees to stand-ins. That made the trees about
5 ms instead of 30 - 60. What is left is mostly the ground and the grass at full resolution:
rendering the 3D at 0.75 x the screen is the next easy win on this phone.

