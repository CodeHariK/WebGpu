#ifndef TERRASPLINE_NEW_H
#define TERRASPLINE_NEW_H

#include "godot_cpp/classes/texture_rect.hpp"
#include "utils/spline3d/procedural_spline3d.h"
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/multi_mesh_instance3d.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/noise.hpp>
#include <godot_cpp/classes/physics_server3d.hpp>
#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/templates/hash_map.hpp>
#include <godot_cpp/variant/packed_float32_array.hpp>
#include <godot_cpp/variant/rect2.hpp>
#include <godot_cpp/variant/transform3d.hpp>
#include <godot_cpp/variant/vector2i.hpp>
#include <algorithm>
#include <vector>

namespace godot {

class ProceduralSpline3D;
class TerrainSplineDeformer;
class TerrainSplineScatter;
class TerrainSplineCompositor;
class ScatterJob;

/**
 * @class TerrainHeightmap
 * @brief Thread-safe wrapper around raw terrain elevation grids.
 *
 * Keeps a contiguous 1D array of floats representing height values for a single terrain chunk.
 * Exposes get_data_ptrw() to allow worker threads to read and write grid heights directly
 * and concurrently with no locks. Converts the raw data to a Godot Image format via get_image()
 * for importing into Terrain3D GPU storage.
 */
class TerrainHeightmap : public RefCounted {
	GDCLASS(TerrainHeightmap, RefCounted)

private:
	int width = 0;
	int height = 0;
	PackedFloat32Array data;

protected:
	static void _bind_methods();

public:
	TerrainHeightmap();
	~TerrainHeightmap();

	void initialize(int p_width, int p_height, float p_default_value = 0.0f);
	void clear(float p_default_value = 0.0f);

	int get_width() const { return width; }
	int get_height() const { return height; }

	float *get_data_ptrw() { return data.ptrw(); }
	Ref<Image> get_image() const;
};

/**
 * @class TerrainChunk
 * @brief Represents a localized physical block of the world.
 *
 * Manages the heightmap data, active visual nodes (MultiMeshInstance3D IDs), and physics body
 * collision instances (RIDs) for a single chunk coordinate. Operates on a coordinate-based
 * state machine (STATE_UNLOADED to STATE_VISUAL_AND_PHYSICS) to cull meshes and physics
 * depending on proximity to the player.
 */
class TerrainChunk : public RefCounted {
	GDCLASS(TerrainChunk, RefCounted)

public:
	enum ChunkState {
		STATE_UNLOADED = 0,
		STATE_GENERATING = 1,
		STATE_VISUAL_ONLY = 2,
		STATE_VISUAL_AND_PHYSICS = 3,
		STATE_MARKED_FOR_EVICTION = 4
	};

	struct PhysicsCache {
		RID shape_rid;
		std::vector<Transform3D> transforms;
	};

private:
	Vector2i chunk_coords;
	ChunkState current_state = STATE_UNLOADED;
	Ref<TerrainHeightmap> heightmap;

	std::vector<uint64_t> visual_nodes;
	RID physics_body_rid;
	std::vector<PhysicsCache> physics_caches;

protected:
	static void _bind_methods();

public:
	TerrainChunk();
	~TerrainChunk();

	Vector2i get_chunk_coords() const { return chunk_coords; }
	void set_chunk_coords(const Vector2i &p_coords) { chunk_coords = p_coords; }

	ChunkState get_state() const { return current_state; }
	void set_state(ChunkState p_state) { current_state = p_state; }

	Ref<TerrainHeightmap> get_heightmap() const { return heightmap; }
	void set_heightmap(const Ref<TerrainHeightmap> &p_heightmap) { heightmap = p_heightmap; }

	std::vector<uint64_t> &get_visual_nodes() { return visual_nodes; }
	const std::vector<uint64_t> &get_visual_nodes() const { return visual_nodes; }

	RID get_physics_body_rid() const { return physics_body_rid; }
	void set_physics_body_rid(const RID &p_rid) { physics_body_rid = p_rid; }

	std::vector<PhysicsCache> &get_physics_caches() { return physics_caches; }
	const std::vector<PhysicsCache> &get_physics_caches() const { return physics_caches; }
};

class GrayscaleJob : public RefCounted {
	GDCLASS(GrayscaleJob, RefCounted)
public:
	const float *ptr = nullptr;
	uint8_t *byte_ptr = nullptr;
	float min_h = 0.0f;
	float range = 1.0f;
	int size = 0;
	int chunk_size = 0;

	GrayscaleJob() {}
	~GrayscaleJob() {}

protected:
	static void _bind_methods() {}
};

/**
 * @class ChunkJob
 * @brief Everything one chunk generation needs, gathered on the main thread so the math can run anywhere.
 * Phase 1 (`_run_chunk_job_math`, any thread): noise fill, spline deformers, scatter transforms.
 * Phase 2 (`_finalize_chunk_job`, main thread): MultiMesh nodes, physics caches, Terrain3D region.
 */
class ChunkJob : public RefCounted {
	GDCLASS(ChunkJob, RefCounted)
public:
	struct SplineEntry {
		ProceduralSpline3D *spline = nullptr;
		Rect2 padded_aabb;
		std::vector<TerrainSplineDeformer *> deformers;
		std::vector<TerrainSplineScatter *> scatterers;
	};

	Vector2i chunk_pos;
	Vector2 offset;
	Rect2 chunk_rect;
	int chunk_size = 0;
	Ref<TerrainChunk> chunk;
	std::vector<SplineEntry> splines; // only splines whose padded AABB intersects this chunk
	Ref<Noise> noise;
	float default_elevation = 0.0f;
	float noise_amplitude = 0.0f;
	Vector2 world_offset_at_dispatch;

	// Outputs
	std::vector<Ref<ScatterJob>> scatter_jobs;
	uint64_t math_usec = 0;
	uint64_t scatter_usec = 0;

	// Async bookkeeping
	int task_id = -1;

	ChunkJob() {}
	~ChunkJob() {}

protected:
	static void _bind_methods() {}
};

class TerrainSplineCompositor : public Node {
	GDCLASS(TerrainSplineCompositor, Node)
private:
	Node *terrain = nullptr;
	int chunk_size = 256;
	float default_elevation = 0.0f;
	bool auto_apply = true;
	bool _rebuild_queued = false;
	bool _rebuild_retry_pending = false;
	int _rebuild_retry_frames = 0;
	static constexpr int MAX_REBUILD_RETRY_FRAMES = 600; // ~10 s at 60 fps
	bool _warned_vertex_spacing = false;
	std::vector<uint64_t> _known_spline_ids;
	bool compositor_full_rebuild = true;
	HashMap<Vector2i, Ref<TerrainChunk>> chunk_buffers;

	float max_render_radius = 2048.0f;
	float max_physics_radius = 150.0f;
	uint64_t last_eviction_check_time = 0;
	static constexpr uint64_t DISCOVERY_INTERVAL_MS = 500;

	// Streaming generation queue (see Terraspline.md step 1).
	std::vector<Vector2i> _gen_queue;
	float generation_budget_ms = 4.0f;
	void _enqueue_chunks(const std::vector<Vector2i> &p_chunks, bool p_allow_existing);
	void _drain_generation_queue(uint64_t p_frame_start_usec);

	// Chunk job pipeline (tr_chunk_job.cpp). See Terraspline.md step 3.
	Ref<ChunkJob> _make_chunk_job(const Vector2i &p_chunk_pos, const std::vector<ProceduralSpline3D *> &p_splines);
	static void _run_chunk_job_math(const Ref<ChunkJob> &p_job, bool p_threaded);
	void _finalize_chunk_job(const Ref<ChunkJob> &p_job, Object *p_target_api);
	std::vector<Ref<ChunkJob>> _jobs_in_flight;
	int max_jobs_in_flight = 0; // 0 = auto (hardware threads - 1)
	void _wait_for_jobs_in_flight();
	void _refresh_terrain_collision();
	bool _terrain_maps_dirty = false;
	int _frames_since_flush = 0;
	static constexpr int FLUSH_MAX_FRAMES = 6; // ~100 ms at 60 fps
	void _flush_terrain_maps(Object *p_target_api);
	Vector2 global_world_offset = Vector2(0.0f, 0.0f);
	Node3D *scatter_container = nullptr;

	Ref<Noise> global_terrain_noise;
	float global_terrain_amplitude = 50.0f;

	void _check_and_evict_far_chunks();
	void _check_origin_shift();
	void _generate_chunks(const std::vector<Vector2i> &p_chunks, const std::vector<ProceduralSpline3D *> &p_splines, Object *p_target_api);

	void _update_chunk_physics(const Ref<TerrainChunk> &p_chunk);
	void _check_chunk_physics_culling();
	Vector3 _get_player_position() const;
	bool _spline_set_changed();

	// --- Benchmark mode (see Terraspline.md; enabled with `-- --terraspline-bench`) ---
	bool _bench = false;
	bool _bench_done = false;
	std::vector<Vector2i> _bench_chunks;
	uint64_t _bench_start_usec = 0;
	uint64_t _bench_first_frame = 0;
	HashMap<uint64_t, double> _bench_frame_ms; // process frame -> main-thread ms spent in compositor
	double _bench_math_ms = 0.0;
	double _bench_scatter_ms = 0.0;
	double _bench_upload_ms = 0.0; // per-chunk region add (main)
	double _bench_flush_ms = 0.0; // update_maps per batch (main)
	int _bench_flushes = 0;
	double _bench_collision_ms = 0.0; // collision.update(true) per batch (main)
	double _bench_make_ms = 0.0; // _make_chunk_job (main)
	double _bench_finalize_ms = 0.0; // _finalize_chunk_job incl. region add (main)
	void _bench_init();
	void _bench_add_frame_time(uint64_t p_usec);
	void _bench_check_done();
	bool _bench_wants_chunk(const Vector2i &p_chunk) const;
	String _bench_dump_dir;
	void _bench_dump_chunk(const Ref<TerrainChunk> &p_chunk);

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	TerrainSplineCompositor();
	~TerrainSplineCompositor();

	void set_global_terrain_noise(const Ref<Noise> &p_noise);
	Ref<Noise> get_global_terrain_noise() const;
	void set_global_terrain_amplitude(float p_amp);
	float get_global_terrain_amplitude() const;

	void set_terrain(Node *p_terrain);
	Node *get_terrain() const;
	void set_chunk_size(int p_size);
	int get_chunk_size() const;
	void set_default_elevation(float p_elev);
	float get_default_elevation() const;
	void set_auto_apply(bool p_auto);
	bool get_auto_apply() const;
	void set_apply_now(bool p_apply);
	bool get_apply_now() const;
	void set_max_render_radius(float p_radius);
	float get_max_render_radius() const;
	void set_max_physics_radius(float p_radius);
	float get_max_physics_radius() const;
	void set_global_world_offset(Vector2 p_offset);
	Vector2 get_global_world_offset() const;
	void set_generation_budget_ms(float p_ms);
	float get_generation_budget_ms() const;
	void queue_rebuild();
	void _execute_rebuild();
	void apply_all_splines();
	void _connect_spline(Node *p_node);
	void _disconnect_spline(Node *p_node);
	void _on_spline_changed();
	void _run_chunk_job_task(Ref<ChunkJob> p_job); // WorkerThreadPool entry point
	void set_max_jobs_in_flight(int p_n) { max_jobs_in_flight = MAX(0, p_n); }
	int get_max_jobs_in_flight() const { return max_jobs_in_flight; }
};

class TerrainSplineCompositorUI : public TextureRect {
	GDCLASS(TerrainSplineCompositorUI, TextureRect)
private:
	float default_elevation = 0.0f;
	bool auto_apply = true;
	bool _rebuild_queued = false;

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	TerrainSplineCompositorUI();
	~TerrainSplineCompositorUI();
	void set_default_elevation(float p_elev);
	float get_default_elevation() const;
	void set_auto_apply(bool p_auto);
	bool get_auto_apply() const;
	void set_apply_now(bool p_apply);
	bool get_apply_now() const;
	void queue_rebuild();
	void _execute_rebuild();
	void apply_all_splines();
	void _connect_spline(Node *p_node);
	void _disconnect_spline(Node *p_node);
	void _on_spline_changed();
	void _normalize_grayscale_task(int p_task_idx, Ref<GrayscaleJob> p_job);
};

} // namespace godot

#include "tr_deformer.h"
#include "tr_scatter.h"
#include <vector>

#endif // TERRASPLINE_NEW_H
