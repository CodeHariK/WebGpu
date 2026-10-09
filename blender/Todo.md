# Blender / stylized art — Todo

## Sand island shaders: make them cheap on mobile (`sand_island/`)

Not measured on a phone yet; estimate from the shader code. Geometry is fine, the per-pixel
procedural noise is the cost.

| Part | Cost now | Why |
|---|---|---|
| `sand.gdshader` | medium–high | ~10 `noise()` calls per pixel (each 8 hashes ≈ 80 hashes + trig). Ripples are the worst: evaluated 3× for the normal tilt (6 noise calls). Every pixel also computes all zones (grass, bank, sand, wet). |
| `water.gdshader` | medium | Light maths, but alpha-blended over the seabed: every sea pixel shades the sand shader *and* the water shader (double work over most of the screen). |
| Geometry | low–medium | ~31k tris (terrain 22k, water 8k); the directional shadow map draws the terrain again. |
| Shadows + MSAA 2× | medium | Both cost on phones. |

Fixes, best value first:

- [ ] **Noise texture instead of hash noise** — one 128² R8 tileable noise texture; each `noise()`
      becomes one texture fetch instead of 8 hashes. Same look; removes most of the sand cost.
- [ ] **Cheaper ripples** — drop the normal tilt (2 fewer evaluations) or store the ripple pattern
      in a channel of the same noise texture.
- [ ] **Zone masks per vertex** — grass / bank / wet / seabed depend only on height and slope:
      compute in `vertex()` and pass as varyings.
- [ ] **Opaque deep water** — alpha only near the shore (vertex-colour depth), don't draw seabed
      under deep water (cull terrain below ~-1 m or make deep water opaque).
- [ ] `mediump` precision for colours and masks.
- [ ] Shadows: short shadow distance, or baked, and off on low-end phones.
- [ ] Measure on device: Godot profiler / frame-time monitor; on iPhone an Xcode GPU frame capture.
      Worst case = island filling the screen. Compare before/after the fixes.
- [ ] Coarser water grid away from the shore; lower-res terrain grid (or move the sand shader onto
      Terrain3D / TerraSpline ground — it only needs world position and normal).
- [ ] Keep `godot_demo/` copies of `sand.gdshader`, `water.gdshader`, `sand_island.glb` in sync
      with the ones in `sand_island/` (or make the demo use the parent files).
