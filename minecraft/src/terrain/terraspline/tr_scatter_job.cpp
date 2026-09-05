/**
 * @file tr_scatter_job.cpp
 * @brief ScatterJob lifecycle: make (main thread) -> run (any thread) -> finalize (main thread).
 */
#include "tr_scatter.h"
#include <godot_cpp/classes/multi_mesh.hpp>
#include <godot_cpp/classes/multi_mesh_instance3d.hpp>
#include <godot_cpp/classes/time.hpp>
#include <godot_cpp/classes/worker_thread_pool.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

static constexpr uint64_t SCATTER_BASE_SEED = 1234567ULL;

// ---------------------------------------------------------------------------------------------
// Make (main thread)
// ---------------------------------------------------------------------------------------------

Ref<ScatterJob> TerrainSplineScatter::make_scatter_job(
		const Ref<TerrainChunk> &p_chunk,
		ProceduralSpline3D *p_spline,
		TerrainSplineScatter *p_scatterer,
		const Rect2 &p_spline_padded_aabb,
		const Vector2 &p_offset
) {
	Ref<ScatterJob> job;
	job.instantiate();
	job->chunk = p_chunk;
	job->spline = p_spline;
	job->scatterer = p_scatterer;
	job->offset = p_offset;
	job->spline_bounds = p_spline_padded_aabb;
	// Stable across runs (unlike instance ids) so a chunk always regrows the same rocks.
	job->scatterer_seed = uint64_t(String(p_scatterer->get_path()).hash()) * 0x9E3779B97F4A7C15ULL;
	return job;
}

// ---------------------------------------------------------------------------------------------
// Run (any thread)
// ---------------------------------------------------------------------------------------------

/**
 * @brief Evaluates every cell of the chunk that lies within the spline's corridor and stores the
 * resulting instance transforms in the job.
 */
void TerrainSplineScatter::run_scatter_job(
		const Ref<ScatterJob> &p_job,
		int p_chunk_size
) {
	if (p_job.is_null() || p_job->chunk.is_null() || p_job->spline == nullptr || p_job->scatterer == nullptr) {
		return;
	}
#if DEBUG
	uint64_t t_start = Time::get_singleton()->get_ticks_usec();
#endif
	TerrainSplineScatter *scatterer = p_job->scatterer;
	ProceduralSpline3D *spline = p_job->spline;
	Vector2 offset = p_job->offset;

	float spacing = scatterer->get_spacing();
	float density = scatterer->get_density();
	if (density <= 0.0f || spacing <= 0.1f) {
		return;
	}
	Ref<TerrainHeightmap> buffer = p_job->chunk->get_heightmap();
	if (buffer.is_null()) {
		return;
	}
	float *heightmap_data = buffer->get_data_ptrw();
	if (!heightmap_data) {
		return;
	}
	int num_cells = (int)Math::floor(p_chunk_size / spacing);
	if (num_cells <= 0) {
		return;
	}

	// Only the part of the chunk the corridor can reach.
	float max_dist = scatterer->get_max_spline_dist();
	Rect2 chunk_rect(offset, Vector2(p_chunk_size, p_chunk_size));
	Rect2 active_area = p_job->spline_bounds.grow(max_dist).intersection(chunk_rect);
	if (!active_area.has_area()) {
		return;
	}

	// Segments whose padded bounds touch the active area.
	std::vector<int> active_segments;
	for (size_t i = 0; i < spline->baked_segments.size(); ++i) {
		const ProceduralSpline3D::BakedSegment &seg = spline->baked_segments[i];
		Rect2 seg_aabb(seg.a, Vector2());
		seg_aabb = seg_aabb.expand(seg.b).grow(max_dist);
		if (seg_aabb.intersects(active_area)) {
			active_segments.push_back((int)i);
		}
	}
	if (active_segments.empty()) {
		return;
	}

	int min_cx = Math::max(0, (int)Math::floor((active_area.position.x - offset.x) / spacing));
	int max_cx = Math::min(
			num_cells - 1, (int)Math::ceil((active_area.position.x + active_area.size.x - offset.x) / spacing)
	);
	int min_cz = Math::max(0, (int)Math::floor((active_area.position.y - offset.y) / spacing));
	int max_cz = Math::min(
			num_cells - 1, (int)Math::ceil((active_area.position.y + active_area.size.y - offset.y) / spacing)
	);

	p_job->debug_total_cells = (max_cx - min_cx + 1) * (max_cz - min_cz + 1);
	p_job->debug_density_skipped = 0;
	p_job->debug_noise_skipped = 0;
	p_job->debug_spline_skipped = 0;
	p_job->debug_slope_skipped = 0;

	std::vector<Transform3D> transforms;
	transforms.reserve(p_job->debug_total_cells);
	for (int cz = min_cz; cz <= max_cz; ++cz) {
		for (int cx = min_cx; cx <= max_cx; ++cx) {
			Transform3D t;
			if (_process_scatter_cell(
						p_job, cx, cz, SCATTER_BASE_SEED, offset, p_chunk_size, spacing, density, heightmap_data,
						active_segments, t
				)) {
				transforms.push_back(t);
			}
		}
	}
	transforms.shrink_to_fit();
	p_job->transforms = transforms;
#if DEBUG
	p_job->debug_time_spent_usec = Time::get_singleton()->get_ticks_usec() - t_start;
#endif
}

// ---------------------------------------------------------------------------------------------
// Finalize (main thread)
// ---------------------------------------------------------------------------------------------

void TerrainSplineScatter::_finalize_scatter_job(
		const Ref<ScatterJob> &p_job,
		Node3D *p_scatter_container,
		Node *p_owner_node
) {
#if DEBUG
	_print_scatter_debug(p_job);
#endif
	if (p_job->transforms.empty()) {
		return;
	}
	_build_multimesh(p_job, p_scatter_container, p_owner_node);
	_record_physics_cache(p_job);
}

void TerrainSplineScatter::_print_scatter_debug(const Ref<ScatterJob> &p_job) {
	int spawned = (int)p_job->transforms.size();
	float time_ms = p_job->debug_time_spent_usec / 1000.0f;
	float spawned_pct = p_job->debug_total_cells > 0 ? (float)spawned / p_job->debug_total_cells * 100.0f : 0.0f;
	UtilityFunctions::print(
			"    [Scatterer: ", p_job->scatterer->get_name(), "] Spawned: ", spawned, " / ", p_job->debug_total_cells,
			" cells (", spawned_pct, "%) | Time: ", time_ms, " ms"
	);
	if (p_job->debug_total_cells > 0) {
		float dens_pct = (float)p_job->debug_density_skipped / p_job->debug_total_cells * 100.0f;
		float noise_pct = (float)p_job->debug_noise_skipped / p_job->debug_total_cells * 100.0f;
		float spline_pct = (float)p_job->debug_spline_skipped / p_job->debug_total_cells * 100.0f;
		float slope_pct = (float)p_job->debug_slope_skipped / p_job->debug_total_cells * 100.0f;
		UtilityFunctions::print(
				"      └─ Culled -> Density: ", p_job->debug_density_skipped, " (", dens_pct,
				"%) | Noise: ", p_job->debug_noise_skipped, " (", noise_pct,
				"%) | Spline Corridor: ", p_job->debug_spline_skipped, " (", spline_pct,
				"%) | Slope: ", p_job->debug_slope_skipped, " (", slope_pct, "%)"
		);
	}
}

/// One MultiMeshInstance3D with every transform, parented under the scatter container (or owner).
void TerrainSplineScatter::_build_multimesh(
		const Ref<ScatterJob> &p_job,
		Node3D *p_scatter_container,
		Node *p_owner_node
) {
	const size_t count = p_job->transforms.size();
	Ref<MultiMesh> mm;
	mm.instantiate();
	mm->set_transform_format(MultiMesh::TRANSFORM_3D);
	mm->set_instance_count(count);
	mm->set_mesh(p_job->scatterer->get_mesh());

	// MultiMesh buffer layout: 12 floats per instance, rows of the 3x4 transform.
	PackedFloat32Array buffer_array;
	buffer_array.resize(count * 12);
	float *ptr = buffer_array.ptrw();
	for (size_t i = 0; i < count; ++i) {
		const Transform3D &t = p_job->transforms[i];
		int base = i * 12;
		ptr[base + 0] = t.basis[0][0];
		ptr[base + 1] = t.basis[0][1];
		ptr[base + 2] = t.basis[0][2];
		ptr[base + 3] = t.origin.x;
		ptr[base + 4] = t.basis[1][0];
		ptr[base + 5] = t.basis[1][1];
		ptr[base + 6] = t.basis[1][2];
		ptr[base + 7] = t.origin.y;
		ptr[base + 8] = t.basis[2][0];
		ptr[base + 9] = t.basis[2][1];
		ptr[base + 10] = t.basis[2][2];
		ptr[base + 11] = t.origin.z;
	}
	mm->set_buffer(buffer_array);

	MultiMeshInstance3D *mmi = memnew(MultiMeshInstance3D);
	mmi->set_multimesh(mm);
	if (p_scatter_container) {
		p_scatter_container->add_child(mmi);
	} else if (p_owner_node) {
		p_owner_node->add_child(mmi);
	}
	p_job->chunk->get_visual_nodes().push_back(mmi->get_instance_id());
}

/// Records the collision shape + transforms so the compositor can wake physics later.
void TerrainSplineScatter::_record_physics_cache(const Ref<ScatterJob> &p_job) {
	Ref<Shape3D> shape = p_job->scatterer->get_collision_shape();
	if (!shape.is_valid()) {
		return;
	}
	TerrainChunk::PhysicsCache pc;
	pc.shape_rid = shape->get_rid();
	pc.transforms = p_job->transforms;
	p_job->chunk->get_physics_caches().push_back(pc);
}

// ---------------------------------------------------------------------------------------------
// Synchronous convenience
// ---------------------------------------------------------------------------------------------

void TerrainSplineScatter::_dispatch_scatter_jobs(
		const Ref<TerrainChunk> &p_chunk,
		const std::vector<ProceduralSpline3D *> &p_splines,
		const Rect2 &p_chunk_rect,
		const Vector2 &p_offset,
		int p_chunk_size,
		std::vector<Ref<ScatterJob>> &r_jobs,
		std::vector<int> &r_task_ids
) {
	WorkerThreadPool *wtp = WorkerThreadPool::get_singleton();
	for (ProceduralSpline3D *spline : p_splines) {
		Rect2 aabb = spline->get_padded_aabb();
		if (!aabb.intersects(p_chunk_rect)) {
			continue;
		}
		spline->ensure_baked_cache();
		TypedArray<Node> spline_children = spline->get_children();
		for (int i = 0; i < spline_children.size(); ++i) {
			TerrainSplineScatter *scatterer = Object::cast_to<TerrainSplineScatter>(spline_children[i]);
			if (!scatterer) {
				continue;
			}
			Ref<ScatterJob> job = make_scatter_job(p_chunk, spline, scatterer, aabb, p_offset);
			r_jobs.push_back(job);
			if (wtp) {
				Callable callable = Callable(scatterer, "run_scatter_job").bind(job, p_chunk_size);
				r_task_ids.push_back(wtp->add_task(callable, "TerrainScatterJob"));
			} else {
				scatterer->run_scatter_job(job, p_chunk_size);
			}
		}
	}
}

void TerrainSplineScatter::scatter_chunk(
		const Ref<TerrainChunk> &p_chunk,
		const std::vector<ProceduralSpline3D *> &p_splines,
		const Rect2 &p_chunk_rect,
		const Vector2 &p_offset,
		int p_chunk_size,
		Node3D *p_scatter_container,
		Node *p_owner_node
) {
	std::vector<Ref<ScatterJob>> jobs;
	std::vector<int> task_ids;
	_dispatch_scatter_jobs(p_chunk, p_splines, p_chunk_rect, p_offset, p_chunk_size, jobs, task_ids);

	WorkerThreadPool *wtp = WorkerThreadPool::get_singleton();
	if (wtp) {
		for (int task_id : task_ids) {
			wtp->wait_for_task_completion(task_id);
		}
	}
	for (const Ref<ScatterJob> &job : jobs) {
		_finalize_scatter_job(job, p_scatter_container, p_owner_node);
	}
}

} // namespace godot
