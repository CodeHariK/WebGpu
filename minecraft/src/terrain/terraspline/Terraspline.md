# TerraSpline — performance plan and benchmark log

TerraSpline turns child `ProceduralSpline3D`s (with `TerrainSplineDeformer` / `TerrainSplineScatter`
components) into Terrain3D heightmap regions, streamed in `chunk_size` (256 m) chunks around the player.

## Decision log

- **Ribbon triangulation / barycentric rasterization (rejected, Sep 2026).** The deformer is not the
  bottleneck, and the ribbon self-overlaps on the concave side of any bend tighter than the falloff
  width (20–37 m here), which double-applies `BLEND_ADD`. The nearest-segment SDF handles that for free.
  If the deformer ever dominates, use a distance transform (step 4 below), not triangles.

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
user arg (zero cost otherwise). Prints a per-frame main-thread timeline and one summary line.
Extra flags: `--terraspline-no-field` (legacy deformer path, for A/B) and `--terraspline-dump=<dir>`
(writes each chunk's raw float heightmap as `chunk_x_z.bin` for numeric diffing).

