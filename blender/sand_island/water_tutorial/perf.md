# Shader performance, step by step

How much each tutorial step costs on the GPU, and why. The table is written by the demo itself —
press **B** in the running demo (or run `godot --path . -- --bench`) and the part between the
`bench` markers is replaced with fresh numbers. The explanations below it are written by hand and
quote the numbers from the run they were written against (9 Oct 2026, Apple M2).

## How it is measured

- The camera looks straight down so **one material fills the whole screen**: water over open shallow
  sea (the ground under it kept on the cheapest sand step), sand on dry beach (water hidden).
- 3D is rendered at **3× resolution (4800×2700, ~13 million pixels)** so the GPU is the slow part,
  not the CPU.
- Each step is drawn **240 frames back to back** (`PerfHud.measure_gpu`) and the wall time divided by
  240. Godot's own GPU timer reads 0 on Metal (macOS); on Vulkan/Android it works and the on-screen
  HUD shows it.
- **ms / frame** includes everything (clearing, the sun's shadow map, the other material in view),
  so read the *differences*: **vs previous step** = what that step's new lines cost, **shader cost**
  = the shader's own cost above step 1.
- Numbers are for a desktop M2. A phone GPU is roughly 5–10× slower but a 1080p phone screen has
  ~6× fewer pixels than this test, so these ms are a rough guide to a phone at full screen. Measure
  on the device before trusting it (Todo: Xcode GPU capture / Android GPU profiler).

<!-- bench:start -->
Measured 2026-10-11 00:45:34 · Apple M2 (Apple8) · mobile renderer · 3D at 4800x2700 · 240 frames per step

| shader | step | file | ms / frame | vs previous step | shader cost (− step 1) |
|---|---|---|---|---|---|
| water | 1 | step01_flat | 0.87 | +0.00 | 0.00 |
| water | 2 | step02_depth | 0.84 | -0.02 | -0.02 |
| water | 3 | step03_colour | 0.86 | +0.02 | -0.01 |
| water | 4 | step04_alpha | 1.90 | +1.04 | 1.03 |
| water | 5 | step05_foam | 1.92 | +0.03 | 1.06 |
| water | 6 | step06_wash | 1.98 | +0.06 | 1.12 |
| water | 7 | step07_wobble | 2.34 | +0.36 | 1.48 |
| water | 8 | step08_lines | 2.75 | +0.41 | 1.89 |
| water | 9 | step09_sparkles | 2.86 | +0.10 | 1.99 |
| water | 10 | step10_swell | 2.85 | -0.01 | 1.98 |
| water | 11 | step11_noise_texture | 2.47 | -0.38 | 1.61 |
| water | 12 | step12_baked_noise | 2.25 | -0.23 | 1.38 |
| water | 13 | step13_opaque | 1.23 | -1.02 | 0.36 |
| sand | 1 | step01_lit | 1.70 | +0.00 | 0.00 |
| sand | 2 | step02_height | 1.70 | +0.00 | 0.00 |
| sand | 3 | step03_zones | 1.83 | +0.13 | 0.13 |
| sand | 4 | step04_wet | 1.92 | +0.09 | 0.22 |
| sand | 5 | step05_waves | 1.99 | +0.07 | 0.29 |
| sand | 6 | step06_noise | 3.04 | +1.05 | 1.34 |
| sand | 7 | step07_grain | 3.27 | +0.24 | 1.58 |
| sand | 8 | step08_ripples | 3.78 | +0.50 | 2.08 |
| sand | 9 | step09_ripple_light | 4.57 | +0.79 | 2.87 |
| sand | 10 | step10_toon | 4.23 | -0.34 | 2.53 |
| sand | 11 | step11_noise_texture | 2.79 | -1.44 | 1.09 |
| sand | 12 | step12_baked_ripples | 2.36 | -0.43 | 0.66 |
| sand | 13 | step13_baked_macro | 2.18 | -0.18 | 0.48 |
| snow | 1 | step01_rock | 1.67 | +0.00 | 0.00 |
| snow | 2 | step02_slope | 1.71 | +0.04 | 0.04 |
| snow | 3 | step03_snow_line | 1.80 | +0.09 | 0.13 |
| snow | 4 | step04_edge | 1.95 | +0.16 | 0.28 |
| snow | 5 | step05_blue_shadow | 1.99 | +0.04 | 0.32 |
| snow | 6 | step06_colour | 2.13 | +0.14 | 0.46 |
| snow | 7 | step07_drifts | 2.63 | +0.50 | 0.97 |
| snow | 8 | step08_sparkle | 3.06 | +0.43 | 1.40 |
| snow | 9 | step09_baked | 2.42 | -0.64 | 0.75 |
<!-- bench:end -->

## What each step costs, and why

### Water

| step | cost | why |
|---|---|---|
| 1 flat · 2 depth · 3 colour | ≈ 0 | A colour, a varying, two `smoothstep` + `mix`. A modern GPU does dozens of these per pixel for free — the frame is limited by everything else. Rule: **simple maths is cheap**. |
| **4 alpha** | **+1.0 ms** (the biggest water step) | Turning on transparency (`blend_mix`). The water can no longer hide what's behind it: the seabed under it is drawn *and* the water is blended on top, every sea pixel is read-modified-written, and the GPU can't skip hidden pixels early. **Transparency costs more than all the water's maths together.** |
| 5 foam · 6 wash | +0.04 / +0.06 | One `smoothstep`, one `sin`. Cheap. |
| **7 wobble** | **+0.38 ms** | The first `noise()`: 8 `hash()` calls ≈ 80 multiplies per pixel. |
| **8 lines** | **+0.45 ms** | A second `noise()` plus `fract` and two `smoothstep`. |
| 9 sparkles | +0.07 | One `hash()` per pixel (not 8). |
| 10 swell | ≈ 0 | `vertex()` work runs once per **vertex** (≈2k), not per pixel (≈13M) — almost free. Rule: **move work to vertex() when you can**. |
| **11 noise texture** | **−0.43 ms** | Each `noise()` becomes 2 texture reads (time-varying) instead of 8 hashes. Removes almost all the noise cost — but the water's biggest cost (alpha, step 4) is untouched. |
| **12 baked noise** | **−0.23 ms** | `water_macro.png` holds two versions of each moving noise; the shader cross-fades them with `sin(TIME)` and drifts the texture. 1 read replaces 4. Water's own noise cost is now small; alpha (step 4) is ~3/4 of what's left. |
| **13 opaque** | **−1.02 ms** | No `blend_mix`: the water is solid and mixes a fake bed colour in by the old ALPHA amount. Undoes step 4's +1 ms: no separate transparent pass, and the depth test skips the sea bed under the water. Water's own cost: 2.02 → 0.36 ms over steps 10–13. |

**Done in step 13:** the water is opaque and fakes the see-through, which removes step 4's +1 ms.

### Sand

| step | cost | why |
|---|---|---|
| 1 lit · 2 height | ≈ 0 | Godot's standard lighting + one varying. |
| 3 zones · 4 wet · 5 waves | +0.14 / +0.08 / +0.04 | `smoothstep`, `mix`, one `sin`: cheap, as with the water. |
| **6 noise** | **+1.10 ms** (the biggest sand step) | Five `noise()` calls (two sand tones, wet edge, grass patches, bank stripes) ≈ 40 hashes per pixel. Note the shader computes grass, bank *and* sand for every pixel — a GPU runs both sides of the `mix`. |
| 7 grain | +0.16 | Two `hash()` calls. |
| **8 ripples** | **+0.53 ms** | `ripple()` = two more `noise()` calls + `pow`. |
| **9 ripple light** | **+0.78 ms** | `ripple()` is evaluated **twice more** (to measure the slope), i.e. four more `noise()` calls — the light effect costs more than the ripples themselves. |
| 10 toon light | **−0.34 ms** | Our `light()` (one `dot` + `smoothstep` + `mix`) is *cheaper* than Godot's full PBR lighting it replaces. Stylized can be faster than realistic. |
| **11 noise texture** | **−1.45 ms** | All ~11 `noise()` calls become 1 texture read each, grain becomes `texelFetch`. The sand shader's own cost falls from 2.48 to 1.03 ms (−58%) with the same look. |
| **12 baked ripples** | **−0.43 ms** | Ripples + slope + grain baked in Blender (`sand_ripples.png`): 1 read replaces 6 noise reads + 2 hashes. |
| **13 baked macro** | **−0.20 ms** | The 4 remaining `noise2()` (tone, wet wobble, grass patches, stripe bend) baked into `sand_macro.png` (80 m tile): 1 read replaces 4. Smaller win than 12 because each `noise2()` was already a single cached read. Sand shader's own cost: 0.43 ms, 2 texture reads total. |

### Snow

| step | cost | why |
|---|---|---|
| **7 drifts** | **+0.50 ms** | `ripple()` run 3 times (height + 2 for the slope) = 6 noise reads. |
| **8 sparkle** | **+0.44 ms** | 4 `tex_hash()` per grain, plus the half-vector test in `light()`. |
| **9 baked** | **−0.65 ms** | `snow_texture/snow_texture.blend` + `bake_snow_texture.py`: drifts (height + slope), the 4 big noises and the grain random numbers baked into 3 textures. 14 reads → 3. Snow's own cost: 1.39 → 0.74 ms. |

## Rules of thumb from these numbers

1. **Simple maths is nearly free; loops of maths are not.** One `smoothstep` ≈ nothing; `noise()`
   (8 hashes) ≈ 0.2–0.4 ms each at this resolution.
2. **A small texture read beats a lot of maths.** 128×128 random pixels replace 8 hashes per call.
3. **Transparency is expensive** — more than the whole water shader's maths. Keep it to where you need it.
4. **Per-vertex is cheaper than per-pixel** (step 10 water swell is free).
5. **Evaluating a pattern several times to get its slope multiplies its cost** (sand step 9).
6. **Measure, then optimise.** The ranking above (alpha, noise, slope sampling) is not what you'd guess
   from line counts.

## Rock (rock/rock.tscn: Rock.blend mesh + rock_rg.png, the Blender look and steps 1–9)

Run: B in rock/rock.tscn, or `godot --path . res://rock/rock.tscn -- --bench`.

<!-- rock-bench:start -->
Measured 2026-10-10 23:14:14 · Apple M2 (Apple8) · mobile renderer · 3D at 4800x2700 · 240 frames per view

| view | shader | ms / frame | vs previous step | shader cost (− step 1) |
|---|---|---|---|---|
| 0 | rock.gdshader (Blender look) | 1.50 | — | +0.03 |
| 1 | step01_plain | 1.47 | — | +0.00 |
| 2 | step02_facet_tint | 1.64 | +0.17 | +0.17 |
| 3 | step03_sky_colours | 1.74 | +0.10 | +0.27 |
| 4 | step04_painted_light | 1.80 | +0.06 | +0.32 |
| 5 | step05_washes | 2.09 | +0.29 | +0.62 |
| 6 | step06_strokes | 2.17 | +0.08 | +0.69 |
| 7 | step07_edges | 2.22 | +0.05 | +0.74 |
| 8 | step08_moss | 2.39 | +0.17 | +0.92 |
| 9 | step09_baked_paint | 2.06 | -0.33 | +0.59 |
<!-- rock-bench:end -->
