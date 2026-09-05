/**
 * @file tr_scatter.h
 * @brief TerrainSplineScatter: places mesh instances in a corridor along its parent spline.
 */
#ifndef TR_SCATTER_H
#define TR_SCATTER_H

#include "tr_chunk.h"
#include "tr_scatter_job.h"
#include "utils/spline3d/procedural_spline3d.h"
#include <godot_cpp/classes/mesh.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/noise.hpp>
#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/classes/shape3d.hpp>
#include <godot_cpp/variant/rect2.hpp>
#include <godot_cpp/variant/transform3d.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <vector>

namespace godot {

/**
 * @class TerrainSplineScatter
 * @brief SplineComponent that scatters a mesh (and optional collision shape) near its parent spline.
 *
 * The chunk is divided into `spacing`-sized cells; each cell rolls a deterministic RNG seeded from
 * its coordinates (so a chunk always regrows the same instances), then is filtered by density,
 * biome noise, distance to the spline and terrain slope. Survivors become one instance each.
 *
 * Implementation is split across:
 *  - tr_scatter.cpp       bindings
 *  - tr_scatter_cell.cpp  per-cell evaluation (RNG, filters, height/slope sampling)
 *  - tr_scatter_job.cpp   job creation, execution and MultiMesh finalization
 */
class TerrainSplineScatter : public SplineComponent {
	GDCLASS(TerrainSplineScatter,
			SplineComponent)

private:
	// ---- What to place ----
	Ref<Mesh> mesh;
	Ref<Shape3D> collision_shape; // Optional; instances get static collision when set

	// ---- Placement ----
	float density = 0.1f; // Probability a cell spawns an instance
	float spacing = 4.0f; // Cell size in metres
	float scale_min = 0.8f;
	float scale_max = 1.2f;
	float min_slope = 0.0f; // Degrees
	float max_slope = 35.0f;
	float min_spline_dist = 2.0f; // Metres from the spline
	float max_spline_dist = 30.0f;
	uint32_t seed_offset = 0;
	Ref<Noise> biome_noise; // Optional extra spawn mask
	float biome_noise_threshold = 0.0f;

	// ---- Per-cell evaluation (tr_scatter_cell.cpp) ----
	bool _process_scatter_cell(
			const Ref<ScatterJob> &p_job,
			int cx,
			int cz,
			uint64_t base_seed,
			const Vector2 &offset,
			int p_chunk_size,
			float spacing,
			float density,
			const float *heightmap_data,
			const std::vector<int> &active_segments,
			Transform3D &r_transform
	);
	bool _evaluate_height_and_slope(
			const float *heightmap_data,
			int p_chunk_size,
			float local_x,
			float local_z,
			float &r_height,
			Vector3 &r_normal,
			float &r_slope_deg
	) const;

	// ---- Job helpers (tr_scatter_job.cpp) ----
	static void _dispatch_scatter_jobs(
			const Ref<TerrainChunk> &p_chunk,
			const std::vector<ProceduralSpline3D *> &p_splines,
			const Rect2 &p_chunk_rect,
			const Vector2 &p_offset,
			int p_chunk_size,
			std::vector<Ref<ScatterJob>> &r_jobs,
			std::vector<int> &r_task_ids
	);
	static void _finalize_scatter_job(
			const Ref<ScatterJob> &p_job,
			Node3D *p_scatter_container,
			Node *p_owner_node
	);
	static void _print_scatter_debug(const Ref<ScatterJob> &p_job);
	static void _build_multimesh(
			const Ref<ScatterJob> &p_job,
			Node3D *p_scatter_container,
			Node *p_owner_node
	);
	static void _record_physics_cache(const Ref<ScatterJob> &p_job);

protected:
	static void _bind_methods();

public:
	TerrainSplineScatter();
	~TerrainSplineScatter();

	// Three-phase API used by the compositor's chunk jobs (Terraspline.md step 3):
	//   make_scatter_job     - main thread (reads the scene tree)
	//   run_scatter_job      - any thread (pure math on the baked spline cache and chunk heightmap)
	//   finalize_scatter_job - main thread (creates MultiMeshInstance3D, records physics caches)
	static Ref<ScatterJob> make_scatter_job(
			const Ref<TerrainChunk> &p_chunk,
			ProceduralSpline3D *p_spline,
			TerrainSplineScatter *p_scatterer,
			const Rect2 &p_spline_padded_aabb,
			const Vector2 &p_offset
	);
	void run_scatter_job(
			const Ref<ScatterJob> &p_job,
			int p_chunk_size
	);
	static void finalize_scatter_job(
			const Ref<ScatterJob> &p_job,
			Node3D *p_scatter_container,
			Node *p_owner_node
	) {
		_finalize_scatter_job(p_job, p_scatter_container, p_owner_node);
	}

	/// Synchronous convenience: make + run (threaded) + finalize for every scatterer touching a chunk.
	static void scatter_chunk(
			const Ref<TerrainChunk> &p_chunk,
			const std::vector<ProceduralSpline3D *> &p_splines,
			const Rect2 &p_chunk_rect,
			const Vector2 &p_offset,
			int p_chunk_size,
			Node3D *p_scatter_container,
			Node *p_owner_node
	);

	/// How far beyond the spline this component places instances (used for chunk culling).
	float get_spline_padding() const override { return max_spline_dist; }

	void set_mesh(const Ref<Mesh> &p_mesh) { mesh = p_mesh; }
	Ref<Mesh> get_mesh() const { return mesh; }
	void set_collision_shape(const Ref<Shape3D> &p_shape) { collision_shape = p_shape; }
	Ref<Shape3D> get_collision_shape() const { return collision_shape; }
	void set_density(float p_density) { density = CLAMP(p_density, 0.0f, 1.0f); }
	float get_density() const { return density; }
	void set_spacing(float p_spacing) { spacing = MAX(0.5f, p_spacing); }
	float get_spacing() const { return spacing; }
	void set_scale_min(float p_min) { scale_min = p_min; }
	float get_scale_min() const { return scale_min; }
	void set_scale_max(float p_max) { scale_max = p_max; }
	float get_scale_max() const { return scale_max; }
	void set_min_slope(float p_min_slope) { min_slope = p_min_slope; }
	float get_min_slope() const { return min_slope; }
	void set_max_slope(float p_max_slope) { max_slope = p_max_slope; }
	float get_max_slope() const { return max_slope; }
	void set_min_spline_dist(float p_dist) { min_spline_dist = p_dist; }
	float get_min_spline_dist() const { return min_spline_dist; }
	void set_max_spline_dist(float p_dist) { max_spline_dist = p_dist; }
	float get_max_spline_dist() const { return max_spline_dist; }
	void set_seed_offset(int p_seed_offset) { seed_offset = p_seed_offset; }
	int get_seed_offset() const { return seed_offset; }
	void set_biome_noise(const Ref<Noise> &p_noise) { biome_noise = p_noise; }
	Ref<Noise> get_biome_noise() const { return biome_noise; }
	void set_biome_noise_threshold(float p_threshold) { biome_noise_threshold = p_threshold; }
	float get_biome_noise_threshold() const { return biome_noise_threshold; }
};

} // namespace godot

#endif // TR_SCATTER_H
