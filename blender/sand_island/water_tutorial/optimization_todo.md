# Optimization Todo — water, sand, snow, rock

Goal: the stylised look at mobile cost. Numbers are from `perf.md` (Apple M2, mobile renderer,
3D at 4800×2700 so the GPU is the bottleneck; "cost" = ms over the cheapest step of that shader).
They show where time goes, not phone frame times: measure on a device too (last section).

**Rule we keep landing on:** bake what a pattern *is* (expensive, never changes); compute *where*
and *how much* in the shader (cheap, or depends on placement, height, depth or time). Pack masks
into the R G B A channels of one small texture so one read returns several answers.

## Where the time goes now

| Shader | Biggest costs (ms) | Total shader cost |
|---|---|---|
| Water | alpha gone (step 13); what's left: colour, foam, lines, 1 read + 1 sparkle read | step 13: **0.36** (was 2.02 at step 10) |
| Sand | 2 texture reads; toon `light()`; zone masks | step 13: **0.43–0.48** (was 2.53 at step 10) |
| Snow | `light()` twice (rock + snow) + sparkle (both functions) ≈ 0.25; 3 reads | step 9: **0.74** (was 1.39 at step 8) — now the most expensive |
| Rock | washes +0.29 · moss +0.17 · facet tint +0.17 | Blender look: 0.03 · step 9: 0.59 |

Biggest single lesson: the rock's Blender look (AO edges, Voronoi dots, noise, waves) costs
**0.03 ms** because it is baked; the same detail computed in a shader would cost far more, and
the AO part could not run at all.

## 1. Bake in Blender

- [x] **Sand ripples → tileable texture** (sand step 12, −0.43): `sand_texture/sand_texture.blend`
      → `sand_ripples.png` 512², 4 m tile: R height, G/B slope, A grain.
- [x] **Sand big noises → macro texture** (sand step 13, −0.20): `sand_macro.png`, 80 m tile.
- [x] **Snow drifts, big noises, sparkle randoms → 3 textures** (snow step 9, −0.65):
      `snow_texture/snow_texture.blend` + `bake_snow_texture.py`. 14 reads → 3.
- [x] **Water moving noise → 1 read** (water step 12, −0.23): `water_texture/water_macro.png`, two
      versions per noise cross-faded with `sin(TIME)`.
- [ ] Hide the 4 m sand-ripple repeat if it shows from high up: a second read at another scale, or
      warp the UV with the macro texture.
- [x] Snow and water bakes come from `.blend` scenes like the sand (`snow_texture.blend`,
      `water_texture.blend`; shared helpers in `bake_lib/`), so the patterns can be tweaked by eye.
- [ ] **Rock moss → a mask in B, not green paint.** Bake "moss can grow here" (crevices, rough
      areas, painted by hand) into B of `rock_rg.png`. Shader: `moss = B × faces-up`. Artist shape
      from the bake, correct placement from the world normal, no extra texture read.
- [ ] **Rock cavity in G or A** if a kit needs darker cracks beyond what the AO edge mask gives.
- [ ] Bake every new rock the same way as `Rock.blend`: R = edge mask, G = pattern, B = moss mask,
      A spare. One read per pixel for the whole look.

## 2. Rock kit: small pieces, one atlas

- [ ] Make 8–16 rock pieces (0.5–1 m), bake each at **128²** (128–256 px per metre: plenty at that
      size; ~32 KB each).
- [ ] **Pack all pieces into one atlas** (e.g. 16 × 128² → one 512²) with one shared material, so
      pieces don't each cost their own draw call.
- [ ] **MultiMesh per piece type** for scattered rocks: hundreds of rocks, a handful of draw calls.
      Per-copy variety from rotation, scale, world-space moss and tint (the facet ID must use the
      instance origin from `MODEL_MATRIX[3]`, not `NODE_POSITION_WORLD`).
- [ ] **Cliffs: stack pieces in Blender, then bake the assembled cliff once**, so the AO between
      pieces is real. Loose stacks baked per piece look floating at the joins.
- [ ] Delete faces buried inside assembled cliffs (overdraw for nothing).
- [ ] Bench: one big rock vs a stacked cliff of pieces vs a MultiMesh field (`rock.tscn`, B).

## 3. Lean shaders for the game (keep the step files as the tutorial)

- [ ] **`rock/rock_moss.gdshader`**: baked texture + moss + optional darker foot, nothing else
      (no facet tint, washes, strokes or edge maths: the bake has them). Expected ≈ Blender look
      cost. Add to `rock.tscn` (key M) and the bench.
- [ ] Facet tint only on flat-shaded low-poly meshes (`facet_grid` 24); off for smooth meshes.
- [ ] Remove unused uniforms left in step 9 (`edge_colour`, `edge_strength`).
- [ ] Toon light bands: keep only where the look needs them; they are cheap but not free.

## 3b. Snow (most expensive now: 0.74 ms)

- [ ] **Measure the split first**: snow step 10 variants without sparkle and with one light model,
      benched, so the items below have real numbers (current split is an estimate:
      basic lighting ≈ 0.3, sparkle ≈ 0.25, reads + colour ≈ 0.2).
- [ ] **Light once, not twice.** `light()` computes the rock toon band AND the wrapped snow light,
      then blends by `snow`. Use one model (or pick by `snow > 0.5`) — halves `light()`.
- [ ] **Sparkle only where it matters.** Fade/skip it for far snow already happens via `fade`, but
      the facet maths (2 `normalize`, `VIEW_MATRIX`, `length`) runs for every pixel. Options: bake
      the tilt as an already-normalised offset; compute in world space and do the half-vector test
      with `INV_VIEW_MATRIX` once; or keep sparkle only on the hero snow, not the cheap variant.
- [ ] **Snow coverage per vertex** (slope + snow line + ragged edge) instead of per pixel; the edge
      noise is low-frequency, so vertex density is enough on the loft mesh.
- [ ] Drop unused uniforms left over from step 8 (`drift_spacing` is gone; check the rest).
- [ ] Bench snow next to sand on the same view so the two are directly comparable.

## 4. Transparency and overdraw (water is the worst)

- [x] **Opaque water** (water step 13, −1.02 ms): no blend_mix at all; the shader mixes a fake bed
      colour in by the old ALPHA amount, and the depth test skips the seabed under the water.
      Lost: seeing the real bed (its ripples) through the shallows.
- [ ] Don't draw seabed terrain below ~−1 m when the water above is opaque.
- [ ] Coarser water grid away from the shore.

## 5. Per-vertex instead of per-pixel

- [ ] Sand zone masks (grass / bank / wet / seabed) depend only on height and slope: compute in
      `vertex()`, pass as varyings.
- [ ] Same for snow coverage (slope + snow line) and the rock's dark foot (height above base).
- [x] Sparkle grain pick from a texture channel (snow step 9: one `texelFetch` → 4 random numbers).
- [ ] Sparkle: skip the facet maths on pixels that are not mirror grains (branch on `is_mirror`;
      measure, GPUs run both sides when neighbouring pixels disagree).

## 6. Textures and precision

- [ ] Data textures (noise, rock_rg): import Lossless or ASTC/ETC2 with care: lossy compression
      bends data values. Check the look after compressing.
- [ ] Mipmaps on for rock textures (no shimmer far away), off for `noise_128` (exact lookups).
- [ ] Texture sizes: 1024² only for hero rocks; 512² or 128² atlases for the kit.
- [ ] `mediump` for colours and masks in all shaders.

## 7. Engine settings

- [ ] Shadows: short shadow distance, fewer cascades; off on low-end phones.
- [ ] MSAA 2× → check FXAA / no AA on device.
- [ ] Godot must not import `.blend` files from the project at runtime (move `Rock.blend` out of the
      project or exclude it) — only the exported `.glb` + textures belong in the game.

## 8. Measure on a device

- [ ] Run `--bench` on an iPhone / Android build; add a device column to `perf.md`.
- [ ] Xcode GPU frame capture (iOS) / Android GPU Inspector for the worst case: island + water
      filling the screen.
- [ ] Re-bench after each item above; keep the before/after numbers in `perf.md`.
