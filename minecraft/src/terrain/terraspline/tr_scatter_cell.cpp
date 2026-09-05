/**
 * @file tr_scatter_cell.cpp
 * @brief Per-cell scatter evaluation: deterministic RNG, spawn filters, terrain height and slope.
 *
 * Runs on worker threads; reads only the job, the baked spline cache and the chunk heightmap.
 */
#include "tr_scatter.h"

namespace godot {

static inline float local_lerp(
		float a,
		float b,
		float t
) {
	return a + t * (b - a);
}

/// Small PCG-style generator; one per cell, seeded from the cell's coordinates so results are
/// reproducible regardless of thread scheduling.
struct CellRNG {
	uint64_t state;
	explicit CellRNG(uint64_t seed) : state(seed) {}
	uint32_t next() {
		state = state * 6364136223846793005ULL + 1442695040888963407ULL;
		return uint32_t(state >> 32);
	}
	float next_float() { return float(next()) / 4294967296.0f; }
	float next_float_range(
			float min_val,
			float max_val
	) {
		return min_val + next_float() * (max_val - min_val);
	}
};

/**
 * @brief Bilinear height and slope at a chunk-local position.
 * The normal is derived from the bilinear surface's partial derivatives; slope is its angle from up.
 */
bool TerrainSplineScatter::_evaluate_height_and_slope(
		const float *heightmap_data,
		int p_chunk_size,
		float local_x,
		float local_z,
		float &r_height,
		Vector3 &r_normal,
		float &r_slope_deg
) const {
	int lx = (int)local_x;
	int lz = (int)local_z;
	int lx_min = Math::clamp(lx, 0, p_chunk_size - 1);
	int lx_max = Math::clamp(lx + 1, 0, p_chunk_size - 1);
	int lz_min = Math::clamp(lz, 0, p_chunk_size - 1);
	int lz_max = Math::clamp(lz + 1, 0, p_chunk_size - 1);

	float fx = local_x - lx;
	float fz = local_z - lz;

	float h00 = heightmap_data[lz_min * p_chunk_size + lx_min];
	float h10 = heightmap_data[lz_min * p_chunk_size + lx_max];
	float h01 = heightmap_data[lz_max * p_chunk_size + lx_min];
	float h11 = heightmap_data[lz_max * p_chunk_size + lx_max];

	r_height = local_lerp(local_lerp(h00, h10, fx), local_lerp(h01, h11, fx), fz);
	float dx = local_lerp(h10 - h00, h11 - h01, fz);
	float dz = local_lerp(h01 - h00, h11 - h10, fx);

	r_normal = Vector3(-dx, 1.0f, -dz);
	r_normal.normalize();
	r_slope_deg = Math::rad_to_deg(Math::acos(r_normal.y));
	return true;
}

/**
 * @brief Decides whether cell (cx, cz) spawns an instance and, if so, where.
 * Filters in order: density roll, biome noise, distance to the spline corridor, terrain slope.
 * The RNG draw order is part of the output (it defines every instance's position) - do not reorder.
 */
bool TerrainSplineScatter::_process_scatter_cell(
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
) {
	Vector2i chunk_pos = p_job->chunk->get_chunk_coords();
	// scatterer_seed distinguishes scatterers sharing a cell (same spline, or overlapping corridors);
	// without it they draw identical positions and stack meshes on top of each other.
	uint64_t cell_seed = base_seed ^ (uint64_t(chunk_pos.x) * 73856093ULL) ^ (uint64_t(chunk_pos.y) * 19349663ULL) ^
			(uint64_t(cx) * 83492791ULL) ^ (uint64_t(cz) * 37476139ULL) ^ uint64_t(seed_offset) ^ p_job->scatterer_seed;
	CellRNG rng(cell_seed);

	if (rng.next_float() > density) {
		p_job->debug_density_skipped++;
		return false;
	}

	float cell_min_x = offset.x + cx * spacing;
	float cell_min_z = offset.y + cz * spacing;
	float random_x = cell_min_x + rng.next_float() * spacing;
	float random_z = cell_min_z + rng.next_float() * spacing;

	if (biome_noise.is_valid()) {
		float noise_val = (biome_noise->get_noise_2d(random_x, random_z) + 1.0f) * 0.5f;
		if (noise_val < biome_noise_threshold || rng.next_float() > noise_val) {
			p_job->debug_noise_skipped++;
			return false;
		}
	}

	Vector2 test_point(random_x, random_z);
	ProceduralSpline3D::SplineEval eval = p_job->spline->evaluate_spline_point_segmented(test_point, active_segments);
	if (eval.distance < min_spline_dist || eval.distance > max_spline_dist) {
		p_job->debug_spline_skipped++;
		return false;
	}

	float local_x = CLAMP(random_x - offset.x, 0.0f, (float)p_chunk_size - 1.0001f);
	float local_z = CLAMP(random_z - offset.y, 0.0f, (float)p_chunk_size - 1.0001f);
	float exact_y = 0.0f;
	Vector3 normal;
	float theta_deg = 0.0f;
	_evaluate_height_and_slope(heightmap_data, p_chunk_size, local_x, local_z, exact_y, normal, theta_deg);
	if (theta_deg < min_slope || theta_deg > max_slope) {
		p_job->debug_slope_skipped++;
		return false;
	}

	float rand_yaw = rng.next_float_range(0.0f, Math::TAU);
	float rand_scale = rng.next_float_range(scale_min, scale_max);
	r_transform.origin = Vector3(random_x, exact_y, random_z);
	r_transform.basis = Basis();
	r_transform.basis.rotate(Vector3(0, 1, 0), rand_yaw);
	r_transform.basis.scale(Vector3(1, 1, 1) * rand_scale);
	return true;
}

} // namespace godot
