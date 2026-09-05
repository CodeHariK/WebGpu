/**
 * @file tr_chunk_job.cpp
 * @brief The chunk job pipeline: make (main) -> math (any thread) -> finalize (main).
 *
 * The synchronous editor path runs all three back to back with tile-level threading inside the
 * deformer; the streaming path (tr_compositor_stream.cpp) dispatches the math as one
 * WorkerThreadPool task per chunk and finalizes when it completes. Terraspline.md, step 3.
 */
#include "tr_compositor.h"
#include "tr_deformer.h"
#include "tr_scatter.h"
#include <godot_cpp/classes/time.hpp>
#include <godot_cpp/classes/worker_thread_pool.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

// ---------------------------------------------------------------------------------------------
// Phase 0: make (main thread)
// ---------------------------------------------------------------------------------------------

/// Returns the resident chunk, creating it (and its heightmap) when allowed; null otherwise.
Ref<TerrainChunk> TerrainSplineCompositor::_get_or_create_chunk(
		const Vector2i &p_chunk_pos,
		bool p_allow_create
) {
	if (chunk_buffers.has(p_chunk_pos)) {
		return chunk_buffers[p_chunk_pos];
	}
	if (!p_allow_create) {
		return Ref<TerrainChunk>();
	}
	Ref<TerrainChunk> chunk;
	chunk.instantiate();
	Ref<TerrainHeightmap> buffer;
	buffer.instantiate();
	buffer->initialize(chunk_size, chunk_size, default_elevation);
	chunk->set_heightmap(buffer);
	chunk->set_chunk_coords(p_chunk_pos);
	chunk_buffers[p_chunk_pos] = chunk;
	return chunk;
}

/**
 * @brief Gathers everything one chunk's math needs from the scene tree. Returns null when there is
 * nothing to generate (no spline, no noise, elevation 0). Marks the chunk GENERATING; while a
 * regeneration is in flight the old visuals/physics stay put until finalize replaces them.
 */
Ref<ChunkJob> TerrainSplineCompositor::_make_chunk_job(
		const Vector2i &p_chunk_pos,
		const std::vector<ProceduralSpline3D *> &p_splines
) {
	Ref<ChunkJob> job;
	job.instantiate();
	job->chunk_pos = p_chunk_pos;
	job->chunk_size = chunk_size;
	job->offset = Vector2(p_chunk_pos.x * chunk_size, p_chunk_pos.y * chunk_size);
	job->chunk_rect = Rect2(job->offset, Vector2(chunk_size, chunk_size));
	job->noise = global_terrain_noise;
	job->default_elevation = default_elevation;
	job->noise_amplitude = global_terrain_amplitude;
	job->world_offset_at_dispatch = global_world_offset;

	for (ProceduralSpline3D *spline : p_splines) {
		Rect2 aabb = spline->get_padded_aabb();
		if (!aabb.intersects(job->chunk_rect)) {
			continue;
		}
		spline->ensure_baked_cache(); // Needs the main thread (reads the global transform).
		ChunkJob::SplineEntry entry;
		entry.spline = spline;
		entry.padded_aabb = aabb;
		TypedArray<Node> children = spline->get_children();
		for (int i = 0; i < children.size(); ++i) {
			Node *child = Object::cast_to<Node>(children[i]);
			if (TerrainSplineDeformer *d = Object::cast_to<TerrainSplineDeformer>(child)) {
				entry.deformers.push_back(d);
			} else if (TerrainSplineScatter *sc = Object::cast_to<TerrainSplineScatter>(child)) {
				entry.scatterers.push_back(sc);
			}
		}
		job->splines.push_back(entry);
	}

	bool worth_generating = !job->splines.empty() || global_terrain_noise.is_valid() || default_elevation != 0.0f;
	job->chunk = _get_or_create_chunk(p_chunk_pos, /*allow_create=*/worth_generating);
	if (job->chunk.is_null()) {
		return Ref<ChunkJob>();
	}

	for (ChunkJob::SplineEntry &entry : job->splines) {
		for (TerrainSplineScatter *sc : entry.scatterers) {
			job->scatter_jobs.push_back(
					TerrainSplineScatter::make_scatter_job(job->chunk, entry.spline, sc, entry.padded_aabb, job->offset)
			);
		}
	}

	job->chunk->set_state(TerrainChunk::STATE_GENERATING);
	return job;
}

// ---------------------------------------------------------------------------------------------
// Phase 1: math (any thread)
// ---------------------------------------------------------------------------------------------

/**
 * @brief Fills the chunk heightmap (noise, then each spline deformer) and computes scatter
 * transforms. Pure math; safe on any thread once _make_chunk_job has run on the main thread.
 */
void TerrainSplineCompositor::_run_chunk_job_math(
		const Ref<ChunkJob> &p_job,
		bool p_threaded
) {
	if (p_job.is_null() || p_job->chunk.is_null()) {
		return;
	}
	Ref<TerrainHeightmap> buffer = p_job->chunk->get_heightmap();
	if (buffer.is_null()) {
		return;
	}
	const int cs = p_job->chunk_size;
	uint64_t t0 = Time::get_singleton()->get_ticks_usec();

	buffer->clear(p_job->default_elevation);

	// 1. Base terrain from global noise.
	if (p_job->noise.is_valid()) {
		float *ptr = buffer->get_data_ptrw();
		for (int z = 0; z < cs; ++z) {
			for (int x = 0; x < cs; ++x) {
				float world_x = p_job->offset.x + (float)x;
				float world_z = p_job->offset.y + (float)z;
				ptr[z * cs + x] = p_job->default_elevation +
						p_job->noise->get_noise_2d(world_x, world_z) * p_job->noise_amplitude;
			}
		}
	}

	// 2. Spline deformers on top.
	for (const ChunkJob::SplineEntry &entry : p_job->splines) {
		for (TerrainSplineDeformer *d : entry.deformers) {
			d->deform_heightmap_prepared(buffer, entry.spline, p_job->offset, entry.padded_aabb, p_threaded);
		}
	}
	uint64_t t1 = Time::get_singleton()->get_ticks_usec();

	// 3. Scatter transforms (reads the finished heightmap).
	for (const Ref<ScatterJob> &sj : p_job->scatter_jobs) {
		sj->scatterer->run_scatter_job(sj, cs);
	}
	uint64_t t2 = Time::get_singleton()->get_ticks_usec();

	p_job->math_usec = t1 - t0;
	p_job->scatter_usec = t2 - t1;
}

void TerrainSplineCompositor::_run_chunk_job_task(Ref<ChunkJob> p_job) { _run_chunk_job_math(p_job, false); }

// ---------------------------------------------------------------------------------------------
// Phase 2: finalize (main thread)
// ---------------------------------------------------------------------------------------------

/**
 * @brief Swaps in the new visuals and physics caches and hands the heightmap to Terrain3D. Regions
 * are added with update=false; the caller flushes once per batch. A job whose chunk was evicted
 * while it ran is dropped.
 */
void TerrainSplineCompositor::_finalize_chunk_job(
		const Ref<ChunkJob> &p_job,
		Object *p_target_api
) {
	if (p_job.is_null() || p_job->chunk.is_null() || !p_target_api) {
		return;
	}
	Ref<TerrainChunk> chunk = p_job->chunk;
	if (!chunk_buffers.has(p_job->chunk_pos) || chunk_buffers[p_job->chunk_pos] != chunk) {
		return;
	}

	chunk->release_visuals_and_physics();
	chunk->get_physics_caches().clear();
	for (const Ref<ScatterJob> &sj : p_job->scatter_jobs) {
		TerrainSplineScatter::finalize_scatter_job(sj, scatter_container, this);
	}
	chunk->set_state(TerrainChunk::STATE_VISUAL_ONLY);
	if (_thumbnail_size > 0) {
		chunk->build_thumbnail(_thumbnail_size);
	}
	if (_bench) {
		_bench_dump_chunk(chunk);
	}

	uint64_t t_t3d_start = Time::get_singleton()->get_ticks_usec();
	_write_chunk_heights_to_terrain(p_target_api, chunk, p_job->offset);
	uint64_t t_t3d_end = Time::get_singleton()->get_ticks_usec();

	if (_bench) {
		_bench_math_ms += p_job->math_usec / 1000.0;
		_bench_scatter_ms += p_job->scatter_usec / 1000.0;
		_bench_upload_ms += (t_t3d_end - t_t3d_start) / 1000.0;
	}
#if DEBUG
	UtilityFunctions::print(
			"  -> Chunk [", p_job->chunk_pos.x, ", ", p_job->chunk_pos.y, "] | Math: ", p_job->math_usec / 1000.0,
			" ms | Scatter: ", p_job->scatter_usec / 1000.0, " ms | Terrain3D: ", (t_t3d_end - t_t3d_start) / 1000.0,
			" ms"
	);
#endif
}

// ---------------------------------------------------------------------------------------------
// Synchronous convenience and draining
// ---------------------------------------------------------------------------------------------

/// Editor path: make -> math (tile-threaded) -> finalize per chunk, blocking.
void TerrainSplineCompositor::_generate_chunks(
		const std::vector<Vector2i> &p_chunks,
		const std::vector<ProceduralSpline3D *> &p_splines,
		Object *p_target_api
) {
	for (const Vector2i &chunk_pos : p_chunks) {
		Ref<ChunkJob> job = _make_chunk_job(chunk_pos, p_splines);
		if (job.is_null()) {
			continue;
		}
		_run_chunk_job_math(job, true);
		_finalize_chunk_job(job, p_target_api);
	}
}

/// Blocks until every in-flight job has finished and finalizes them. Used before a spline rebake
/// would invalidate the baked caches the workers are reading.
void TerrainSplineCompositor::_wait_for_jobs_in_flight() {
	if (_jobs_in_flight.empty()) {
		return;
	}
	WorkerThreadPool *wtp = WorkerThreadPool::get_singleton();
	Object *target_api = _get_terrain_data_api();
	for (const Ref<ChunkJob> &job : _jobs_in_flight) {
		if (wtp && job->task_id >= 0) {
			wtp->wait_for_task_completion(job->task_id);
		}
		_finalize_chunk_job(job, target_api);
	}
	_jobs_in_flight.clear();
	_flush_terrain_maps(target_api);
	_refresh_terrain_collision();
}

} // namespace godot
