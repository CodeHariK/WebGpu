/*
 * Module Path: src/terrain/terraspline/tr_scatter.h
 * Explicit System Responsibility: Defines the TerrainSplineScatter and ScatterJob classes
 * which manage procedural spawning/scattering of meshes and collision physics bodies along 3D splines.
 * Build Dependencies: godot_cpp/classes/texture_rect.hpp, utils/spline3d/procedural_spline3d.h, Godot APIs, std::vector.
 */

#ifndef TR_SCATTER_H
#define TR_SCATTER_H

#include "godot_cpp/classes/texture_rect.hpp"
#include "utils/spline3d/procedural_spline3d.h"
#include <godot_cpp/classes/curve.hpp>
#include <godot_cpp/classes/curve3d.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/mesh.hpp>
#include <godot_cpp/classes/multi_mesh.hpp>
#include <godot_cpp/classes/multi_mesh_instance3d.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/noise.hpp>
#include <godot_cpp/classes/path3d.hpp>
#include <godot_cpp/classes/physics_server3d.hpp>
#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/classes/shape3d.hpp>
#include <godot_cpp/classes/world3d.hpp>
#include <godot_cpp/templates/hash_map.hpp>
#include <godot_cpp/variant/packed_float32_array.hpp>
#include <godot_cpp/variant/packed_vector2_array.hpp>
#include <godot_cpp/variant/packed_vector3_array.hpp>
#include <godot_cpp/variant/rect2.hpp>
#include <godot_cpp/variant/rect2i.hpp>
#include <godot_cpp/variant/transform3d.hpp>
#include <godot_cpp/variant/vector2i.hpp>
#include <vector>

namespace godot {

class ProceduralSpline3D;
class TerrainSplineScatter;
class TerrainChunk;

class ScatterJob : public RefCounted {
	GDCLASS(ScatterJob, RefCounted)

public:
	// Target chunk where meshes are scattered.
	Ref<TerrainChunk> chunk;
	// Pointer to the source spline used for distance checks.
	ProceduralSpline3D *spline = nullptr;
	// Pointer to the parent scatterer component defining parameters.
	TerrainSplineScatter *scatterer = nullptr;
	// Global coordinate offset of the target chunk.
	Vector2 offset;
	// Array storing generated instance transforms.
	std::vector<Transform3D> transforms;
	// Cache of the spline's global bounding box.
	Rect2 spline_bounds;

	// Total candidate cells evaluated during scattering.
	int debug_total_cells = 0;
	// Number of instances skipped due to density probability check.
	int debug_density_skipped = 0;
	// Number of instances skipped due to noise threshold test.
	int debug_noise_skipped = 0;
	// Number of instances skipped because they fell outside the spline corridor.
	int debug_spline_skipped = 0;
	// Number of instances skipped due to slope threshold tests.
	int debug_slope_skipped = 0;
	// Elapsed processing time in microseconds.
	uint64_t debug_time_spent_usec = 0;

	/*
	 * Purpose: Construct the ScatterJob data envelope.
	 * Execution steps: None.
	 * Parameters: None.
	 * Behavioral bounds: Initializes job to default empty state.
	 */
	ScatterJob() {}

	/*
	 * Purpose: Destruct the ScatterJob data envelope.
	 * Execution steps: None.
	 * Parameters: None.
	 * Behavioral bounds: Safely releases resources.
	 */
	~ScatterJob() {}

protected:
	static void _bind_methods() {}
};

class TerrainSplineScatter : public SplineComponent {
	GDCLASS(TerrainSplineScatter, SplineComponent)

private:
	// The mesh resource to be instantiated.
	Ref<Mesh> mesh;
	// Collision shape resource used for physics instances.
	Ref<Shape3D> collision_shape;
	// Density multiplier for spawning likelihood (0.0 to 1.0).
	float density = 0.1f;
	// Grid cell spacing between candidate spawn points.
	float spacing = 4.0f;
	// Minimum scale multiplier applied to spawned instances.
	float scale_min = 0.8f;
	// Maximum scale multiplier applied to spawned instances.
	float scale_max = 1.2f;
	// Minimum allowed surface slope angle in degrees.
	float min_slope = 0.0f;
	// Maximum allowed surface slope angle in degrees.
	float max_slope = 35.0f;
	// Minimum distance from spline centerline to allow spawning.
	float min_spline_dist = 2.0f;
	// Maximum distance from spline centerline to allow spawning.
	float max_spline_dist = 30.0f;
	// Offset added to the RNG seed.
	uint32_t seed_offset = 0;
	// FastNoiseLite resource used to modulate scatter density.
	Ref<Noise> biome_noise;
	// Threshold below which noise values cull instance placement.
	float biome_noise_threshold = 0.0f;

	/*
	 * Purpose: Processes a single grid cell to evaluate if a foliage instance should spawn.
	 * Execution steps:
	 *   1. Initialize SimpleRNG using cell coordinate seed and seed_offset.
	 *   2. Perform density probability check.
	 *   3. Generate candidate coordinates within the cell.
	 *   4. Sample biome noise (if valid) and perform threshold tests.
	 *   5. Perform spline corridor distance boundaries check.
	 *   6. Compute interpolated height, normal, and slope using helper.
	 *   7. If slope is within bounds, generate Transform3D with random scale and yaw rotation.
	 * Parameters:
	 *   - p_job: Active scatter job.
	 *   - cx: Cell grid X coordinate.
	 *   - cz: Cell grid Z coordinate.
	 *   - base_seed: Shared RNG baseline seed.
	 *   - offset: World offset coordinates.
	 *   - p_chunk_size: Size of chunk.
	 *   - spacing: Grid spacing size.
	 *   - density: Density threshold multiplier.
	 *   - heightmap_data: Raw float array pointer of heights.
	 *   - active_segments: Segment indices to test against.
	 *   - r_transform: Out reference to populate the resulting transform.
	 * Behavioral bounds: Thread-safe. Returns true if instance is valid, false otherwise.
	 */
	bool _process_scatter_cell(const Ref<ScatterJob> &p_job, int cx, int cz, uint64_t base_seed, const Vector2 &offset, int p_chunk_size, float spacing, float density, const float *heightmap_data, const std::vector<int> &active_segments, Transform3D &r_transform);

	/*
	 * Purpose: Evaluate exact surface height, normal vector, and slope angle at localized XZ coordinates.
	 * Execution steps:
	 *   1. Calculate surrounding 4 grid coordinates and interpolation fractions.
	 *   2. Perform bilinear interpolation to compute surface height.
	 *   3. Compute normal vector via local height derivatives.
	 *   4. Calculate slope angle in degrees via acos of the normal Y component.
	 * Parameters:
	 *   - heightmap_data: Raw elevation float grid pointer.
	 *   - p_chunk_size: Dimension length of chunk.
	 *   - local_x: Local X offset coordinate.
	 *   - local_z: Local Z offset coordinate.
	 *   - r_height: Out reference to receive exact height.
	 *   - r_normal: Out reference to receive surface normal.
	 *   - r_slope_deg: Out reference to receive slope in degrees.
	 * Behavioral bounds: Thread-safe math helper. Returns true on success.
	 */
	bool _evaluate_height_and_slope(const float *heightmap_data, int p_chunk_size, float local_x, float local_z, float &r_height, Vector3 &r_normal, float &r_slope_deg) const;

	/*
	 * Purpose: Dispatch scatter tasks to the background worker threads.
	 * Execution steps:
	 *   1. Iterate over all splines.
	 *   2. Find active scatterer children.
	 *   3. Create and configure a ScatterJob for each.
	 *   4. Submit jobs to WorkerThreadPool (or execute sequentially if no pool).
	 * Parameters:
	 *   - p_chunk: Target chunk object.
	 *   - p_splines: Vector of splines in the scene.
	 *   - p_chunk_rect: Bounds of the chunk.
	 *   - p_offset: Global coordinate offset.
	 *   - p_chunk_size: Size of chunk.
	 *   - r_jobs: Out reference vector storing created jobs.
	 *   - r_task_ids: Out reference vector storing thread task IDs.
	 * Behavioral bounds: Executed on main thread.
	 */
	static void _dispatch_scatter_jobs(const Ref<TerrainChunk> &p_chunk, const std::vector<ProceduralSpline3D *> &p_splines, const Rect2 &p_chunk_rect, const Vector2 &p_offset, int p_chunk_size, std::vector<Ref<ScatterJob>> &r_jobs, std::vector<int> &r_task_ids);

	/*
	 * Purpose: Finalize single completed ScatterJob on the main thread.
	 * Execution steps:
	 *   1. Log debug metrics if compiling in debug mode.
	 *   2. If transforms exist, create MultiMeshInstance3D and set visual meshes.
	 *   3. Pack transforms into float buffer array.
	 *   4. Append visual instance to parent hierarchy and chunk tracking.
	 *   5. Register physics collision bodies if Shape3D is valid.
	 * Parameters:
	 *   - p_job: Completed scatter job.
	 *   - p_scatter_container: Target container Node3D.
	 *   - p_owner_node: Root owner node fallback.
	 * Behavioral bounds: Must be executed on main thread.
	 */
	static void _finalize_scatter_job(const Ref<ScatterJob> &p_job, Node3D *p_scatter_container, Node *p_owner_node);

protected:
	/*
	 * Purpose: Bind class methods, properties, and signals to Godot's ClassDB for scripting.
	 * Execution steps:
	 *   1. Bind getter/setter methods.
	 *   2. Define property fields for mesh, collision shape, density, spacing, scale, slope, spline distance, seed, and biome noise.
	 * Parameters: None.
	 * Behavioral bounds: Called internally by Godot during class registration.
	 */
	static void _bind_methods();

public:
	/*
	 * Purpose: Construct the TerrainSplineScatter object with default scattering parameters.
	 * Execution steps: None.
	 * Parameters: None.
	 * Behavioral bounds: Initializes all scattering coefficients to default values.
	 */
	TerrainSplineScatter();

	/*
	 * Purpose: Destruct the TerrainSplineScatter object.
	 * Execution steps: None.
	 * Parameters: None.
	 * Behavioral bounds: Standard cleanup.
	 */
	~TerrainSplineScatter();

	/*
	 * Purpose: Execute a single parallelized scatter task across a grid cell coordinate.
	 * Execution steps:
	 *   1. Sample grid points inside the chunk.
	 *   2. Evaluate density, biome noise, slope, and spline corridor bounds.
	 *   3. If valid, append a Transform3D instance into the job transforms array.
	 * Parameters:
	 *   - p_job: The ScatterJob containing input state and outputs.
	 *   - p_chunk_size: Size length of the chunk in units.
	 * Behavioral bounds: Thread-safe, must only write to the job object.
	 */
	void run_scatter_job(const Ref<ScatterJob> &p_job, int p_chunk_size);

	/*
	 * Purpose: Coordinate thread-pool dispatching to scatter instances on a specific chunk.
	 * Execution steps:
	 *   1. Filter active splines intersecting the chunk AABB.
	 *   2. Create ScatterJobs for each active scatterer.
	 *   3. Dispatch jobs to WorkerThreadPool.
	 *   4. Wait for completion and commit instances to visual/physics servers.
	 * Parameters:
	 *   - p_chunk: Target chunk object.
	 *   - p_splines: Vector of splines in the scene.
	 *   - p_chunk_rect: XZ boundary rectangle of the chunk.
	 *   - p_offset: Global world offset coordinate.
	 *   - p_chunk_size: Dimension size of chunk.
	 *   - p_scatter_container: Node3D containing visual representations.
	 *   - p_owner_node: Root owner node.
	 * Behavioral bounds: Runs on main thread, blocks/waits for background threads.
	 */
	static void scatter_chunk(const Ref<TerrainChunk> &p_chunk, const std::vector<ProceduralSpline3D *> &p_splines, const Rect2 &p_chunk_rect, const Vector2 &p_offset, int p_chunk_size, Node3D *p_scatter_container, Node *p_owner_node);

	/*
	 * Purpose: Get the spline padding boundary required by this scatterer.
	 * Parameters: None.
	 * Behavioral bounds: Returns max_spline_dist, overriding SplineComponent virtual function.
	 */
	float get_spline_padding() const override {
		return max_spline_dist;
	}

	/*
	 * Purpose: Set the mesh resource to be scattered.
	 * Parameters:
	 *   - p_mesh: Ref pointer to the mesh resource.
	 * Behavioral bounds: Assigns the mesh reference.
	 */
	void set_mesh(const Ref<Mesh> &p_mesh) {
		mesh = p_mesh;
	}

	/*
	 * Purpose: Get the mesh resource to be scattered.
	 * Parameters: None.
	 * Behavioral bounds: Returns the mesh reference.
	 */
	Ref<Mesh> get_mesh() const {
		return mesh;
	}

	/*
	 * Purpose: Set the collision shape resource.
	 * Parameters:
	 *   - p_shape: Ref pointer to the Shape3D resource.
	 * Behavioral bounds: Assigns the shape reference.
	 */
	void set_collision_shape(const Ref<Shape3D> &p_shape) {
		collision_shape = p_shape;
	}

	/*
	 * Purpose: Get the collision shape resource.
	 * Parameters: None.
	 * Behavioral bounds: Returns the shape reference.
	 */
	Ref<Shape3D> get_collision_shape() const {
		return collision_shape;
	}

	/*
	 * Purpose: Set the spawn density value.
	 * Parameters:
	 *   - p_density: Density probability value, clamped between 0.0 and 1.0.
	 * Behavioral bounds: Limits the density bounds.
	 */
	void set_density(float p_density) {
		density = CLAMP(p_density, 0.0f, 1.0f);
	}

	/*
	 * Purpose: Get the spawn density value.
	 * Parameters: None.
	 * Behavioral bounds: Returns the density value.
	 */
	float get_density() const {
		return density;
	}

	/*
	 * Purpose: Set the cell spacing distance.
	 * Parameters:
	 *   - p_spacing: Spacing distance value, minimum clamped to 0.5f.
	 * Behavioral bounds: Limits spawn proximity.
	 */
	void set_spacing(float p_spacing) {
		spacing = MAX(0.5f, p_spacing);
	}

	/*
	 * Purpose: Get the cell spacing distance.
	 * Parameters: None.
	 * Behavioral bounds: Returns spacing distance.
	 */
	float get_spacing() const {
		return spacing;
	}

	/*
	 * Purpose: Set the minimum scale multiplier.
	 * Parameters:
	 *   - p_min: Scale value.
	 * Behavioral bounds: Assigns the minimum scale limit.
	 */
	void set_scale_min(float p_min) {
		scale_min = p_min;
	}

	/*
	 * Purpose: Get the minimum scale multiplier.
	 * Parameters: None.
	 * Behavioral bounds: Returns min scale multiplier.
	 */
	float get_scale_min() const {
		return scale_min;
	}

	/*
	 * Purpose: Set the maximum scale multiplier.
	 * Parameters:
	 *   - p_max: Scale value.
	 * Behavioral bounds: Assigns the maximum scale limit.
	 */
	void set_scale_max(float p_max) {
		scale_max = p_max;
	}

	/*
	 * Purpose: Get the maximum scale multiplier.
	 * Parameters: None.
	 * Behavioral bounds: Returns max scale multiplier.
	 */
	float get_scale_max() const {
		return scale_max;
	}

	/*
	 * Purpose: Set the minimum allowed slope angle.
	 * Parameters:
	 *   - p_min_slope: Minimum slope angle in degrees.
	 * Behavioral bounds: Assigns the slope boundary.
	 */
	void set_min_slope(float p_min_slope) {
		min_slope = p_min_slope;
	}

	/*
	 * Purpose: Get the minimum allowed slope angle.
	 * Parameters: None.
	 * Behavioral bounds: Returns min slope.
	 */
	float get_min_slope() const {
		return min_slope;
	}

	/*
	 * Purpose: Set the maximum allowed slope angle.
	 * Parameters:
	 *   - p_max_slope: Maximum slope angle in degrees.
	 * Behavioral bounds: Assigns the slope boundary.
	 */
	void set_max_slope(float p_max_slope) {
		max_slope = p_max_slope;
	}

	/*
	 * Purpose: Get the maximum allowed slope angle.
	 * Parameters: None.
	 * Behavioral bounds: Returns max slope.
	 */
	float get_max_slope() const {
		return max_slope;
	}

	/*
	 * Purpose: Set the minimum distance from spline centerline.
	 * Parameters:
	 *   - p_dist: Distance value.
	 * Behavioral bounds: Assigns the minimum distance limit.
	 */
	void set_min_spline_dist(float p_dist) {
		min_spline_dist = p_dist;
	}

	/*
	 * Purpose: Get the minimum distance from spline centerline.
	 * Parameters: None.
	 * Behavioral bounds: Returns min distance.
	 */
	float get_min_spline_dist() const {
		return min_spline_dist;
	}

	/*
	 * Purpose: Set the maximum distance from spline centerline.
	 * Parameters:
	 *   - p_dist: Distance value.
	 * Behavioral bounds: Assigns the maximum distance limit.
	 */
	void set_max_spline_dist(float p_dist) {
		max_spline_dist = p_dist;
	}

	/*
	 * Purpose: Get the maximum distance from spline centerline.
	 * Parameters: None.
	 * Behavioral bounds: Returns max distance.
	 */
	float get_max_spline_dist() const {
		return max_spline_dist;
	}

	/*
	 * Purpose: Set the RNG seed offset.
	 * Parameters:
	 *   - p_seed_offset: The offset value.
	 * Behavioral bounds: Assigns seed modifier.
	 */
	void set_seed_offset(int p_seed_offset) {
		seed_offset = p_seed_offset;
	}

	/*
	 * Purpose: Get the RNG seed offset.
	 * Parameters: None.
	 * Behavioral bounds: Returns seed offset.
	 */
	int get_seed_offset() const {
		return seed_offset;
	}

	/*
	 * Purpose: Set the Noise resource to modulate density.
	 * Parameters:
	 *   - p_noise: Ref pointer to the Noise resource.
	 * Behavioral bounds: Assigns biome noise reference.
	 */
	void set_biome_noise(const Ref<Noise> &p_noise) {
		biome_noise = p_noise;
	}

	/*
	 * Purpose: Get the Noise resource.
	 * Parameters: None.
	 * Behavioral bounds: Returns biome noise.
	 */
	Ref<Noise> get_biome_noise() const {
		return biome_noise;
	}

	/*
	 * Purpose: Set the biome noise threshold value.
	 * Parameters:
	 *   - p_threshold: Threshold limit.
	 * Behavioral bounds: Assigns noise filter threshold.
	 */
	void set_biome_noise_threshold(float p_threshold) {
		biome_noise_threshold = p_threshold;
	}

	/*
	 * Purpose: Get the biome noise threshold value.
	 * Parameters: None.
	 * Behavioral bounds: Returns noise threshold.
	 */
	float get_biome_noise_threshold() const {
		return biome_noise_threshold;
	}
};

} // namespace godot

#endif // TR_SCATTER_H
