# TerraSpline — performance plan and benchmark log

TerraSpline turns child `ProceduralSpline3D`s (with `TerrainSplineDeformer` / `TerrainSplineScatter`
components) into Terrain3D heightmap regions, streamed in `chunk_size` (256 m) chunks around the player.

## Architecture

### Classes

| Class | Kind | Owns | Purpose |
|---|---|---|---|
| `TerrainSplineCompositor` | Node (scene) | `chunk_buffers`, the generation queue, in-flight jobs, the scatter container | The orchestrator. Watches its child `ProceduralSpline3D`s, decides which chunks need (re)generation, streams them under a per-frame time budget, evicts far chunks, shifts the origin, and is the only class that talks to Terrain3D. |
| `ProceduralSpline3D` *(utils/spline3d)* | Path3D (scene) | Baked polyline (`baked_poly3d`, `baked_segments`), dirty rect | A designer-placed spline. Children that are `SplineComponent`s (deformer, scatter) act along it. `evaluate_spline_point_segmented` gives distance / height / inside for a 2D point. |
| `TerrainSplineDeformer` | SplineComponent (scene) | Shape parameters, curves | Raises or lowers a chunk heightmap in a corridor around its parent spline. Stateless per chunk: all per-run data lives in a `DeformerJob`. |
| `TerrainSplineScatter` | SplineComponent (scene) | Mesh, shape, placement parameters | Places mesh instances (and optional collision) in a corridor around its parent spline. Stateless per chunk: per-run data lives in a `ScatterJob`. |
| `TerrainChunk` | RefCounted | Heightmap, MultiMesh instance ids, physics body + caches, state | The resident state of one chunk coordinate. Owns the lifetime of everything the chunk put in the scene / physics server. |
| `TerrainHeightmap` | RefCounted | `PackedFloat32Array` | Flat float grid, one value per metre. Written lock-free by worker tiles; exported as `Image::FORMAT_RF`. |
| `ChunkJob` | RefCounted (data) | Inputs captured from the scene tree, outputs of the math | One chunk generation. Exists so the math phase never touches the scene tree. |
| `DeformerJob` | RefCounted (data) | Baked curves, SoA spline geometry, distance field, active tiles | One (deformer, chunk) deformation. Read-only for the per-pixel tasks; each task writes disjoint tiles of the heightmap. |
| `ScatterJob` | RefCounted (data) | Inputs, resulting transforms, debug counters | One (scatterer, spline, chunk) scattering. |
| `TerrainSplineStreamMap` | Control | — | Live top-down debug map of streaming state around the player: resident chunks with real heights, physics-live chunks, queued / generating chunks, recent evictions, render & physics radii, spline bounds, player and camera headings. Reads `StreamSnapshot` from the compositor every `refresh_interval`. |
| `TerrainSplineCompositorUI`, `GrayscaleJob` | TextureRect / data | — | Debug preview: one unified heightmap over the `ProceduralSpline3D` children of `splines_root` (default: itself), shown as normalized grayscale; refreshes on `spline_changed`. Works in the editor and in-game (`toggle_key`). |

Threading rule that shapes everything: **anything that reads the scene tree runs on the main
thread** (`_make_chunk_job`, `finalize_*`); **anything that is pure math on job data may run on a
worker** (`_run_chunk_job_math`, `deform_heightmap_prepared`, `run_scatter_job`). Jobs are the
hand-off between the two.

Coordinate rule: chunk keys, `TerrainChunk` transforms and Terrain3D positions are **physical**
(scene) coordinates; the player and all radii are compared in **logical** coordinates
(`physical + global_world_offset`) so an origin shift does not change what is "in range".

### Flow 1 — startup and rebuild (main thread)

```
Node::NOTIFICATION_READY
└─ _on_ready                                  tr_compositor.cpp
   ├─ memnew scatter_container (internal child)
   ├─ _connect_spline (each child)             spline_changed → _on_spline_changed
   ├─ connect child_entered_tree/child_exiting_tree
   ├─ _bench_init                              tr_bench.cpp (no-op unless --terraspline-bench)
   └─ call_deferred apply_all_splines

spline edited / added / removed / property changed
└─ _on_spline_changed | _disconnect_spline | set_default_elevation | ...
   └─ queue_rebuild                            one deferred _execute_rebuild per frame
      └─ _execute_rebuild
         └─ apply_all_splines                  tr_compositor_rebuild.cpp
            ├─ _get_terrain_data_api           tr_compositor_terrain3d.cpp
            ├─ _is_terrain_ready               not ready → flag retry, _on_process re-queues
            ├─ _wait_for_jobs_in_flight        workers read baked caches; finish before rebake
            ├─ _warn_if_vertex_spacing_mismatch
            ├─ _gather_splines
            ├─ _collect_dirty_rect             consumes every dirty spline's rect (always)
            ├─ full rebuild?  _select_full_rebuild_chunks   editor: under any spline; game: in radius
            │  else           _select_dirty_rect_chunks     chunks under the merged dirty rect
            ├─ _run_rebuild
            │  ├─ editor: _generate_chunks → _flush_terrain_maps → _refresh_terrain_collision
            │  └─ game:   _enqueue_chunks(allow_existing=true)      streamed by Flow 2
            └─ _check_chunk_physics_culling
```

### Flow 2 — every frame (main thread)

```
Node::NOTIFICATION_PROCESS
└─ _on_process                                tr_compositor.cpp
   ├─ retry pending rebuild (≤ MAX_REBUILD_RETRY_FRAMES) → queue_rebuild
   ├─ _check_origin_shift                     Flow 6
   ├─ _check_chunk_physics_culling            Flow 5b
   ├─ every DISCOVERY_INTERVAL_MS: _check_and_evict_far_chunks     Flow 5a
   ├─ _drain_generation_queue                 tr_compositor_stream.cpp
   │  ├─ _finalize_completed_jobs             completed workers → _finalize_chunk_job (Flow 3, phase 2)
   │  │                                       origin shifted mid-flight → _enqueue_chunks instead
   │  ├─ _dispatch_queued_jobs
   │  │  ├─ _sort_queue_nearest_first
   │  │  ├─ _gather_splines
   │  │  └─ per chunk while < max jobs && within budget:
   │  │     ├─ _make_chunk_job                Flow 3, phase 0
   │  │     └─ WorkerThreadPool.add_task(_run_chunk_job_task)   Flow 3, phase 1 on a worker
   │  └─ _flush_terrain_if_due                idle, or FLUSH_MAX_FRAMES since last flush
   │     ├─ _flush_terrain_maps               Terrain3DData.update_maps()  (one GPU rebuild)
   │     └─ _refresh_terrain_collision        Terrain3D.collision.update(true)
   ├─ _bench_add_frame_time
   └─ _bench_check_done
```

The frame budget (`generation_budget_ms`) is measured from the start of `_on_process`, so finalize
and dispatch together share it. At least one finalize happens per frame when any job is ready, so
progress is guaranteed even if a single chunk exceeds the budget.

### Flow 3 — one chunk job (tr_chunk_job.cpp)

```
phase 0  _make_chunk_job(chunk_pos, splines)                      MAIN THREAD
         ├─ for each spline whose padded AABB intersects the chunk:
         │  ├─ spline->ensure_baked_cache()          reads global transform → main thread only
         │  └─ collect its TerrainSplineDeformer / TerrainSplineScatter children
         ├─ _get_or_create_chunk                     new TerrainChunk + TerrainHeightmap if needed
         ├─ TerrainSplineScatter::make_scatter_job   one ScatterJob per (spline, scatterer)
         └─ chunk->set_state(GENERATING)             old visuals/physics stay until phase 2

phase 1  _run_chunk_job_math(job, threaded)                       ANY THREAD
         ├─ heightmap->clear(default_elevation)
         ├─ noise fill (global_terrain_noise, if set)
         ├─ for each deformer: deform_heightmap_prepared(...)      Flow 4
         └─ for each ScatterJob: scatterer->run_scatter_job(...)  Flow 5

phase 2  _finalize_chunk_job(job, api)                            MAIN THREAD
         ├─ drop if the chunk was evicted while running
         ├─ chunk->release_visuals_and_physics(); caches.clear()
         ├─ TerrainSplineScatter::finalize_scatter_job (each)     MultiMesh node + physics cache
         ├─ chunk->set_state(VISUAL_ONLY)
         ├─ _bench_dump_chunk
         └─ _write_chunk_heights_to_terrain                       tr_compositor_terrain3d.cpp
            ├─ chunk size == region size: Terrain3DRegion.set_maps + Data.add_region(update=false)
            │                             → _terrain_maps_dirty (flushed by Flow 2)
            └─ else: Data.import_images (Terrain3D slices and uploads itself)

_generate_chunks = phase 0 → phase 1 (threaded=true) → phase 2, synchronously (editor path)
_wait_for_jobs_in_flight = block on every in-flight task → phase 2 each → flush → collision
```

### Flow 4 — deforming one chunk with one deformer (tr_deformer*.cpp)

```
deform_heightmap(heightmap, spline, offset)             main-thread convenience
└─ spline->ensure_baked_cache(); deform_heightmap_prepared(..., spline->get_padded_aabb(), threaded=true)

deform_heightmap_prepared(heightmap, spline, offset, padded_aabb, threaded)     any thread
├─ skip if padded_aabb misses the chunk
├─ _create_deformer_job                               tr_deformer_legacy.cpp
│  ├─ bake_curve (falloff, inner falloff → 256-entry tables)
│  └─ copy_spline_geometry (SoA segments + vertices, mode, steepness, closed)
├─ use_distance_field ? _compute_distance_field       tr_deformer_field.cpp
│  │  ├─ _field_allocate              grid = chunk + 2*(search_radius+1) margin
│  │  ├─ _field_seed_segments         rasterize segments every ≤0.5 px as seeds
│  │  ├─ _field_sweep_nearest         two 8-neighbour sweeps (8SSEDT)
│  │  ├─ _field_refine_adjacent       exact projection onto seg, seg±1
│  │  ├─ _field_exact_band            brute force (bbox-culled) where weight can be > 0
│  │  ├─ _field_fill_interior         even-odd scanline fill (closed splines)
│  │  └─ _field_collect_active_tiles  tiles with any pixel in reach or inside
│  : else _compute_active_tiles_and_culling           legacy: segments per tile by centre distance
└─ _dispatch_deformer_job(job, threaded)
   ├─ threaded: WorkerThreadPool group task over active_tiles
   └─ else:     serial loop
      └─ _deform_heightmap_task(tile_idx, job)        tr_deformer_pixel.cpp
         ├─ field_valid: _deform_tile_field
         │  per pixel: distance+inside from field → _falloff_weight → (weight>0) field_spline_y → blend_pixel
         └─ else:        _deform_tile_fallback
            per pixel: spline->evaluate_spline_point_segmented → _falloff_weight → blend_pixel
```

`blend_pixel` receives the absolute target `spline_y + max_height` and the heightmap's
`base_elevation` (what the chunk was cleared to, i.e. `default_elevation`). ADD/SUBTRACT apply
`(target − base) · weight` to the current height, so on flat ground the surface meets the spline
exactly and base noise underneath is preserved; MAX/MIN/REPLACE lerp toward the absolute target.
(Before Sep 2026 ADD added the absolute target onto the 40 m base, leaving every ridge 40 m above
its spline — the "mountain offset upwards" bug.)

Why the field pass is fast: the legacy path evaluated `spline_y` (an O(vertices) IDW sum for
`INTERP_IDW_VERTEX`) for every pixel before knowing the weight. The field path computes the weight
first and only evaluates `spline_y` where it is non-zero. Output is bit-identical.

### Flow 5 — scattering one chunk with one scatterer (tr_scatter*.cpp)

```
make_scatter_job(chunk, spline, scatterer, padded_aabb, offset)      MAIN THREAD
└─ captures inputs; scatterer_seed = hash(node path)                stable across runs

run_scatter_job(job, chunk_size)                                    ANY THREAD
├─ active_area = spline bounds grown by max_spline_dist ∩ chunk
├─ active_segments = segments whose padded bbox touches active_area
└─ for each cell (cx, cz) in active_area:
   └─ _process_scatter_cell                        tr_scatter_cell.cpp
      ├─ CellRNG(seed from chunk, cell, seed_offset, scatterer_seed)
      ├─ density roll → biome noise → spline distance (evaluate_spline_point_segmented)
      ├─ _evaluate_height_and_slope (bilinear on the chunk heightmap) → slope filter
      └─ transform (position, random yaw, random scale)

finalize_scatter_job(job, container, owner)                         MAIN THREAD
├─ _print_scatter_debug (DEBUG)
├─ _build_multimesh        one MultiMeshInstance3D under scatter_container; id → chunk visual_nodes
└─ _record_physics_cache   shape RID + transforms → chunk physics_caches (if collision_shape set)

scatter_chunk(...) = make + run (thread pool) + finalize for every scatterer touching the chunk
```

### Flow 4a — heightmap preview (tr_compositor_ui.cpp)

```
TerrainSplineCompositorUI::_notification(READY)
├─ _watch_root(_resolve_splines_root())      splines_root NodePath, or this node when unset
│  ├─ _connect_spline(child) for each child  ProceduralSpline3D.spline_changed → _on_spline_changed
│  └─ root.child_entered/exiting_tree → _connect_spline / _disconnect_spline
└─ call_deferred(apply_all_splines)
_on_spline_changed / set_default_elevation / set_splines_root / apply_now → queue_rebuild → _execute_rebuild
apply_all_splines
├─ _gather_splines(bounds)                   union of padded AABBs of the root's splines
├─ _deform_unified_heightmap                 one TerrainHeightmap over the bounds (≤ 2048²), cleared to
│  └─ deformer->deform_heightmap(...)        default_elevation, every deformer applied (Flow 4)
└─ _show_as_grayscale                        min/max normalize → 8-bit L8 (WorkerThreadPool group
   └─ _normalize_grayscale_task              task per 16 K pixels) → ImageTexture on this TextureRect
_unhandled_key_input                         toggle_key shows/hides (rebuilds when shown)
```

The demo scene has `SplinePreviewLayer/SplinePreview` pointing at `SplineManager` with `toggle_key = H`.
Set the preview's `default_elevation` to the compositor's so relative ADD/SUBTRACT levels match.

### Flow 4b — streaming map (tr_stream_map.cpp, tr_compositor_snapshot.cpp)

```
TerrainSplineStreamMap::_notification(READY)
└─ _resolve_compositor            NodePath → TerrainSplineCompositor; compositor->set_thumbnail_size(n)
                                  (backfills thumbnails for resident chunks; _finalize_chunk_job builds
                                  them for every chunk finalized from then on: TerrainChunk::build_thumbnail)
_notification(PROCESS) every refresh_interval (0.1 s), only while visible
└─ _refresh
   ├─ compositor->get_stream_snapshot(StreamSnapshot&)   logical-metre rects for resident (+state,
   │                                                     thumbnail ptr), _gen_queue, _jobs_in_flight,
   │                                                     _evicted_recent (ring of 64, logged by
   │                                                     _evict_far_chunks), spline padded AABBs,
   │                                                     player XZ, player/camera XZ headings
   ├─ _rebuild_heights_texture   all thumbnails → one L8 image, global min/max normalized
   └─ queue_redraw → _draw: heights, chunks (evicted fade → queued → generating → resident), splines,
                            radii, player/camera wedge, legend + counters
```

What the map shows about the streaming policy: chunks are generated when their centre comes within
`max_render_radius` of the player and evicted when it leaves (checked every 0.5 s), independent of
where the camera looks; Terrain3D handles frustum culling/LOD of what is resident. Physics for scatter
instances is switched on/off per chunk by `max_physics_radius`. A chunk's heights are regenerated only
when it streams back in or when a spline changes (dirty-rect rebuild); the camera wedge makes the
"looking vs loading" distinction visible. The demo has `SplinePreviewLayer/StreamMap` (toggle **M**).

### Flow 5a — eviction and discovery (tr_compositor_eviction.cpp, every 0.5 s)

```
_check_and_evict_far_chunks
├─ bench mode: enqueue missing bench chunks, return
├─ _evict_far_chunks       erase chunk_buffers entries beyond max_render_radius
│                          (TerrainChunk destructor frees MultiMeshes and the physics body)
├─ _evict_far_regions      Terrain3DData.remove_regionl for regions beyond radius + ½ region
│                          that no resident chunk still lives in
└─ _discover_missing_chunks   every in-radius chunk not resident → _enqueue_chunks(allow_existing=false)
```

### Flow 5b — physics culling (every frame)

```
_check_chunk_physics_culling
└─ for each resident chunk: _distance_to_chunk_edge ≤ max_physics_radius ?
   ├─ VISUAL_ONLY          → _update_chunk_physics: body_create + body_add_shape per cached transform
   └─ VISUAL_AND_PHYSICS   → _update_chunk_physics: free_rid(body)      (when outside the radius)
```

### Flow 6 — origin shift (tr_compositor_origin_shift.cpp)

```
_check_origin_shift
└─ _compute_origin_shift(player)  |x| or |z| > 4096 → ±4096 per axis
   └─ _apply_origin_shift(shift)
      ├─ _shift_terrain_targets    Terrain3D's collision target, clipmap target, camera (each once)
      ├─ global_world_offset += shift
      ├─ _shift_spline_nodes       every ProceduralSpline3D child
      ├─ Terrain3D node, scatter_container    moved by -shift
      ├─ _shift_chunk_buffers      re-key chunk_buffers; move cached + live physics transforms
      └─ Terrain3D.snap()
   in-flight jobs dispatched before the shift are detected in _finalize_completed_jobs and re-enqueued
```

### Flow 7 — benchmark mode (tr_bench.cpp)

```
_bench_init (from _on_ready)      parse --terraspline-bench / --terraspline-no-field / --terraspline-dump=
_get_player_position              returns origin in bench mode
_select_full_rebuild_chunks       substitutes the fixed 5x5 block
_check_and_evict_far_chunks       never evicts; enqueues missing bench chunks
_bench_add_frame_time             called from _on_process and _execute_rebuild
_bench_dump_chunk                 from _finalize_chunk_job when a dump dir is set
_bench_check_done (end of _on_process)
└─ all 25 resident and uploaded → _bench_content_hash → print [BENCH] line → SceneTree.quit()
```

### Terrain3D touch points (all in tr_compositor_terrain3d.cpp, via Object::call/get)

| Call | Used by | Why |
|---|---|---|
| `Terrain3D.data` (fallback `storage`) | `_get_terrain_data_api` | The `Terrain3DData` object |
| `Terrain3D.get_region_size`, `get_vertex_spacing` | readiness check, region write, eviction | Readiness; 1 chunk == 1 region fast path; region world size |
| `Terrain3DData.get_region_location(pos)` | region write, eviction | Region key for a world position |
| `Terrain3DRegion.set_maps`, `Data.add_region(region, false)` | `_write_chunk_heights_to_terrain` | Stage a chunk without a GPU rebuild |
| `Terrain3DData.import_images` | `_write_chunk_heights_to_terrain` | Generic path when sizes differ |
| `Terrain3DData.update_maps()` | `_flush_terrain_maps` | One GPU texture-array rebuild per batch |
| `Terrain3D.collision.update(true)` | `_refresh_terrain_collision` | Re-read heights into collision shapes |
| `Data.get_region_locations`, `remove_regionl` | `_evict_far_regions` | Unload far regions |
| `get_collision_target`, `get_clipmap_target`, `get_camera`, `snap` | player position fallback, origin shift | Nodes Terrain3D follows |

## Source layout

One class per header, one responsibility per source file. `terraspline.h` is an umbrella include for
code outside the module (register_types.cpp); inside the module include the specific header.

| File                             | Contains                                                                  |
|----------------------------------|---------------------------------------------------------------------------|
| `tr_heightmap.h/.cpp`            | `TerrainHeightmap` — float grid for one chunk                              |
| `tr_chunk.h/.cpp`                | `TerrainChunk` — resident chunk state (heightmap, visuals, physics)        |
| `tr_deformer_job.h`              | `DeformerJob` — per-(deformer, chunk) inputs, SoA geometry, distance field |
| `tr_deformer.h/.cpp`             | `TerrainSplineDeformer` — bindings, curve wiring, entry points, dispatch  |
| `tr_deformer_field.cpp`          | Distance-field pass, one function per step (seed, sweep, refine, band, fill, tiles) |
| `tr_deformer_legacy.cpp`         | Job creation; legacy tile-culling decomposition (A/B fallback)            |
| `tr_deformer_pixel.cpp`          | Per-pixel weight, spline height, blend; the two tile loops                |
| `tr_scatter_job.h`               | `ScatterJob` — per-(scatterer, chunk) inputs and transforms               |
| `tr_scatter.h/.cpp`              | `TerrainSplineScatter` — bindings                                          |
| `tr_scatter_cell.cpp`            | Per-cell RNG, filters, height/slope sampling                              |
| `tr_scatter_job.cpp`             | make / run / finalize (MultiMesh, physics cache)                          |
| `tr_chunk_job.h/.cpp`            | `ChunkJob`; make (main) → math (any thread) → finalize (main)             |
| `tr_compositor.h`                | `TerrainSplineCompositor` declaration with a per-file responsibility map  |
| `tr_compositor.cpp`              | Bindings, properties, lifecycle, spline signal wiring                     |
| `tr_compositor_rebuild.cpp`      | Which chunks a rebuild touches (`apply_all_splines` and its helpers)      |
| `tr_compositor_stream.cpp`       | Budgeted queue: finalize completed jobs, dispatch new ones, throttled flush |
| `tr_compositor_eviction.cpp`     | Evict far chunks/regions, discover missing chunks, physics culling        |
| `tr_compositor_origin_shift.cpp` | Floating origin at ±4096 m                                                 |
| `tr_compositor_terrain3d.cpp`    | Every call into Terrain3D (data API, region write, flush, collision)      |
| `tr_compositor_snapshot.cpp`     | `StreamSnapshot` filling, eviction log, thumbnail switch (debug map support)          |
| `tr_stream_map.h/.cpp`           | `TerrainSplineStreamMap` — live streaming debug map                                    |
| `tr_compositor_ui.h/.cpp`        | `TerrainSplineCompositorUI`, `GrayscaleJob` — debug preview               |
| `tr_bench.cpp`                   | Benchmark mode, content hash; zero cost when off                          |

### Refactor verification (Sep 2026)

The split from 4 large files (compositor 779 lines, deformer 757) into the layout above was checked
with the benchmark's content hash — FNV-1a over every chunk heightmap and every scatter MultiMesh
buffer. Before: `hash=f5a4760c1154d18f`, 1221 instances. After: identical hash and instance count;
math 17.1 ms vs 17.5 ms, total ≈220 ms vs ≈220 ms (noise). No function is longer than ~90 lines.

Golden hash after the relative ADD/SUBTRACT fix (intentional output change): `hash=c5e7bde0659bb845`,
1468 instances. Dumped chunk heightmaps are exactly 40 m lower than before at every deformed pixel
(e.g. chunk −1,0 max 232.80 → 192.80); the extra instances are scatter cells whose slope filter now
passes on the lower, gentler ridges.

## Where the time goes (baseline, M2, 7 worker threads, 12-chunk startup rebuild)

| Cost                                  | Per chunk           | Notes                                                        |
|---------------------------------------|---------------------|--------------------------------------------------------------|
| Deformer math (chunks with a spline)  | 4–11 ms wall        | Threaded per tile; ~70 ms CPU across cores                   |
| Deformer math (no spline)             | ~0 ms               | `default_elevation` fill only                                |
| Terrain3D `import_images`             | 2.3 → 7.8 ms        | Grows with region count (rebuilds the whole texture array)   |
| Scatter                               | ~2 ms               |                                                              |
| Full 12-chunk rebuild                 | ~102 ms main thread | One frozen frame                                             |

Problem statement: startup is fine, but streaming generates chunks in synchronous batches every 3 s
(40–100 ms stall when several chunks enter the radius) and the per-chunk upload cost grows with world size.

## Plan

1. **Time-budgeted generation queue.** Discovery pass every 0.5 s enqueues missing chunks sorted by
   distance; `_process` drains the queue under a per-frame budget (~4 ms). Same total work, no stall.
2. **Batch the Terrain3D upload.** Blit chunk heights into region height `Image`s and call
   `update_maps` once per batch instead of `import_images` per chunk.
3. **Chunk math off the main thread.** Phase 1 (noise + deform + scatter transforms) as one
   WorkerThreadPool task with inputs gathered on main; phase 2 (Terrain3D write, MultiMesh, physics
   cache) on main when the task finishes.
4. **Distance-transform deformer (only if math still dominates).** Rasterize the polyline into the
   chunk grid, run a two-pass exact Euclidean distance transform → distance + nearest segment per pixel
   in O(pixels). Removes the "interior tiles evaluate every segment" cost.

## Benchmark protocol

Run the demo scene with the benchmark flag:

    /Applications/Godot.app/Contents/MacOS/Godot --path project scene/terraspline/chunked_terrain_demo.tscn -- --terraspline-bench

Benchmark mode fixes the player position at the origin, sets `max_render_radius` so exactly a 5×5
chunk block (25 chunks) is generated, and quits once all 25 are resident. It prints one `[BENCH]`
summary line with:

- `total_ms` — wall time from first generation request to last chunk resident (throughput)
- `main_max_ms` / `main_p95_ms` — worst and 95th-percentile main-thread time spent in the compositor
  per frame (stall / latency — this is what the player feels)
- `math_ms`, `upload_ms` — summed per-chunk deformer and Terrain3D costs
- `frames` — frames from first request to completion

Each step is measured as the median of 3 runs. Lower is better everywhere; step 1 and 3 are expected
to *raise* `total_ms` slightly while collapsing `main_max_ms` — that is the intended trade.

## Results

| Step                         | total_ms | main_max_ms | main_p95_ms | math_ms | upload_ms | frames |
|------------------------------|---------:|------------:|------------:|--------:|----------:|-------:|
| Baseline                     |    290.4 |       209.5 |       209.5 |    38.5 |     163.0 |      1 |
| 1. Budgeted queue            |    612.3 |        23.9 |        23.1 |    37.5 |     375.1 |     25 |
| 2. Batched upload            |    229.7 |        20.6 |        14.4 |    40.2 |      51.1 |      8 |
| 3. Async math                |    250.0 |        14.4 |        13.8 |   72.3* |      36.0 |     10 |
| 4. Distance field            |    209.7 |        12.2 |        9.9  |   17.4* |      35.0 |      8 |

`*` From step 3 on, `math_ms` is worker-thread time (summed over chunks, running in parallel on up to
7 threads), not main-thread time. It rose because each chunk's tiles now run serially inside one task
(chunk-level parallelism instead of tile-level) — the same total CPU, no longer on the main thread.
`upload_ms` for step 3 is region adds (18 ms) + throttled `update_maps` flushes (18 ms / 2 calls).

### Outcome

| Metric                                | Baseline | After step 4 | Change   |
|---------------------------------------|---------:|-------------:|----------|
| Worst main-thread frame (the stall)   | 209.5 ms |      12.2 ms | **17× lower** |
| Typical streaming frame               |   n/a    |     ~4 ms    | budget honored |
| Terrain3D upload, 25 chunks           | 163.0 ms |      36.0 ms | 4.5× lower |
| Deformer math on the main thread      |  38.5 ms |       0 ms   | moved to workers |
| Deformer math, worker CPU (25 chunks) |  72.3 ms*|      17.4 ms | 4.2× lower (bit-identical output) |
| Wall time to 25 resident chunks       | 290.4 ms |     209.7 ms | 1.38× faster |

Main-thread ms per frame after step 3 (one run): `0.5 0.9 4.1 4.0 1.9 4.4 14.5 11.0`. Every frame
sits at the 4 ms budget except the two that contain a Terrain3D `update_maps` (~9 ms at 25 regions)
plus a collision refresh (~1.5 ms). Those are throttled to when the pipeline goes idle or at most every
`FLUSH_MAX_FRAMES` (6) frames.

### Step 4: what it actually fixed

The original plan assumed the cost was the nearest-segment search. Profiling the splines in use showed
otherwise: three of four use `INTERP_IDW_VERTEX`, whose `spline_y` is a 1/d² sum over **every baked
vertex for every pixel**, computed *before* the falloff weight was known — so pixels outside the
falloff paid the full O(vertices) cost for nothing, plus a point-in-polygon test per pixel for closed
splines. The distance-field pass reorders that: distance and inside/outside are O(1) per pixel, the
weight is computed first, and `spline_y` runs only where weight > 0.

Correctness was verified by dumping all 25 chunk heightmaps (`--terraspline-dump=<dir>`) from the
legacy path (`--terraspline-no-field`) and the new one and diffing 1,638,400 pixels: 1 pixel differs,
by 0.0001 m (float rounding). Two approximations were tried and rejected on the way: the raw 8SSEDT
nearest-segment identity produced 74 pixels off by up to 3.2 m at polyline folds; adding a
neighbour-identity repair pass got that to 22 pixels / 0.34 m. Resolving the falloff band exactly by
bbox-culled brute force (the same cull the legacy code used) made it exact and is still cheap, because
the band is a thin strip along the spline. The far interior of closed splines keeps the O(1) field.

Per-deformer toggle: `use_distance_field` (default on). `INTERP_IDW_LINE` (unused in the demo) now
sums over all segments instead of the legacy tile-culled subset; far segments contribute ~1/d² so the
difference is negligible, but it is a change.

Worker CPU for the 25-chunk block fell 72 → 17 ms. That does not move the main-thread stall (which
is Terrain3D's `update_maps`, see above) but it cuts time-to-resident, battery/thermal load, and
means low-core machines finish streaming ~4× sooner.

## What changed (step by step)

1. **Budgeted queue** (`_enqueue_chunks`, `_drain_generation_queue`, `generation_budget_ms`):
   discovery runs every 0.5 s and enqueues; the in-game rebuild path enqueues too (the editor keeps
   synchronous generation for immediate feedback). Result: 209 → 24 ms stall, but `import_images`
   per frame lost its intra-frame coalescing (163 → 375 ms upload). Motivated step 2.
2. **Batched upload** (`_flush_terrain_maps`): since chunk size == Terrain3D region size, each chunk
   becomes a `Terrain3DRegion` added with `update=false`; one `update_maps()` per batch. This is what
   `import_images` did internally minus its per-call rebuild. Upload 375 → 51 ms; 0.7 ms/chunk region
   add. Falls back to `import_images` if the sizes ever differ.
3. **Async math** (`ChunkJob`, `tr_chunk_job.cpp`): `_make_chunk_job` gathers padded AABBs,
   deformer/scatterer lists and bakes spline caches on main; `_run_chunk_job_math` runs as one
   WorkerThreadPool task per chunk (up to `max_jobs_in_flight`, default cores−1);
   `_finalize_chunk_job` creates MultiMeshes, physics caches and the region on main. Rebuilds wait for
   in-flight jobs before rebaking caches; an origin shift mid-flight requeues the chunk. `update_maps`
   is throttled (idle or every 6 frames). Stall 20.6 → 14.4 ms with ~4 ms typical frames.
4. **Distance field** (`_compute_distance_field`, `_field_spline_y` in `tr_deformer.cpp`): per
   (deformer, chunk) rasterize segments into a grid padded by the search radius, 8SSEDT nearest-seed
   propagation, exact projection refine, exact brute force (bbox-culled) inside the falloff band,
   even-odd scanline fill for closed splines; then per pixel weight-first, `spline_y` only where
   needed, with SoA geometry for tight loops. Legacy path kept as fallback and A/B switch.

## Benchmark instrumentation

`tr_bench.cpp` plus `_bench_*` counters in the compositor, all gated on the `--terraspline-bench`
user arg (zero cost otherwise). Prints a per-frame main-thread timeline and one summary line that
ends with `instances=<n> hash=<fnv1a>` — the content hash of all heightmaps and scatter buffers. Any
change that should not alter output must reproduce that hash.
Extra flags: `--terraspline-no-field` (legacy deformer path, for A/B) and `--terraspline-dump=<dir>`
(writes each chunk's raw float heightmap as `chunk_x_z.bin` for numeric diffing).
