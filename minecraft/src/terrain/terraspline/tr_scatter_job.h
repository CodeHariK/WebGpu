/**
 * @file tr_scatter_job.h
 * @brief ScatterJob: inputs and results for scattering one scatterer over one chunk.
 */
#ifndef TR_SCATTER_JOB_H
#define TR_SCATTER_JOB_H

#include "tr_chunk.h"
#include <cstdint>
#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/rect2.hpp>
#include <godot_cpp/variant/transform3d.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <vector>

namespace godot {

class ProceduralSpline3D;
class TerrainSplineScatter;

/**
 * @class ScatterJob
 * @brief One (scatterer, spline, chunk) scattering task.
 *
 * Created on the main thread (TerrainSplineScatter::make_scatter_job), run on any thread
 * (run_scatter_job writes only `transforms` and the debug counters), finalized on the main thread
 * (finalize_scatter_job creates the MultiMeshInstance3D and records the physics cache).
 */
class ScatterJob : public RefCounted {
	GDCLASS(ScatterJob,
			RefCounted)

public:
	// ---- Inputs ----
	Ref<TerrainChunk> chunk;
	ProceduralSpline3D *spline = nullptr;
	TerrainSplineScatter *scatterer = nullptr;
	Vector2 offset; // World XZ of the chunk's pixel (0,0)
	Rect2 spline_bounds; // Spline's padded AABB (main-thread value, cached)
	uint64_t scatterer_seed = 0; // Stable per-scatterer hash so overlapping scatterers don't stack

	// ---- Output ----
	std::vector<Transform3D> transforms;

	// ---- Debug statistics ----
	int debug_total_cells = 0;
	int debug_density_skipped = 0;
	int debug_noise_skipped = 0;
	int debug_spline_skipped = 0;
	int debug_slope_skipped = 0;
	uint64_t debug_time_spent_usec = 0;

	ScatterJob() {}
	~ScatterJob() {}

protected:
	static void _bind_methods() {}
};

} // namespace godot

#endif // TR_SCATTER_JOB_H
