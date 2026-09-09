# Environment props

Procedural set-dressing for the open world: semi-realistic stylized (Odyssey / Link's Awakening), not
cel-shaded. Every prop follows the same pattern so it can be placed by hand, by `TerrainSplineArray`,
by `TerrainSplineScatter`, or later by the map generator:

- `XxxMesh : ArrayMesh` — the generator. Only its parameters are saved; geometry is rebuilt on load and
  whenever a parameter changes (`_queue_rebuild` → one deferred `rebuild()` per frame, `emit_changed`).
  Vertex colours carry the look; UV.y is a 0..1 "height" ramp the shader can use.
- `Xxx : MeshInstance3D` — thin node: creates the mesh on first use, applies the shared prop material
  unless `material` is set, keeps an internal StaticBody3D / CollisionShape3D in sync when
  `collision_enabled`. `seed_from_position` (Rock): the node takes a private copy of its
  mesh resource and seeds it from its world position (0.5 m cells), re-seeding when moved — so
  duplicate-and-drag gives a different rock every time, while nodes sharing one resource stay
  identical when it is off. All shape / size / variation knobs live on the mesh resource: expand it in
  the inspector (click the thumbnail next to `rock_mesh` / `tree_mesh`).
  `_validate_property` strips `mesh` and `material_override` from storage so the
  scene file only holds parameters.
- `make_prop_material()` (prop_material.cpp) — one shader for all props: wrapped diffuse (`wrap`, light
  bleeds past the terminator like a translucent solid), tight specular, fresnel rim (`rim_*`), back-light
  `transmission`, emission ramp along UV.y (`glow`), `saturation`; honours the F3 `ts_debug_view` global.
  `LIGHT_COLOR / PI` because Godot pre-multiplies energy by π.

## Props

| Class | Files | What |
|-------|-------|------|
| `CrystalClusterMesh` / `CrystalCluster` | crystal_cluster_mesh.*, crystal_cluster.* | Amethyst-style cluster: main prism, `crystal_count` ring leaning outward by `lean`, `small_count` shards, `pebble_count` squat pebbles at the base; n-sided tapered prisms (`sides`, `taper`, `tip_ratio`, `sink`), faceted or smooth, `base_color` → `tip_color` gradient (`gradient_power`, `color_variation`). Node adds `glow` / `transmission`. Collision = convex hull of the main crystal. |
| `RockMesh` / `Rock` | rock_mesh.*, rock.* | Displaced icosphere — or, with `base_shape = Cube`, the icosphere projected onto a cube and blended back by `roundness` for blocky, jointed boulders — with fractal value noise (`ridged` creases), `shear` (leaning slabs), per-blob `tilt`, `facet_snap` onto `facet_count` planes for split-stone faces, `flatten_bottom` + `sink`, flat or smooth. Clusters: PILE (heap, small stones on top) / OUTCROP (line of boulders over `spread` m — cliff feet, mountain rubble) / STACK (slabs stacked with shift and twist — cairns, jointed rock towers). Colour: height gradient, concavity darkening (`crevice_*`), `strata` bands, `moss_color` on up-facing surfaces. One convex collider per blob. `custom_mesh` for a Blender rock. Replaces the old convex-hull rock (retired: hulls have no concavities). |
| `SplineRocks` | spline_rocks.* | SplineComponent: boulders along the parent spline every `spacing` m in `rows` rows — overlapping = a continuous rock wall (cliff feet, mountain flanks, ridge lines), sparse = a boulder trail. `variants` RockMesh seeds share one style; per instance random size, yaw or tangent-aligned; `snap_to_ground` on the Terrain3D heightmap (retries while chunks stream). One MultiMesh per variant + per-blob convex colliders, stone material. |

### Crystal geometry

```
_layout       seeded PCG: main at the origin; ring at height·spread with jittered angle/distance,
              height 0.6·(1 ± variation), lean·(0.6..1.4) outward; shards 0.2..0.4 high leaning more;
              pebbles 0.05..0.12 high, taper 1, tip 0.6, just outside the ring
_append_crystal   base ring buried sink·height, top ring at height·(1 − tip), apex; faceted = own
              vertices + flat normal per side / tip facet / bottom cap; smooth = shared rings, radial
              normals. Colour = lerp(base, tip, t^gradient_power)·(1 ± tint·color_variation), UV = (angle, t).
              Triangles are wound clockwise-from-outside (Godot front faces) by comparing the geometric
              cross product with the vertex normal.
```

### Rock geometry

```
_layout        SINGLE | PILE (main + ring at ground + smaller perched on top) | OUTCROP (along X over
               `spread`, largest in the middle) | STACK (slabs 40 % high, each on the last, shifted, twisted),
               each blob: centre, half extents, yaw, tilt, tint, noise seed
_append_blob   1. base = icosphere, or projected onto the unit cube and lerp'd back by roundness (blocks);
                  radius = 1 + amplitude·fbm(v·freq) (ridged: 1 − |n|); × half extents; shear x/z by y;
                  tilt about a random horizontal axis
               2. facets: group verts by nearest of facet_count random planes, project toward the
                  group's mean plane by facet_snap  → broad flat faces
               3. bottom: clamp y to the cut plane, drop by sink
               4. concavity = mean neighbour radius − radius  (dents → crevice_color)
               5. colour: base→top by height · crevice · strata bands · moss where normal.y is high
               6. flat (per-face normals) or smooth (accumulated) emit; points kept per blob for the hull
```

Demo: `chunked_terrain_demo.tscn` → crystals `CrystalBig` / `CrystalBlue`, rocks `RockOutcrop` (leaning strata blocks), `RockPile` (mossy blocky heap), `RockStack` (slab cairn), `RockBoulder` (smooth), and `RockWallSpline/Rocks` (two-row wall along a spline) — all by the lake.
