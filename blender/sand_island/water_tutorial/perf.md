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
Measured 2026-10-10 00:00:48 · Apple M2 (Apple8) · mobile renderer · 3D at 3636x2046 · 240 frames per step

| shader | step | file | ms / frame | vs previous step | shader cost (− step 1) |
|---|---|---|---|---|---|
| water | 1 | step01_flat | 0.73 | +0.00 | 0.00 |
| water | 2 | step02_depth | 0.75 | +0.02 | 0.02 |
| water | 3 | step03_colour | 0.74 | -0.01 | 0.01 |
| water | 4 | step04_alpha | 1.34 | +0.60 | 0.61 |
| water | 5 | step05_foam | 1.41 | +0.07 | 0.67 |
| water | 6 | step06_wash | 1.40 | -0.00 | 0.67 |
| water | 7 | step07_wobble | 1.59 | +0.18 | 0.85 |
| water | 8 | step08_lines | 1.84 | +0.25 | 1.10 |
| water | 9 | step09_sparkles | 1.92 | +0.09 | 1.19 |
| water | 10 | step10_swell | 1.93 | +0.01 | 1.20 |
| water | 11 | step11_noise_texture | 1.65 | -0.28 | 0.92 |
| sand | 1 | step01_lit | 1.25 | +0.00 | 0.00 |
| sand | 2 | step02_height | 1.26 | +0.00 | 0.00 |
| sand | 3 | step03_zones | 1.31 | +0.05 | 0.06 |
| sand | 4 | step04_wet | 1.36 | +0.04 | 0.10 |
| sand | 5 | step05_waves | 1.42 | +0.06 | 0.16 |
| sand | 6 | step06_noise | 2.10 | +0.69 | 0.85 |
| sand | 7 | step07_grain | 2.20 | +0.10 | 0.95 |
| sand | 8 | step08_ripples | 2.47 | +0.27 | 1.22 |
| sand | 9 | step09_ripple_light | 2.96 | +0.49 | 1.71 |
| sand | 10 | step10_toon | 2.73 | -0.24 | 1.47 |
| sand | 11 | step11_noise_texture | 1.90 | -0.82 | 0.65 |
| snow | 1 | step01_rock | 1.21 | +0.00 | 0.00 |
| snow | 2 | step02_slope | 1.22 | +0.02 | 0.02 |
| snow | 3 | step03_snow_line | 1.30 | +0.08 | 0.10 |
| snow | 4 | step04_edge | 1.38 | +0.08 | 0.17 |
| snow | 5 | step05_blue_shadow | 1.41 | +0.03 | 0.21 |
| snow | 6 | step06_colour | 1.50 | +0.09 | 0.29 |
| snow | 7 | step07_drifts | 1.80 | +0.30 | 0.59 |
| snow | 8 | step08_sparkle | 2.04 | +0.25 | 0.84 |
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

**Next win for water:** make deep water opaque (alpha only near the shore) and stop drawing seabed
under deep water — that targets the +1.0 ms of step 4.

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
