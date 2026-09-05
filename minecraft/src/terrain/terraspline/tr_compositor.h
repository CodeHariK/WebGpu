/**
 * @file tr_compositor.h
 * @brief TerrainSplineCompositor: streams spline-deformed terrain chunks into Terrain3D.
 */
#ifndef TR_COMPOSITOR_H
#define TR_COMPOSITOR_H

#include "tr_chunk.h"
#include "tr_chunk_job.h"
#include "utils/spline3d/procedural_spline3d.h"
#include <cstdint>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/noise.hpp>
#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/templates/hash_map.hpp>
#include <godot_cpp/variant/rect2.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/vector2i.hpp>
#include <godot_cpp/variant/vector3.hpp>
#include <vector>

namespace godot {

/**
 * @class TerrainSplineCompositor
 * @brief Turns child ProceduralSpline3Ds (with deformer / scatter components) into Terrain3D
 * heightmap regions, streamed in `chunk_size`-metre chunks around the player.
 *
 * Responsibilities, one source file each:
 *  - tr_compositor.cpp              bindings, properties, lifecycle, signal wiring
 *  - tr_compositor_rebuild.cpp      deciding which chunks a rebuild touches (apply_all_splines)
 *  - tr_compositor_stream.cpp       the time-budgeted generation queue and async job pipeline
 *  - tr_compositor_eviction.cpp     evicting far chunks/regions, discovering missing chunks,
 *                                   waking/sleeping chunk physics by distance
 *  - tr_compositor_origin_shift.cpp floating-origin shift at +-4096 m
 *  - tr_compositor_terrain3d.cpp    every call into the Terrain3D GDExtension
 *  - tr_chunk_job.cpp               make -> math -> finalize for one chunk
 *  - tr_bench.cpp                   benchmark mode (--terraspline-bench), zero cost when off
 *  - tr_compositor_snapshot.cpp     read-only state snapshot for TerrainSplineStreamMap
 *
 * See Terraspline.md for the performance history and benchmark protocol.
 */
/**
 * @struct StreamSnapshot
 * @brief Read-only copy of the compositor's streaming state, in LOGICAL world coordinates (metres),
 * filled by TerrainSplineCompositor::get_stream_snapshot for the debug map. Heights are included
 * only for chunks that built a thumbnail (see set_thumbnail_size).
 */
struct StreamSnapshot {
	struct Chunk {
		Rect2 rect; // Logical world footprint
		int state = 0; // TerrainChunk::ChunkState
		const std::vector<float> *thumbnail = nullptr; // thumbnail_size² heights, or nullptr
	};
	struct Evicted {
		Rect2 rect;
		uint64_t time_msec = 0;
	};
	int chunk_size = 0;
	int thumbnail_size = 0;
	float render_radius = 0.0f;
	float physics_radius = 0.0f;
	Vector2 player; // Logical XZ
	Vector2 player_forward; // Unit XZ heading of the followed node (zero if unknown)
	Vector2 camera_forward; // Unit XZ heading of the active camera (zero if unknown)
	std::vector<Chunk> resident;
	std::vector<Rect2> queued;
	std::vector<Rect2> in_flight;
	std::vector<Evicted> evicted; // Recent evictions, oldest first
	std::vector<Rect2> spline_bounds; // Padded AABB of every spline
};

class TerrainSplineCompositor : public Node {
	GDCLASS(TerrainSplineCompositor,
			Node)

private:
	// ---- Configuration (exposed as properties) ----
	Node *terrain = nullptr; // The Terrain3D node
	int chunk_size = 256; // Power of two >= 64, metres (== pixels)
	float default_elevation = 0.0f;
	bool auto_apply = true; // Rebuild automatically when a spline changes
	float max_render_radius = 2048.0f; // Chunks resident within this distance of the player
	float max_physics_radius = 150.0f; // Scatter collision live within this distance
	Vector2 global_world_offset = Vector2(0.0f, 0.0f); // Accumulated origin shift (logical = physical + offset)
	Ref<Noise> global_terrain_noise; // Optional base terrain noise
	float global_terrain_amplitude = 50.0f;
	float generation_budget_ms = 4.0f; // Main-thread time per frame for chunk generation
	int max_jobs_in_flight = 0; // Async chunk jobs; 0 = hardware threads - 1

	// ---- Rebuild bookkeeping ----
	bool _rebuild_queued = false;
	bool compositor_full_rebuild = true;
	bool _rebuild_retry_pending = false;
	int _rebuild_retry_frames = 0;
	static constexpr int MAX_REBUILD_RETRY_FRAMES = 600; // ~10 s at 60 fps
	bool _warned_vertex_spacing = false;
	std::vector<uint64_t> _known_spline_ids; // For detecting added/removed splines

	// ---- Resident chunks ----
	HashMap<Vector2i, Ref<TerrainChunk>> chunk_buffers; // Keyed by physical chunk coordinate
	Node3D *scatter_container = nullptr; // Parent of all scattered MultiMeshInstance3Ds
	uint64_t last_eviction_check_time = 0;
	static constexpr uint64_t DISCOVERY_INTERVAL_MS = 500;

	// ---- Debug map support (tr_compositor_snapshot.cpp) ----
	int _thumbnail_size = 0; // 0 = no thumbnails; set by TerrainSplineStreamMap
	std::vector<StreamSnapshot::Evicted> _evicted_recent; // Ring of the last evictions (logical rects)
	static constexpr size_t EVICTED_LOG_MAX = 64;
	Rect2 _logical_chunk_rect(const Vector2i &p_physical_chunk) const;
	void _log_eviction(const Vector2i &p_physical_chunk);

	// ---- Streaming queue and async jobs (tr_compositor_stream.cpp) ----
	std::vector<Vector2i> _gen_queue;
	std::vector<Ref<ChunkJob>> _jobs_in_flight;
	bool _terrain_maps_dirty = false;
	int _frames_since_flush = 0;
	static constexpr int FLUSH_MAX_FRAMES = 6; // ~100 ms at 60 fps

	// ---- Lifecycle (tr_compositor.cpp) ----
	void _on_ready();
	void _on_process();
	bool _spline_set_changed();
	std::vector<ProceduralSpline3D *> _gather_splines() const;
	Vector3 _get_player_position() const;
	Vector2 _logical_player_position_2d() const;

	// ---- Rebuild (tr_compositor_rebuild.cpp) ----
	bool _is_terrain_ready();
	void _warn_if_vertex_spacing_mismatch();
	bool _collect_dirty_rect(
			const std::vector<ProceduralSpline3D *> &p_splines,
			Rect2 &r_rect
	) const;
	void _select_full_rebuild_chunks(
			const std::vector<ProceduralSpline3D *> &p_splines,
			const Vector2 &p_logical_player,
			HashMap<Vector2i,
					bool> &r_chunks
	) const;
	void _select_dirty_rect_chunks(
			const Rect2 &p_dirty_rect,
			const Vector2 &p_logical_player,
			HashMap<Vector2i,
					bool> &r_chunks
	) const;
	void _run_rebuild(
			const std::vector<Vector2i> &p_chunks,
			const std::vector<ProceduralSpline3D *> &p_splines,
			Object *p_target_api
	);

	// ---- Streaming (tr_compositor_stream.cpp) ----
	void _enqueue_chunks(
			const std::vector<Vector2i> &p_chunks,
			bool p_allow_existing
	);
	void _drain_generation_queue(uint64_t p_frame_start_usec);
	int _finalize_completed_jobs(
			Object *p_target_api,
			uint64_t p_frame_start_usec,
			uint64_t p_budget_usec
	);
	int _dispatch_queued_jobs(
			Object *p_target_api,
			uint64_t p_frame_start_usec,
			uint64_t p_budget_usec
	);
	void _sort_queue_nearest_first();
	void _flush_terrain_if_due(Object *p_target_api);
	int _effective_max_jobs_in_flight() const;

	// ---- Chunk job pipeline (tr_chunk_job.cpp) ----
	Ref<ChunkJob> _make_chunk_job(
			const Vector2i &p_chunk_pos,
			const std::vector<ProceduralSpline3D *> &p_splines
	);
	static void _run_chunk_job_math(
			const Ref<ChunkJob> &p_job,
			bool p_threaded
	);
	void _finalize_chunk_job(
			const Ref<ChunkJob> &p_job,
			Object *p_target_api
	);
	void _generate_chunks(
			const std::vector<Vector2i> &p_chunks,
			const std::vector<ProceduralSpline3D *> &p_splines,
			Object *p_target_api
	);
	void _wait_for_jobs_in_flight();
	Ref<TerrainChunk> _get_or_create_chunk(
			const Vector2i &p_chunk_pos,
			bool p_allow_create
	);

	// ---- Eviction, discovery, physics culling (tr_compositor_eviction.cpp) ----
	void _check_and_evict_far_chunks();
	void _evict_far_chunks(const Vector2 &p_logical_player);
	void _evict_far_regions(
			Object *p_target_api,
			const Vector2 &p_logical_player
	);
	void _discover_missing_chunks(const Vector2 &p_logical_player);
	void _check_chunk_physics_culling();
	void _update_chunk_physics(const Ref<TerrainChunk> &p_chunk);
	float _distance_to_chunk_edge(
			const Vector2i &p_chunk,
			const Vector2 &p_logical_player
	) const;

	// ---- Origin shift (tr_compositor_origin_shift.cpp) ----
	void _check_origin_shift();
	bool _compute_origin_shift(
			const Vector3 &p_target_pos,
			Vector3 &r_shift
	) const;
	void _apply_origin_shift(const Vector3 &p_shift);
	void _shift_terrain_targets(const Vector3 &p_shift);
	void _shift_spline_nodes(const Vector3 &p_shift);
	void _shift_chunk_buffers(const Vector3 &p_shift);

	// ---- Terrain3D adapters (tr_compositor_terrain3d.cpp) ----
	Object *_get_terrain_data_api() const;
	void _write_chunk_heights_to_terrain(
			Object *p_target_api,
			const Ref<TerrainChunk> &p_chunk,
			const Vector2 &p_offset
	);
	void _flush_terrain_maps(Object *p_target_api);
	void _refresh_terrain_collision();

	// ---- Benchmark mode (tr_bench.cpp; `-- --terraspline-bench`) ----
	bool _bench = false;
	bool _bench_done = false;
	std::vector<Vector2i> _bench_chunks;
	String _bench_dump_dir;
	uint64_t _bench_start_usec = 0;
	uint64_t _bench_first_frame = 0;
	HashMap<uint64_t, double> _bench_frame_ms; // process frame -> main-thread ms in compositor
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
	void _bench_dump_chunk(const Ref<TerrainChunk> &p_chunk);
	uint64_t _bench_content_hash(int &r_total_instances) const;

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
	void set_max_jobs_in_flight(int p_n) { max_jobs_in_flight = MAX(0, p_n); }
	int get_max_jobs_in_flight() const { return max_jobs_in_flight; }

	/// Coalesces rebuild requests into one deferred call per frame.
	void queue_rebuild();
	void _execute_rebuild();
	/// Decides which chunks need (re)generation and generates or enqueues them.
	void apply_all_splines();

	void _connect_spline(Node *p_node);
	void _disconnect_spline(Node *p_node);
	void _on_spline_changed();
	/// WorkerThreadPool entry point for one chunk's math.
	void _run_chunk_job_task(Ref<ChunkJob> p_job);

	// ---- Debug map (tr_compositor_snapshot.cpp) ----
	/// Chunks finalized from now on carry a p_size² height thumbnail (0 disables). Main thread only.
	void set_thumbnail_size(int p_size);
	int get_thumbnail_size() const { return _thumbnail_size; }
	/// Fills r_out with the current streaming state; thumbnail pointers are valid until the next frame.
	void get_stream_snapshot(StreamSnapshot &r_out) const;
};

} // namespace godot

#endif // TR_COMPOSITOR_H
