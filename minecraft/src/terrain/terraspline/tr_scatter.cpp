#include "terraspline.h"
#include <godot_cpp/classes/time.hpp> // NEW: For high-precision profiling
#include <godot_cpp/classes/worker_thread_pool.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/object.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

/**
 * @brief Binds TerrainSplineScatter methods and properties to Godot's ClassDB.
 * Exposes spawned mesh, collision shapes, spacing/density parameters, slope thresholds, and spline corridor boundaries.
 */
void TerrainSplineScatter::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_mesh", "mesh"), &TerrainSplineScatter::set_mesh);
	ClassDB::bind_method(D_METHOD("get_mesh"), &TerrainSplineScatter::get_mesh);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "mesh", PROPERTY_HINT_RESOURCE_TYPE, "Mesh"), "set_mesh", "get_mesh");

	ClassDB::bind_method(D_METHOD("set_collision_shape", "shape"), &TerrainSplineScatter::set_collision_shape);
	ClassDB::bind_method(D_METHOD("get_collision_shape"), &TerrainSplineScatter::get_collision_shape);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "collision_shape", PROPERTY_HINT_RESOURCE_TYPE, "Shape3D"), "set_collision_shape", "get_collision_shape");

	ClassDB::bind_method(D_METHOD("set_density", "density"), &TerrainSplineScatter::set_density);
	ClassDB::bind_method(D_METHOD("get_density"), &TerrainSplineScatter::get_density);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "density"), "set_density", "get_density");

	ClassDB::bind_method(D_METHOD("set_spacing", "spacing"), &TerrainSplineScatter::set_spacing);
	ClassDB::bind_method(D_METHOD("get_spacing"), &TerrainSplineScatter::get_spacing);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "spacing"), "set_spacing", "get_spacing");

	ClassDB::bind_method(D_METHOD("set_scale_min", "min_scale"), &TerrainSplineScatter::set_scale_min);
	ClassDB::bind_method(D_METHOD("get_scale_min"), &TerrainSplineScatter::get_scale_min);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "scale_min"), "set_scale_min", "get_scale_min");

	ClassDB::bind_method(D_METHOD("set_scale_max", "max_scale"), &TerrainSplineScatter::set_scale_max);
	ClassDB::bind_method(D_METHOD("get_scale_max"), &TerrainSplineScatter::get_scale_max);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "scale_max"), "set_scale_max", "get_scale_max");

	ClassDB::bind_method(D_METHOD("set_min_slope", "min_slope"), &TerrainSplineScatter::set_min_slope);
	ClassDB::bind_method(D_METHOD("get_min_slope"), &TerrainSplineScatter::get_min_slope);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "min_slope"), "set_min_slope", "get_min_slope");

	ClassDB::bind_method(D_METHOD("set_max_slope", "max_slope"), &TerrainSplineScatter::set_max_slope);
	ClassDB::bind_method(D_METHOD("get_max_slope"), &TerrainSplineScatter::get_max_slope);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "max_slope"), "set_max_slope", "get_max_slope");

	ClassDB::bind_method(D_METHOD("set_min_spline_dist", "dist"), &TerrainSplineScatter::set_min_spline_dist);
	ClassDB::bind_method(D_METHOD("get_min_spline_dist"), &TerrainSplineScatter::get_min_spline_dist);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "min_spline_dist"), "set_min_spline_dist", "get_min_spline_dist");

	ClassDB::bind_method(D_METHOD("set_max_spline_dist", "dist"), &TerrainSplineScatter::set_max_spline_dist);
	ClassDB::bind_method(D_METHOD("get_max_spline_dist"), &TerrainSplineScatter::get_max_spline_dist);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "max_spline_dist"), "set_max_spline_dist", "get_max_spline_dist");

	ClassDB::bind_method(D_METHOD("set_seed_offset", "seed_offset"), &TerrainSplineScatter::set_seed_offset);
	ClassDB::bind_method(D_METHOD("get_seed_offset"), &TerrainSplineScatter::get_seed_offset);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "seed_offset"), "set_seed_offset", "get_seed_offset");

	ClassDB::bind_method(D_METHOD("set_biome_noise", "noise"), &TerrainSplineScatter::set_biome_noise);
	ClassDB::bind_method(D_METHOD("get_biome_noise"), &TerrainSplineScatter::get_biome_noise);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "biome_noise", PROPERTY_HINT_RESOURCE_TYPE, "Noise"), "set_biome_noise", "get_biome_noise");

	ClassDB::bind_method(D_METHOD("set_biome_noise_threshold", "threshold"), &TerrainSplineScatter::set_biome_noise_threshold);
	ClassDB::bind_method(D_METHOD("get_biome_noise_threshold"), &TerrainSplineScatter::get_biome_noise_threshold);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "biome_noise_threshold"), "set_biome_noise_threshold", "get_biome_noise_threshold");

	ClassDB::bind_method(D_METHOD("run_scatter_job", "job", "chunk_size"), &TerrainSplineScatter::run_scatter_job);
}

/**
 * @brief Default Constructor. Configures default density, spacing, and scale limits.
 */
TerrainSplineScatter::TerrainSplineScatter() {
	density = 0.1f;
	spacing = 4.0f;
	scale_min = 0.8f;
	scale_max = 1.2f;
	min_slope = 0.0f;
	max_slope = 35.0f;
	min_spline_dist = 2.0f;
	max_spline_dist = 30.0f;
	seed_offset = 0;
	biome_noise_threshold = 0.0f;
}

/**
 * @brief Default Destructor.
 */
TerrainSplineScatter::~TerrainSplineScatter() {}

void TerrainSplineScatter::set_mesh(const Ref<Mesh> &p_mesh) { mesh = p_mesh; }
Ref<Mesh> TerrainSplineScatter::get_mesh() const { return mesh; }

void TerrainSplineScatter::set_collision_shape(const Ref<Shape3D> &p_shape) { collision_shape = p_shape; }
Ref<Shape3D> TerrainSplineScatter::get_collision_shape() const { return collision_shape; }

void TerrainSplineScatter::set_density(float p_density) { density = CLAMP(p_density, 0.0f, 1.0f); }
float TerrainSplineScatter::get_density() const { return density; }

void TerrainSplineScatter::set_spacing(float p_spacing) { spacing = MAX(0.5f, p_spacing); }
float TerrainSplineScatter::get_spacing() const { return spacing; }

void TerrainSplineScatter::set_scale_min(float p_min) { scale_min = p_min; }
float TerrainSplineScatter::get_scale_min() const { return scale_min; }

void TerrainSplineScatter::set_scale_max(float p_max) { scale_max = p_max; }
float TerrainSplineScatter::get_scale_max() const { return scale_max; }

void TerrainSplineScatter::set_min_slope(float p_min_slope) { min_slope = p_min_slope; }
float TerrainSplineScatter::get_min_slope() const { return min_slope; }

void TerrainSplineScatter::set_max_slope(float p_max_slope) { max_slope = p_max_slope; }
float TerrainSplineScatter::get_max_slope() const { return max_slope; }

void TerrainSplineScatter::set_min_spline_dist(float p_dist) { min_spline_dist = p_dist; }
float TerrainSplineScatter::get_min_spline_dist() const { return min_spline_dist; }

void TerrainSplineScatter::set_max_spline_dist(float p_dist) { max_spline_dist = p_dist; }
float TerrainSplineScatter::get_max_spline_dist() const { return max_spline_dist; }

void TerrainSplineScatter::set_seed_offset(int p_seed_offset) { seed_offset = p_seed_offset; }
int TerrainSplineScatter::get_seed_offset() const { return seed_offset; }

void TerrainSplineScatter::set_biome_noise(const Ref<Noise> &p_noise) { biome_noise = p_noise; }
Ref<Noise> TerrainSplineScatter::get_biome_noise() const { return biome_noise; }

void TerrainSplineScatter::set_biome_noise_threshold(float p_threshold) { biome_noise_threshold = p_threshold; }
float TerrainSplineScatter::get_biome_noise_threshold() const { return biome_noise_threshold; }

static inline float local_lerp(float a, float b, float t) {
	return a + t * (b - a);
}

/**
 * @brief Thread-pool worker function that evaluates and generates transform matrices for foliage scattering.
 * Iterates through grid cells, checks biome noise, evaluates distances to spline corridors, checks heightmap slopes,
 * and adds successfully generated foliage points into the ScatterJob transforms.
 */
void TerrainSplineScatter::run_scatter_job(const Ref<ScatterJob> &p_job, int p_chunk_size) {
	if (p_job.is_null() || p_job->chunk.is_null() || p_job->spline == nullptr || p_job->scatterer == nullptr) {
		return;
	}

#if DEBUG
	uint64_t t_start = Time::get_singleton()->get_ticks_usec();
#endif
	TerrainSplineScatter *scatterer = p_job->scatterer;
	ProceduralSpline3D *spline = p_job->spline;
	Ref<TerrainChunk> chunk = p_job->chunk;
	Vector2 offset = p_job->offset;

	float spacing = scatterer->get_spacing();
	float density = scatterer->get_density();
	if (density <= 0.0f || spacing <= 0.1f) {
		return;
	}

	Vector2i chunk_pos = chunk->get_chunk_coords();
	Ref<TerrainHeightmap> buffer = chunk->get_heightmap();
	if (buffer.is_null()) {
		return;
	}
	float *heightmap_data = buffer->get_data_ptrw();
	if (!heightmap_data) {
		return;
	}

	// Calculate the total number of cells in the chunk
	int num_cells = (int)Math::floor(p_chunk_size / spacing);
	if (num_cells <= 0) {
		return;
	}

	// OPTIMIZATION: Only loop through cells near the spline!
	Rect2 spline_bounds = p_job->spline_bounds;
	spline_bounds = spline_bounds.grow(scatterer->get_max_spline_dist()); // Pad by scatter radius

	Rect2 chunk_rect(offset, Vector2(p_chunk_size, p_chunk_size));
	Rect2 active_area = spline_bounds.intersection(chunk_rect);

	if (!active_area.has_area()) {
		return; // Spline doesn't affect this chunk's scatter area
	}

	// Find which segments overlap the active area (grown by max_spline_dist)
	std::vector<int> active_segments;
	float max_dist = scatterer->get_max_spline_dist();
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

	// Convert the active area bounds into grid cell indices
	int min_cx = Math::max(0, (int)Math::floor((active_area.position.x - offset.x) / spacing));
	int max_cx = Math::min(num_cells - 1, (int)Math::ceil((active_area.position.x + active_area.size.x - offset.x) / spacing));
	int min_cz = Math::max(0, (int)Math::floor((active_area.position.y - offset.y) / spacing));
	int max_cz = Math::min(num_cells - 1, (int)Math::ceil((active_area.position.y + active_area.size.y - offset.y) / spacing));

	p_job->debug_total_cells = (max_cx - min_cx + 1) * (max_cz - min_cz + 1);
	p_job->debug_density_skipped = 0;
	p_job->debug_noise_skipped = 0;
	p_job->debug_spline_skipped = 0;
	p_job->debug_slope_skipped = 0;

	std::vector<Transform3D> transforms;
	transforms.reserve(p_job->debug_total_cells);

	uint64_t base_seed = 1234567ULL;

	// Loop ONLY over the restricted bounding box!
	for (int cz = min_cz; cz <= max_cz; ++cz) {
		for (int cx = min_cx; cx <= max_cx; ++cx) {
			// Deterministic cell seed
			uint64_t cell_seed = base_seed ^ (uint64_t(chunk_pos.x) * 73856093ULL) ^ (uint64_t(chunk_pos.y) * 19349663ULL) ^ (uint64_t(cx) * 83492791ULL) ^ (uint64_t(cz) * 37476139ULL) ^ uint64_t(scatterer->get_seed_offset());

			// Simple RNG
			struct SimpleRNG {
				uint64_t state;
				SimpleRNG(uint64_t seed) : state(seed) {}
				uint32_t next() {
					state = state * 6364136223846793005ULL + 1442695040888963407ULL;
					return uint32_t(state >> 32);
				}
				float next_float() {
					return float(next()) / 4294967296.0f;
				}
				float next_float_range(float min_val, float max_val) {
					return min_val + next_float() * (max_val - min_val);
				}
			} rng(cell_seed);

			// Density check per cell
			if (rng.next_float() > density) {
				p_job->debug_density_skipped++;
				continue;
			}

			// Generate candidate point in cell
			float cell_min_x = offset.x + cx * spacing;
			float cell_min_z = offset.y + cz * spacing;

			float rx = rng.next_float() * spacing;
			float rz = rng.next_float() * spacing;
			float random_x = cell_min_x + rx;
			float random_z = cell_min_z + rz;

			// Coherent biome noise check
			if (scatterer->get_biome_noise().is_valid()) {
				float noise_val = (scatterer->get_biome_noise()->get_noise_2d(random_x, random_z) + 1.0f) * 0.5f;
				if (noise_val < scatterer->get_biome_noise_threshold()) {
					p_job->debug_noise_skipped++;
					continue;
				}
				// Density scaling based on noise
				if (rng.next_float() > noise_val) {
					p_job->debug_noise_skipped++;
					continue;
				}
			}

			// Spline corridor check
			Vector2 test_point(random_x, random_z);
			ProceduralSpline3D::SplineEval eval = spline->evaluate_spline_point_segmented(test_point, active_segments);
			if (eval.distance < scatterer->get_min_spline_dist() || eval.distance > scatterer->get_max_spline_dist()) {
				p_job->debug_spline_skipped++;
				continue;
			}

			// Local heights check to determine normal and slope
			float local_x = CLAMP(random_x - offset.x, 0.0f, (float)p_chunk_size - 1.0001f);
			float local_z = CLAMP(random_z - offset.y, 0.0f, (float)p_chunk_size - 1.0001f);

			int lx = (int)local_x;
			int lz = (int)local_z;

			// clamp coordinates for safety
			int lx_min = Math::clamp(lx, 0, p_chunk_size - 1);
			int lx_max = Math::clamp(lx + 1, 0, p_chunk_size - 1);
			int lz_min = Math::clamp(lz, 0, p_chunk_size - 1);
			int lz_max = Math::clamp(lz + 1, 0, p_chunk_size - 1);

			float fx = local_x - lx;
			float fz = local_z - lz;

			// Get the 4 surrounding heights
			float h00 = heightmap_data[lz_min * p_chunk_size + lx_min];
			float h10 = heightmap_data[lz_min * p_chunk_size + lx_max];
			float h01 = heightmap_data[lz_max * p_chunk_size + lx_min];
			float h11 = heightmap_data[lz_max * p_chunk_size + lx_max];

			// Bilinear interpolation height calculation
			float exact_y = local_lerp(local_lerp(h00, h10, fx), local_lerp(h01, h11, fx), fz);

			// Compute precise normal vector using adjacent heightmap heights (bilinearly interpolated)
			float dx = local_lerp(h10 - h00, h11 - h01, fz);
			float dz = local_lerp(h01 - h00, h11 - h10, fx);

			Vector3 normal(-dx, 1.0f, -dz);
			normal.normalize();

			// Slope angle theta = accos(n.y)
			float theta = Math::acos(normal.y);
			float theta_deg = Math::rad_to_deg(theta);
			if (theta_deg < scatterer->get_min_slope() || theta_deg > scatterer->get_max_slope()) {
				p_job->debug_slope_skipped++;
				continue;
			}

			// Build transform
			float rand_yaw = rng.next_float_range(0.0f, Math_TAU);
			float rand_scale = rng.next_float_range(scatterer->get_scale_min(), scatterer->get_scale_max());

			Transform3D t;
			t.origin = Vector3(random_x, exact_y, random_z);
			t.basis.rotate(Vector3(0, 1, 0), rand_yaw);
			t.basis.scale(Vector3(1, 1, 1) * rand_scale);

			transforms.push_back(t);
		}
	}

	transforms.shrink_to_fit();
	p_job->transforms = transforms;
#if DEBUG
	p_job->debug_time_spent_usec = Time::get_singleton()->get_ticks_usec() - t_start;
#endif
}

/**
 * @brief Static method orchestrating foliage generation for a given chunk across all overlapping splines.
 * Instantiates ScatterJobs, registers them in WorkerThreadPool tasks, waits for completions, and creates
 * MultiMeshInstance3D visual nodes and static colliders on the main thread.
 */
void TerrainSplineScatter::scatter_chunk(const Ref<TerrainChunk> &p_chunk, const std::vector<ProceduralSpline3D *> &p_splines, const Rect2 &p_chunk_rect, const Vector2 &p_offset, int p_chunk_size, Node3D *p_scatter_container, Node *p_owner_node) {
	std::vector<Ref<ScatterJob>> jobs;
	std::vector<int> task_ids;
	WorkerThreadPool *wtp = WorkerThreadPool::get_singleton();

	for (ProceduralSpline3D *spline : p_splines) {
		if (!spline->get_padded_aabb().intersects(p_chunk_rect)) {
			continue;
		}

		spline->ensure_baked_cache();

		TypedArray<Node> spline_children = spline->get_children();
		for (int i = 0; i < spline_children.size(); ++i) {
			TerrainSplineScatter *scatterer = Object::cast_to<TerrainSplineScatter>(spline_children[i]);
			if (scatterer) {
				Ref<ScatterJob> job;
				job.instantiate();
				job->chunk = p_chunk;
				job->spline = spline;
				job->scatterer = scatterer;
				job->offset = p_offset;
				job->spline_bounds = spline->get_padded_aabb();

				jobs.push_back(job);

				if (wtp) {
					Callable callable = Callable(scatterer, "run_scatter_job").bind(job, p_chunk_size);
					int task_id = wtp->add_task(callable, "TerrainScatterJob");
					task_ids.push_back(task_id);
				} else {
					scatterer->run_scatter_job(job, p_chunk_size);
				}
			}
		}
	}

	// Wait for background tasks to complete
	if (wtp) {
		for (int task_id : task_ids) {
			wtp->wait_for_task_completion(task_id);
		}
	}

	// Now finalize the completed jobs on the main thread
	for (const Ref<ScatterJob> &job : jobs) {
		TerrainSplineScatter *scatterer = job->scatterer;

#if DEBUG
		int spawned = (int)job->transforms.size();
		float time_ms = job->debug_time_spent_usec / 1000.0f;
		float spawned_pct = job->debug_total_cells > 0 ? (float)spawned / job->debug_total_cells * 100.0f : 0.0f;

		UtilityFunctions::print("    [Scatterer: ", scatterer->get_name(), "] Spawned: ", spawned, " / ", job->debug_total_cells, " cells (", spawned_pct, "%) | Time: ", time_ms, " ms");

		if (job->debug_total_cells > 0) {
			float dens_pct = (float)job->debug_density_skipped / job->debug_total_cells * 100.0f;
			float noise_pct = (float)job->debug_noise_skipped / job->debug_total_cells * 100.0f;
			float spline_pct = (float)job->debug_spline_skipped / job->debug_total_cells * 100.0f;
			float slope_pct = (float)job->debug_slope_skipped / job->debug_total_cells * 100.0f;
			UtilityFunctions::print("      └─ Culled -> Density: ", job->debug_density_skipped, " (", dens_pct, "%) | Noise: ", job->debug_noise_skipped, " (", noise_pct, "%) | Spline Corridor: ", job->debug_spline_skipped, " (", spline_pct, "%) | Slope: ", job->debug_slope_skipped, " (", slope_pct, "%)");
		}
#endif

		if (job->transforms.empty()) {
			continue;
		}

		// 1. Create visuals (MultiMeshInstance3D)
		MultiMeshInstance3D *mmi = memnew(MultiMeshInstance3D);
		Ref<MultiMesh> mm;
		mm.instantiate();
		mm->set_transform_format(MultiMesh::TRANSFORM_3D);
		mm->set_instance_count(job->transforms.size());
		mm->set_mesh(scatterer->get_mesh());

		// Pack transforms into a float array
		PackedFloat32Array buffer_array;
		buffer_array.resize(job->transforms.size() * 12);
		float *ptr = buffer_array.ptrw();
		for (size_t i = 0; i < job->transforms.size(); ++i) {
			Transform3D t = job->transforms[i];
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
		mmi->set_multimesh(mm);
		if (p_scatter_container) {
			p_scatter_container->add_child(mmi);
		} else if (p_owner_node) {
			p_owner_node->add_child(mmi);
		}
		p_chunk->get_visual_nodes().push_back(mmi->get_instance_id());

		// 2. Cache physics transforms if collision shape is set
		if (scatterer->get_collision_shape().is_valid()) {
			TerrainChunk::PhysicsCache pc;
			pc.shape_rid = scatterer->get_collision_shape()->get_rid();
			pc.transforms = job->transforms;
			p_chunk->get_physics_caches().push_back(pc);
		}
	}
}

} //namespace godot
