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
Measured 2026-10-09 21:36:25 · Apple M2 (Apple8) · mobile renderer · 3D at 4800x2700 · 240 frames per step

| shader | step | file | ms / frame | vs previous step | shader cost (− step 1) |
|---|---|---|---|---|---|
| water | 1 | step01_flat | 0.88 | +0.00 | 0.00 |
| water | 2 | step02_depth | 0.86 | -0.01 | -0.01 |
| water | 3 | step03_colour | 0.91 | +0.05 | 0.04 |
| water | 4 | step04_alpha | 1.92 | +1.00 | 1.04 |
| water | 5 | step05_foam | 1.96 | +0.04 | 1.08 |
| water | 6 | step06_wash | 2.02 | +0.06 | 1.14 |
| water | 7 | step07_wobble | 2.40 | +0.38 | 1.52 |
| water | 8 | step08_lines | 2.85 | +0.45 | 1.97 |
| water | 9 | step09_sparkles | 2.91 | +0.07 | 2.04 |
| water | 10 | step10_swell | 2.92 | +0.00 | 2.04 |
| water | 11 | step11_noise_texture | 2.49 | -0.43 | 1.61 |
| sand | 1 | step01_lit | 1.74 | +0.00 | 0.00 |
| sand | 2 | step02_height | 1.73 | -0.01 | -0.01 |
| sand | 3 | step03_zones | 1.87 | +0.14 | 0.13 |
| sand | 4 | step04_wet | 1.95 | +0.08 | 0.22 |
| sand | 5 | step05_waves | 2.00 | +0.04 | 0.26 |
| sand | 6 | step06_noise | 3.10 | +1.10 | 1.36 |
| sand | 7 | step07_grain | 3.25 | +0.16 | 1.52 |
| sand | 8 | step08_ripples | 3.78 | +0.53 | 2.05 |
| sand | 9 | step09_ripple_light | 4.56 | +0.78 | 2.82 |
| sand | 10 | step10_toon | 4.22 | -0.34 | 2.48 |
| sand | 11 | step11_noise_texture | 2.77 | -1.45 | 1.03 |
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
