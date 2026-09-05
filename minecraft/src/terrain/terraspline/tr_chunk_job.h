/**
 * @file tr_chunk_job.h
 * @brief ChunkJob: everything one chunk generation needs, gathered on the main thread.
 */
#ifndef TR_CHUNK_JOB_H
#define TR_CHUNK_JOB_H

#include "tr_chunk.h"
#include "tr_scatter_job.h"
#include <cstdint>
#include <godot_cpp/classes/noise.hpp>
#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/rect2.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/vector2i.hpp>
#include <vector>

namespace godot {

class ProceduralSpline3D;
class TerrainSplineDeformer;
class TerrainSplineScatter;

/**
 * @class ChunkJob
 * @brief Inputs, intermediate results and bookkeeping for generating one chunk.
 *
 * Phase 1 (`TerrainSplineCompositor::_run_chunk_job_math`, any thread): noise fill, spline
 * deformers, scatter transforms. Phase 2 (`_finalize_chunk_job`, main thread): MultiMesh nodes,
 * physics caches, Terrain3D region. Everything that touches the scene tree is captured here by
 * `_make_chunk_job` so phase 1 never has to.
 */
class ChunkJob : public RefCounted {
	GDCLASS(ChunkJob,
			RefCounted)

public:
	/// A spline whose padded AABB intersects the chunk, with its components resolved.
	struct SplineEntry {
		ProceduralSpline3D *spline = nullptr;
		Rect2 padded_aabb;
		std::vector<TerrainSplineDeformer *> deformers;
		std::vector<TerrainSplineScatter *> scatterers;
	};

	// ---- Inputs ----
	Vector2i chunk_pos;
	Vector2 offset; // World XZ of pixel (0,0)
	Rect2 chunk_rect;
	int chunk_size = 0;
	Ref<TerrainChunk> chunk;
	std::vector<SplineEntry> splines;
	Ref<Noise> noise;
	float default_elevation = 0.0f;
	float noise_amplitude = 0.0f;
	Vector2 world_offset_at_dispatch; // Detects an origin shift while the job was in flight

	// ---- Outputs ----
	std::vector<Ref<ScatterJob>> scatter_jobs;
	uint64_t math_usec = 0;
	uint64_t scatter_usec = 0;

	// ---- Async bookkeeping ----
	int task_id = -1; // WorkerThreadPool task, -1 when run synchronously

	ChunkJob() {}
	~ChunkJob() {}

protected:
	static void _bind_methods() {}
};

} // namespace godot

#endif // TR_CHUNK_JOB_H
