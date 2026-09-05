/**
 * @file tr_compositor_stream.cpp
 * @brief The time-budgeted generation queue and the asynchronous chunk-job pipeline.
 *
 * Every frame (_drain_generation_queue):
 *  1. finalize in-flight jobs whose math has finished (main-thread work), nearest first, until
 *     generation_budget_ms of the frame is spent - at least one per frame so progress is guaranteed;
 *  2. dispatch new jobs from the queue, nearest first, up to max_jobs_in_flight; their math runs on
 *     WorkerThreadPool threads;
 *  3. upload to Terrain3D and refresh collision, throttled (idle, or every FLUSH_MAX_FRAMES) because
 *     Terrain3D rebuilds its whole texture array per upload.
 * See Terraspline.md steps 1-3 for the measurements behind this design.
 */
#include "tr_compositor.h"
#include <algorithm>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/time.hpp>
#include <godot_cpp/classes/worker_thread_pool.hpp>

namespace godot {

/**
 * @brief Adds chunk coordinates to the generation queue (deduplicated).
 * With p_allow_existing false, chunks that are already resident are skipped (streaming discovery);
 * with true they are regenerated (spline edits / full rebuilds).
 */
void TerrainSplineCompositor::_enqueue_chunks(
		const std::vector<Vector2i> &p_chunks,
		bool p_allow_existing
) {
	for (const Vector2i &c : p_chunks) {
		if (!p_allow_existing && chunk_buffers.has(c)) {
			continue;
		}
		if (std::find(_gen_queue.begin(), _gen_queue.end(), c) == _gen_queue.end()) {
			_gen_queue.push_back(c);
		}
	}
}

void TerrainSplineCompositor::_drain_generation_queue(uint64_t p_frame_start_usec) {
	if ((_gen_queue.empty() && _jobs_in_flight.empty()) || !terrain || !terrain->is_inside_tree()) {
		return;
	}
	Object *target_api = _get_terrain_data_api();
	if (!target_api) {
		return;
	}
	const uint64_t budget_usec = (uint64_t)(generation_budget_ms * 1000.0f);

	_finalize_completed_jobs(target_api, p_frame_start_usec, budget_usec);
	_dispatch_queued_jobs(target_api, p_frame_start_usec, budget_usec);
	_flush_terrain_if_due(target_api);
}

/**
 * @brief Finalizes jobs whose worker task has completed. Returns how many were finalized.
 * A job dispatched before an origin shift carries stale transforms and is re-enqueued instead.
 */
int TerrainSplineCompositor::_finalize_completed_jobs(
		Object *p_target_api,
		uint64_t p_frame_start_usec,
		uint64_t p_budget_usec
) {
	WorkerThreadPool *wtp = WorkerThreadPool::get_singleton();
	int finalized = 0;
	for (size_t i = 0; i < _jobs_in_flight.size();) {
		Ref<ChunkJob> job = _jobs_in_flight[i];
		bool done = !wtp || job->task_id < 0 || wtp->is_task_completed(job->task_id);
		bool over_budget = Time::get_singleton()->get_ticks_usec() - p_frame_start_usec >= p_budget_usec;
		if (!done || (finalized > 0 && over_budget)) {
			++i;
			continue;
		}
		if (wtp && job->task_id >= 0) {
			wtp->wait_for_task_completion(job->task_id); // Already complete; releases the task.
		}
		if (job->world_offset_at_dispatch != global_world_offset) {
			_enqueue_chunks({ job->chunk_pos }, /*allow_existing=*/true);
		} else {
			uint64_t tf0 = Time::get_singleton()->get_ticks_usec();
			_finalize_chunk_job(job, p_target_api);
			if (_bench) {
				_bench_finalize_ms += (Time::get_singleton()->get_ticks_usec() - tf0) / 1000.0;
			}
			finalized++;
		}
		_jobs_in_flight.erase(_jobs_in_flight.begin() + i);
	}
	return finalized;
}

/**
 * @brief Pops chunks off the queue (nearest first) and starts their math on worker threads until the
 * in-flight limit or the frame budget is reached. Without a thread pool the chunk is generated
 * synchronously. Returns how many chunks were finalized synchronously (normally 0).
 */
int TerrainSplineCompositor::_dispatch_queued_jobs(
		Object *p_target_api,
		uint64_t p_frame_start_usec,
		uint64_t p_budget_usec
) {
	if (_gen_queue.empty()) {
		return 0;
	}
	_sort_queue_nearest_first();

	WorkerThreadPool *wtp = WorkerThreadPool::get_singleton();
	std::vector<ProceduralSpline3D *> splines = _gather_splines();
	const int max_in_flight = _effective_max_jobs_in_flight();
	int finalized = 0;

	while (!_gen_queue.empty() && (int)_jobs_in_flight.size() < max_in_flight) {
		if (Time::get_singleton()->get_ticks_usec() - p_frame_start_usec >= p_budget_usec) {
			break;
		}
		Vector2i c = _gen_queue.back();
		_gen_queue.pop_back();

		uint64_t tm0 = Time::get_singleton()->get_ticks_usec();
		Ref<ChunkJob> job = _make_chunk_job(c, splines);
		if (_bench) {
			_bench_make_ms += (Time::get_singleton()->get_ticks_usec() - tm0) / 1000.0;
		}
		if (job.is_null()) {
			continue;
		}
		if (wtp) {
			job->task_id = wtp->add_task(Callable(this, "_run_chunk_job_task").bind(job), false, "TerraSpline_Chunk");
			_jobs_in_flight.push_back(job);
		} else {
			_run_chunk_job_math(job, true);
			_finalize_chunk_job(job, p_target_api);
			finalized++;
		}
	}
	return finalized;
}

/// Sorts so that pop_back() yields the chunk nearest to the player.
void TerrainSplineCompositor::_sort_queue_nearest_first() {
	Vector2 logical_player_pos_2d = _logical_player_position_2d();
	auto chunk_dist = [&](const Vector2i &c) {
		Vector2 center = Vector2((c.x + 0.5f) * chunk_size, (c.y + 0.5f) * chunk_size) + global_world_offset;
		return logical_player_pos_2d.distance_squared_to(center);
	};
	std::sort(_gen_queue.begin(), _gen_queue.end(), [&](const Vector2i &a, const Vector2i &b) {
		return chunk_dist(a) > chunk_dist(b); // farthest first
	});
}

int TerrainSplineCompositor::_effective_max_jobs_in_flight() const {
	if (max_jobs_in_flight > 0) {
		return max_jobs_in_flight;
	}
	return MAX(1, OS::get_singleton()->get_processor_count() - 1);
}

/**
 * @brief Throttled upload. Terrain3D's update_maps rebuilds the whole texture array (~7 ms at
 * 25 regions), so pending regions are flushed when the pipeline goes idle or at most every
 * FLUSH_MAX_FRAMES frames while chunks keep streaming in.
 */
void TerrainSplineCompositor::_flush_terrain_if_due(Object *p_target_api) {
	if (!_terrain_maps_dirty) {
		return;
	}
	_frames_since_flush++;
	bool idle = _gen_queue.empty() && _jobs_in_flight.empty();
	if (idle || _frames_since_flush >= FLUSH_MAX_FRAMES) {
		_flush_terrain_maps(p_target_api);
		_refresh_terrain_collision();
	}
}

} // namespace godot
