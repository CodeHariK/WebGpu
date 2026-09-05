#include "terraspline.h"
#include <godot_cpp/classes/time.hpp>
#include <godot_cpp/classes/camera3d.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/worker_thread_pool.hpp>
#include <godot_cpp/classes/worker_thread_pool.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/object.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include "../../game_manager/game_manager.h"
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <utility>
#include <vector>

namespace godot {

// =========================================================
// TerrainSplineCompositor Implementation
// =========================================================

/**
 * @brief Binds TerrainSplineCompositor methods and properties to Godot's ClassDB.
 * Exposes target terrain reference, chunk size parameters, render/physics radii, global offsets, and internal methods.
 */
void TerrainSplineCompositor::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_terrain", "terrain"), &TerrainSplineCompositor::set_terrain);
	ClassDB::bind_method(D_METHOD("get_terrain"), &TerrainSplineCompositor::get_terrain);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "terrain", PROPERTY_HINT_NODE_TYPE, "Node3D"), "set_terrain", "get_terrain");

	ClassDB::bind_method(D_METHOD("set_chunk_size", "p_size"), &TerrainSplineCompositor::set_chunk_size);
	ClassDB::bind_method(D_METHOD("get_chunk_size"), &TerrainSplineCompositor::get_chunk_size);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "chunk_size"), "set_chunk_size", "get_chunk_size");

	ClassDB::bind_method(D_METHOD("set_default_elevation", "elevation"), &TerrainSplineCompositor::set_default_elevation);
	ClassDB::bind_method(D_METHOD("get_default_elevation"), &TerrainSplineCompositor::get_default_elevation);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "default_elevation"), "set_default_elevation", "get_default_elevation");


	ClassDB::bind_method(D_METHOD("set_auto_apply", "auto_apply"), &TerrainSplineCompositor::set_auto_apply);
	ClassDB::bind_method(D_METHOD("get_auto_apply"), &TerrainSplineCompositor::get_auto_apply);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "auto_apply"), "set_auto_apply", "get_auto_apply");

	ClassDB::bind_method(D_METHOD("set_apply_now", "apply_now"), &TerrainSplineCompositor::set_apply_now);
	ClassDB::bind_method(D_METHOD("get_apply_now"), &TerrainSplineCompositor::get_apply_now);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "apply_now"), "set_apply_now", "get_apply_now");

	ClassDB::bind_method(D_METHOD("set_max_render_radius", "p_radius"), &TerrainSplineCompositor::set_max_render_radius);
	ClassDB::bind_method(D_METHOD("get_max_render_radius"), &TerrainSplineCompositor::get_max_render_radius);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "max_render_radius"), "set_max_render_radius", "get_max_render_radius");

	ClassDB::bind_method(D_METHOD("set_max_physics_radius", "p_radius"), &TerrainSplineCompositor::set_max_physics_radius);
	ClassDB::bind_method(D_METHOD("get_max_physics_radius"), &TerrainSplineCompositor::get_max_physics_radius);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "max_physics_radius"), "set_max_physics_radius", "get_max_physics_radius");

	ClassDB::bind_method(D_METHOD("set_global_world_offset", "p_offset"), &TerrainSplineCompositor::set_global_world_offset);
	ClassDB::bind_method(D_METHOD("get_global_world_offset"), &TerrainSplineCompositor::get_global_world_offset);
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "global_world_offset"), "set_global_world_offset", "get_global_world_offset");

	ClassDB::bind_method(D_METHOD("set_global_terrain_noise", "noise"), &TerrainSplineCompositor::set_global_terrain_noise);
	ClassDB::bind_method(D_METHOD("get_global_terrain_noise"), &TerrainSplineCompositor::get_global_terrain_noise);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "global_terrain_noise", PROPERTY_HINT_RESOURCE_TYPE, "Noise"), "set_global_terrain_noise", "get_global_terrain_noise");

	ClassDB::bind_method(D_METHOD("set_global_terrain_amplitude", "amp"), &TerrainSplineCompositor::set_global_terrain_amplitude);
	ClassDB::bind_method(D_METHOD("get_global_terrain_amplitude"), &TerrainSplineCompositor::get_global_terrain_amplitude);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "global_terrain_amplitude"), "set_global_terrain_amplitude", "get_global_terrain_amplitude");

	ClassDB::bind_method(D_METHOD("set_generation_budget_ms", "ms"), &TerrainSplineCompositor::set_generation_budget_ms);
	ClassDB::bind_method(D_METHOD("get_generation_budget_ms"), &TerrainSplineCompositor::get_generation_budget_ms);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "generation_budget_ms", PROPERTY_HINT_RANGE, "0.5,32,0.5"), "set_generation_budget_ms", "get_generation_budget_ms");

	ClassDB::bind_method(D_METHOD("set_max_jobs_in_flight", "n"), &TerrainSplineCompositor::set_max_jobs_in_flight);
	ClassDB::bind_method(D_METHOD("get_max_jobs_in_flight"), &TerrainSplineCompositor::get_max_jobs_in_flight);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "max_jobs_in_flight", PROPERTY_HINT_RANGE, "0,16,1"), "set_max_jobs_in_flight", "get_max_jobs_in_flight");
	ClassDB::bind_method(D_METHOD("_run_chunk_job_task", "job"), &TerrainSplineCompositor::_run_chunk_job_task);

	ClassDB::bind_method(D_METHOD("queue_rebuild"), &TerrainSplineCompositor::queue_rebuild);
	ClassDB::bind_method(D_METHOD("_execute_rebuild"), &TerrainSplineCompositor::_execute_rebuild);
	ClassDB::bind_method(D_METHOD("apply_all_splines"), &TerrainSplineCompositor::apply_all_splines);
	ClassDB::bind_method(D_METHOD("_connect_spline", "node"), &TerrainSplineCompositor::_connect_spline);
	ClassDB::bind_method(D_METHOD("_disconnect_spline", "node"), &TerrainSplineCompositor::_disconnect_spline);
	ClassDB::bind_method(D_METHOD("_on_spline_changed"), &TerrainSplineCompositor::_on_spline_changed);
}

/**
 * @brief Default Constructor. Initializes default values for rendering and physics radii.
 */
TerrainSplineCompositor::TerrainSplineCompositor() {
	max_render_radius = 2048.0f;
	max_physics_radius = 150.0f;
	last_eviction_check_time = 0;
	global_world_offset = Vector2(0.0f, 0.0f);
	scatter_container = nullptr;
	global_terrain_amplitude = 50.0f;
}

void TerrainSplineCompositor::set_global_terrain_noise(const Ref<Noise> &p_noise) {
	global_terrain_noise = p_noise;
	queue_rebuild();
}

Ref<Noise> TerrainSplineCompositor::get_global_terrain_noise() const {
	return global_terrain_noise;
}

void TerrainSplineCompositor::set_global_terrain_amplitude(float p_amp) {
	global_terrain_amplitude = p_amp;
	queue_rebuild();
}

float TerrainSplineCompositor::get_global_terrain_amplitude() const {
	return global_terrain_amplitude;
}

/**
 * @brief Default Destructor.
 */
TerrainSplineCompositor::~TerrainSplineCompositor() {}
void TerrainSplineCompositor::set_terrain(Node *p_terrain) { terrain = p_terrain; }
Node *TerrainSplineCompositor::get_terrain() const { return terrain; }
void TerrainSplineCompositor::set_chunk_size(int p_size) {
	// Must be a power of two >= 64 so the 4096 m origin shift is a whole number of chunks.
	int sz = MAX(64, p_size);
	int pow2 = 64;
	while (pow2 < sz) {
		pow2 <<= 1;
	}
	if (pow2 != p_size) {
		UtilityFunctions::print("[Compositor] chunk_size ", p_size, " rounded up to power of two: ", pow2);
	}
	chunk_size = pow2;
}
int TerrainSplineCompositor::get_chunk_size() const { return chunk_size; }
void TerrainSplineCompositor::set_default_elevation(float p_elev) {
	if (default_elevation != p_elev) {
		default_elevation = p_elev;
		queue_rebuild();
	}
}
float TerrainSplineCompositor::get_default_elevation() const { return default_elevation; }
void TerrainSplineCompositor::set_auto_apply(bool p_auto) { auto_apply = p_auto; }
bool TerrainSplineCompositor::get_auto_apply() const { return auto_apply; }
void TerrainSplineCompositor::set_apply_now(bool p_apply) {
	if (p_apply) {
		compositor_full_rebuild = true;
		apply_all_splines();
	}
}
bool TerrainSplineCompositor::get_apply_now() const { return false; }

void TerrainSplineCompositor::set_max_render_radius(float p_radius) {
	max_render_radius = p_radius;
}
float TerrainSplineCompositor::get_max_render_radius() const {
	return max_render_radius;
}

void TerrainSplineCompositor::set_max_physics_radius(float p_radius) {
	max_physics_radius = p_radius;
}
float TerrainSplineCompositor::get_max_physics_radius() const {
	return max_physics_radius;
}

void TerrainSplineCompositor::set_generation_budget_ms(float p_ms) { generation_budget_ms = MAX(0.5f, p_ms); }
float TerrainSplineCompositor::get_generation_budget_ms() const { return generation_budget_ms; }

void TerrainSplineCompositor::set_global_world_offset(Vector2 p_offset) {
	global_world_offset = p_offset;
}
Vector2 TerrainSplineCompositor::get_global_world_offset() const {
	return global_world_offset;
}

Vector3 TerrainSplineCompositor::_get_player_position() const {
	if (_bench) {
		return Vector3(0.0f, 0.0f, 0.0f);
	}
	if (GameManager::get_singleton()) {
		Node3D *active_target = Object::cast_to<Node3D>(GameManager::get_singleton()->get_active_target());
		if (active_target) {
			return active_target->get_global_position();
		}
	}
	if (terrain) {
		// Terrain3D exposes the nodes it follows, not a position.
		Node3D *target = Object::cast_to<Node3D>(terrain->call("get_collision_target"));
		if (!target) {
			target = Object::cast_to<Node3D>(terrain->call("get_camera"));
		}
		if (target && target->is_inside_tree()) {
			return target->get_global_position();
		}
	}
	return Vector3(0.0f, 0.0f, 0.0f);
}

/**
 * @brief Handles engine-level notifications (Ready, Process, Child Order Changed).
 * - NOTIFICATION_READY: Sets up internal visual containers, connects signals, and triggers initial rebuild.
 * - NOTIFICATION_CHILD_ORDER_CHANGED: Flags a full rebuild when child splines sibling hierarchy changes.
 * - NOTIFICATION_PROCESS: Triggers player-centric culling, physics checks, and periodic evictions.
 */
void TerrainSplineCompositor::_notification(int p_what) {
	if (p_what == Node::NOTIFICATION_READY) {
		scatter_container = memnew(Node3D);
		scatter_container->set_name("InternalScatterContainer");
		add_child(scatter_container, false, Node::INTERNAL_MODE_BACK);

		TypedArray<Node> children = get_children();
		for (int i = 0; i < children.size(); ++i) {
			_connect_spline(Object::cast_to<Node>(children[i]));
		}
		connect("child_entered_tree", Callable(this, "_connect_spline"));
		connect("child_exiting_tree", Callable(this, "_disconnect_spline"));
		set_process(true);
		_bench_init();
		call_deferred("apply_all_splines");
	} else if (p_what == Node::NOTIFICATION_CHILD_ORDER_CHANGED) {
		// Reordering children doesn't change the terrain; only an added/removed spline does.
		if (_spline_set_changed()) {
			UtilityFunctions::print("[Compositor] Spline set changed! Flagging full rebuild.");
			compositor_full_rebuild = true;
			queue_rebuild();
		}
	} else if (p_what == Node::NOTIFICATION_PROCESS) {
		uint64_t t_proc_start = Time::get_singleton()->get_ticks_usec();
		if (_rebuild_retry_pending) {
			if (_rebuild_retry_frames < MAX_REBUILD_RETRY_FRAMES) {
				_rebuild_retry_frames++;
				queue_rebuild();
			} else if (_rebuild_retry_frames == MAX_REBUILD_RETRY_FRAMES) {
				_rebuild_retry_frames++; // Report once, then stop retrying.
				UtilityFunctions::printerr("[Compositor] Terrain3D never became ready (is the 'terrain' node in the scene tree?). Giving up on automatic rebuild.");
			}
		}
		_check_origin_shift();
		_check_chunk_physics_culling();
		uint64_t msec = Time::get_singleton()->get_ticks_msec();
		if (msec - last_eviction_check_time >= DISCOVERY_INTERVAL_MS) {
			last_eviction_check_time = msec;
			_check_and_evict_far_chunks(); // Evicts far chunks and *enqueues* missing ones.
		}
		_drain_generation_queue(t_proc_start);
		_bench_add_frame_time(Time::get_singleton()->get_ticks_usec() - t_proc_start);
		_bench_check_done();
	}
}

/**
 * @brief Returns true (and updates the cache) if the set of child ProceduralSpline3D instance ids changed.
 */
bool TerrainSplineCompositor::_spline_set_changed() {
	std::vector<uint64_t> current;
	TypedArray<Node> children = get_children();
	for (int i = 0; i < children.size(); ++i) {
		ProceduralSpline3D *spline = Object::cast_to<ProceduralSpline3D>(children[i]);
		if (spline) {
			current.push_back(spline->get_instance_id());
		}
	}
	std::sort(current.begin(), current.end());
	if (current == _known_spline_ids) {
		return false;
	}
	_known_spline_ids = current;
	return true;
}

/**
 * @brief Connects child ProceduralSpline3D nodes' change signals to trigger compositor rebuilds.
 */
void TerrainSplineCompositor::_connect_spline(Node *p_node) {
	ProceduralSpline3D *spline = Object::cast_to<ProceduralSpline3D>(p_node);
	if (spline) {
		if (!spline->is_connected("spline_changed", Callable(this, "_on_spline_changed"))) {
			spline->connect("spline_changed", Callable(this, "_on_spline_changed"));
		}
	}
}

void TerrainSplineCompositor::_disconnect_spline(Node *p_node) {
	ProceduralSpline3D *spline = Object::cast_to<ProceduralSpline3D>(p_node);
	if (spline) {
		if (spline->is_connected("spline_changed", Callable(this, "_on_spline_changed"))) {
			spline->disconnect("spline_changed", Callable(this, "_on_spline_changed"));
		}
		compositor_full_rebuild = true;
		queue_rebuild();
	}
}

/**
 * @brief Callback triggered when a spline node notifies the compositor of spatial or shape modifications.
 */
void TerrainSplineCompositor::_on_spline_changed() {
	if (auto_apply)
		queue_rebuild();
}

/**
 * @brief Queues a deferred rebuild task, preventing multiple updates within a single frame.
 */
void TerrainSplineCompositor::queue_rebuild() {
	if (!_rebuild_queued) {
		_rebuild_queued = true;
		call_deferred("_execute_rebuild");
	}
}

/**
 * @brief Executes the deferred rebuild task by running a full spline evaluation and application.
 */
void TerrainSplineCompositor::_execute_rebuild() {
	_rebuild_queued = false;
	uint64_t t0 = Time::get_singleton()->get_ticks_usec();
	apply_all_splines();
	_bench_add_frame_time(Time::get_singleton()->get_ticks_usec() - t0);
}

/**
 * @brief Rebuilds and coordinates the entire spline terrain master grid.
 * Merges bounding boxes of modified splines, determines active viewport chunk coords,
 * and schedules localized dirty updates or global regenerations accordingly.
 */
void TerrainSplineCompositor::apply_all_splines() {
	uint64_t t_start = Time::get_singleton()->get_ticks_usec();

	if (!terrain) {
		UtilityFunctions::printerr("[Compositor] ABORT: Terrain3D node not assigned.");
		return;
	}

	Variant api_var = terrain->get("data");
	if (api_var.get_type() == Variant::NIL || (api_var.get_type() == Variant::OBJECT && (Object *)api_var == nullptr)) {
		api_var = terrain->get("storage");
	}
	Object *target_api = (api_var.get_type() == Variant::OBJECT) ? (Object *)api_var : nullptr;
	if (!target_api) {
		UtilityFunctions::printerr("[Compositor] ABORT: Terrain3D Data/Storage is not initialized!");
		return;
	}

	// Terrain3DData is only usable once the Terrain3D node has entered the tree and
	// initialized its data (region_size > 0). Calling into it earlier yields
	// NaN/INT_MIN region locations and can hang inside Terrain3D (region_size == 0).
	// This happens e.g. while the editor is still instantiating the scene.
	// Terrain3D initializes its data on ENTER_TREE, so "both in tree" is the readiness condition.
	// (region_size lives on the Terrain3D node, not on Terrain3DData.)
	if (!is_inside_tree() || !terrain->is_inside_tree() || (int)terrain->call("get_region_size") <= 0) {
		if (!_rebuild_retry_pending) {
			UtilityFunctions::print("[Compositor] Terrain3D not ready yet; will retry on next process frame.");
		}
		_rebuild_retry_pending = true; // Retried from NOTIFICATION_PROCESS (or READY's deferred apply).
		compositor_full_rebuild = true;
		return;
	}
	_rebuild_retry_pending = false;
	_rebuild_retry_frames = 0;

	// Workers read the splines' baked caches; finish them before anything below can rebake.
	_wait_for_jobs_in_flight();

	// Heightmaps are stamped at 1 pixel per metre; Terrain3D must agree or chunks import at the wrong scale.
	float vertex_spacing = terrain->call("get_vertex_spacing");
	if (!Math::is_equal_approx(vertex_spacing, 1.0f)) {
		if (!_warned_vertex_spacing) {
			_warned_vertex_spacing = true;
			UtilityFunctions::printerr("[Compositor] Terrain3D vertex_spacing is ", vertex_spacing, " but the compositor assumes 1.0. Chunks will import at the wrong scale.");
		}
	}

	TypedArray<Node> children = get_children();
	std::vector<ProceduralSpline3D *> splines;
	for (int i = 0; i < children.size(); ++i) {
		ProceduralSpline3D *spline = Object::cast_to<ProceduralSpline3D>(children[i]);
		if (spline)
			splines.push_back(spline);
	}

	uint64_t t_gather = Time::get_singleton()->get_ticks_usec();

	Rect2 master_dirty_rect;
	bool has_master_dirty = false;

	for (ProceduralSpline3D *spline : splines) {
		if (spline->get_is_dirty()) {
			Rect2 sd = spline->consume_dirty_rect();
			if (sd.has_area()) {
				if (!has_master_dirty) {
					master_dirty_rect = sd;
					has_master_dirty = true;
				} else {
					master_dirty_rect = master_dirty_rect.merge(sd);
				}
			}
		}
	}

	HashMap<Vector2i, bool> active_grid_chunks;

	Vector3 target_pos = _get_player_position();
	Vector2 player_pos_2d(target_pos.x, target_pos.z);
	Vector2 logical_player_pos_2d = player_pos_2d + global_world_offset;

	bool is_editor = Engine::get_singleton()->is_editor_hint();

	if (compositor_full_rebuild) {
#if DEBUG
		UtilityFunctions::print("\n=== [Terrain Master Grid] GLOBAL REBUILD STARTED ===");
		UtilityFunctions::print("[Compositor] Chunks in buffer: ", (int)chunk_buffers.size(),
								" | Player logical pos: ", logical_player_pos_2d,
								" | Render radius: ", max_render_radius, "m");
#endif

		if (is_editor) {
			// In the editor, generate chunks for all splines
			for (ProceduralSpline3D *spline : splines) {
				Rect2 sd = spline->get_padded_aabb();
				int s_min_cx = (int)Math::floor(sd.position.x / chunk_size);
				int s_max_cx = (int)Math::floor((sd.position.x + sd.size.x) / chunk_size);
				int s_min_cz = (int)Math::floor(sd.position.y / chunk_size);
				int s_max_cz = (int)Math::floor((sd.position.y + sd.size.y) / chunk_size);
				for (int cx = s_min_cx; cx <= s_max_cx; ++cx) {
					for (int cz = s_min_cz; cz <= s_max_cz; ++cz) {
						active_grid_chunks[Vector2i(cx, cz)] = true;
					}
				}
			}
		} else {
			int player_cx = (int)Math::floor(logical_player_pos_2d.x / chunk_size);
			int player_cz = (int)Math::floor(logical_player_pos_2d.y / chunk_size);
			int radius_chunks = (int)Math::ceil(max_render_radius / chunk_size);

			if (_bench) {
				for (const Vector2i &c : _bench_chunks) {
					active_grid_chunks[c] = true;
				}
				radius_chunks = -1; // skip the radius loop below
			}

			for (int cx = player_cx - radius_chunks; cx <= player_cx + radius_chunks; ++cx) {
				for (int cz = player_cz - radius_chunks; cz <= player_cz + radius_chunks; ++cz) {
					Vector2 c_center((cx + 0.5f) * chunk_size, (cz + 0.5f) * chunk_size);
					Vector2 logical_c_center = c_center + global_world_offset;
					if (logical_player_pos_2d.distance_to(logical_c_center) <= max_render_radius) {
						active_grid_chunks[Vector2i(cx, cz)] = true;
					}
				}
			}
		}
	} else {
		if (!has_master_dirty)
			return;
#if DEBUG
		UtilityFunctions::print("\n=== [Terrain Master Grid] LOCAL DIRTY RECT UPDATE ===");
		UtilityFunctions::print("[Compositor] Chunks in buffer: ", (int)chunk_buffers.size(),
								" | Player logical pos: ", logical_player_pos_2d,
								" | Dirty Rect: ", master_dirty_rect);
#endif

		int min_cx = (int)Math::floor(master_dirty_rect.position.x / chunk_size);
		int max_cx = (int)Math::floor((master_dirty_rect.position.x + master_dirty_rect.size.x) / chunk_size);
		int min_cz = (int)Math::floor(master_dirty_rect.position.y / chunk_size);
		int max_cz = (int)Math::floor((master_dirty_rect.position.y + master_dirty_rect.size.y) / chunk_size);

		for (int cx = min_cx; cx <= max_cx; ++cx) {
			for (int cz = min_cz; cz <= max_cz; ++cz) {
				if (is_editor) {
					active_grid_chunks[Vector2i(cx, cz)] = true;
				} else {
					Vector2 c_center((cx + 0.5f) * chunk_size, (cz + 0.5f) * chunk_size);
					Vector2 logical_c_center = c_center + global_world_offset;
					if (logical_player_pos_2d.distance_to(logical_c_center) <= max_render_radius) {
						active_grid_chunks[Vector2i(cx, cz)] = true;
					}
				}
			}
		}
	}

	compositor_full_rebuild = false;

#if DEBUG
	uint64_t t_grid_calc = Time::get_singleton()->get_ticks_usec();
	UtilityFunctions::print("[Compositor] Grid chunk calculation time: ", (t_grid_calc - t_gather) / 1000.0, " ms.");
#endif

	if (active_grid_chunks.size() == 0) {
#if DEBUG
		UtilityFunctions::print("[Compositor] 0 active chunks to update. Aborting.");
#endif
		return;
	}

#if DEBUG
	UtilityFunctions::print(" -> Recalculating exactly ", (int)active_grid_chunks.size(), " isolated chunks...");
#endif

	std::vector<Vector2i> chunks_to_generate;
	for (const KeyValue<Vector2i, bool> &E : active_grid_chunks) {
		chunks_to_generate.push_back(E.key);
	}

	if (!chunks_to_generate.empty()) {
		if (is_editor) {
			// Editor: immediate feedback on spline edits is worth the stall.
			_generate_chunks(chunks_to_generate, splines, target_api);
			_flush_terrain_maps(target_api);
			_refresh_terrain_collision();
		} else {
			// Game: spread the work over frames under generation_budget_ms.
			_enqueue_chunks(chunks_to_generate, /*allow_existing=*/true);
		}
	}

	// Update physics culling to immediately activate physics inside the radius
	_check_chunk_physics_culling();

#if DEBUG
	uint64_t t_end = Time::get_singleton()->get_ticks_usec();
	UtilityFunctions::print("=== [Terrain Master Grid] TOTAL UPDATE TIME: ", (t_end - t_start) / 1000.0, " ms ===\n");
#endif
}

/**
 * @brief Floating Origin Precision System.
 * Triggers when the player is more than 4096 meters from the center. Shifts camera targets, player
 * visual nodes, splines, and chunk buffer coordinates by the threshold to maintain single-float precision.
 */
void TerrainSplineCompositor::_check_origin_shift() {
	if (!terrain) {
		return;
	}

	Vector3 target_pos = _get_player_position();

	// Check if player's X or Z distance exceeds 4096 meters
	if (Math::abs(target_pos.x) > 4096.0f || Math::abs(target_pos.z) > 4096.0f) {
		Vector3 shift(0.0f, 0.0f, 0.0f);
		if (target_pos.x > 4096.0f) {
			shift.x = 4096.0f;
		} else if (target_pos.x < -4096.0f) {
			shift.x = -4096.0f;
		}
		if (target_pos.z > 4096.0f) {
			shift.z = 4096.0f;
		} else if (target_pos.z < -4096.0f) {
			shift.z = -4096.0f;
		}

#if DEBUG
		UtilityFunctions::print("[Compositor] Origin shift triggered! Shifting world by: ", shift);
#endif

		// 1. Subtract shift from Player/Camera targets
		Node3D *col_target = Object::cast_to<Node3D>(terrain->call("get_collision_target"));
		if (col_target) {
			col_target->set_global_position(col_target->get_global_position() - shift);
		}
		Node3D *clip_target = Object::cast_to<Node3D>(terrain->call("get_clipmap_target"));
		if (clip_target && clip_target != col_target) {
			clip_target->set_global_position(clip_target->get_global_position() - shift);
		}
		Node3D *cam_target = Object::cast_to<Node3D>(terrain->call("get_camera"));
		if (cam_target && cam_target != col_target && cam_target != clip_target) {
			cam_target->set_global_position(cam_target->get_global_position() - shift);
		}

		// 2. Add shift to global_world_offset
		global_world_offset += Vector2(shift.x, shift.z);

		// 3. Loop through every ProceduralSpline3D child and subtract shift from physical position
		TypedArray<Node> children = get_children();
		for (int i = 0; i < children.size(); ++i) {
			ProceduralSpline3D *spline = Object::cast_to<ProceduralSpline3D>(children[i]);
			if (spline) {
				spline->set_global_position(spline->get_global_position() - shift);
			}
		}

		// 4. Subtract shift from Terrain3D node's physical position
		Node3D *terrain_3d = Object::cast_to<Node3D>(terrain);
		if (terrain_3d) {
			terrain_3d->set_global_position(terrain_3d->get_global_position() - shift);
		}

		// 4b. The scattered MultiMeshInstance3Ds live under scatter_container in world space;
		// shift the container so visuals stay aligned with the (shifted) physics caches below.
		if (scatter_container) {
			scatter_container->set_global_position(scatter_container->get_global_position() - shift);
		}

		// 5. Shift compositor's chunk_buffers coordinates
		Vector2i chunk_shift(shift.x / chunk_size, shift.z / chunk_size);
		if (chunk_shift != Vector2i(0, 0)) {
			HashMap<Vector2i, Ref<TerrainChunk>> new_chunk_buffers;
			for (const KeyValue<Vector2i, Ref<TerrainChunk>> &E : chunk_buffers) {
				new_chunk_buffers[E.key - chunk_shift] = E.value;
			}
			chunk_buffers = new_chunk_buffers;
		}

		// Update the cached physics transforms for all chunks so they match the visual shift
		for (const KeyValue<Vector2i, Ref<TerrainChunk>> &E : chunk_buffers) {
			if (E.value.is_valid()) {
				for (TerrainChunk::PhysicsCache &pc : E.value->get_physics_caches()) {
					for (Transform3D &t : pc.transforms) {
						t.origin -= shift;
					}
				}
				// If the chunk is currently active in the physics server, we must also shift the live bodies
				RID body_rid = E.value->get_physics_body_rid();
				if (body_rid.is_valid()) {
					Transform3D body_transform = PhysicsServer3D::get_singleton()->body_get_state(body_rid, PhysicsServer3D::BODY_STATE_TRANSFORM);
					body_transform.origin -= shift;
					PhysicsServer3D::get_singleton()->body_set_state(body_rid, PhysicsServer3D::BODY_STATE_TRANSFORM, body_transform);
				}
			}
		}

		// 6. Reset target positions in Terrain3D to prevent popping/lag
		terrain->call("snap");
	}
}

/**
 * @brief Uploads all regions added with update=false since the last flush (one GPU rebuild per batch).
 */
void TerrainSplineCompositor::_flush_terrain_maps(Object *p_target_api) {
	if (!_terrain_maps_dirty || !p_target_api) {
		return;
	}
	uint64_t t0 = Time::get_singleton()->get_ticks_usec();
	p_target_api->call("update_maps"); // TYPE_MAX, all_regions=true, generate_mipmaps=false
	_terrain_maps_dirty = false;
	_frames_since_flush = 0;
	if (_bench) {
		_bench_flush_ms += (Time::get_singleton()->get_ticks_usec() - t0) / 1000.0;
		_bench_flushes++;
	}
}

void TerrainSplineCompositor::_refresh_terrain_collision() {
	if (!terrain) {
		return;
	}
	Variant col_var = terrain->get("collision");
	Object *collision = (col_var.get_type() == Variant::OBJECT) ? (Object *)col_var : nullptr;
	if (collision && collision->has_method("update")) {
		uint64_t t0 = Time::get_singleton()->get_ticks_usec();
		collision->call("update", true);
		if (_bench) {
			_bench_collision_ms += (Time::get_singleton()->get_ticks_usec() - t0) / 1000.0;
		}
	}
}

/**
 * @brief Adds chunk coordinates to the generation queue (deduplicated).
 * With p_allow_existing false, chunks that are already resident are skipped (streaming discovery);
 * with true they are regenerated (spline edits / full rebuilds).
 */
void TerrainSplineCompositor::_enqueue_chunks(const std::vector<Vector2i> &p_chunks, bool p_allow_existing) {
	for (const Vector2i &c : p_chunks) {
		if (!p_allow_existing && chunk_buffers.has(c)) {
			continue;
		}
		if (std::find(_gen_queue.begin(), _gen_queue.end(), c) == _gen_queue.end()) {
			_gen_queue.push_back(c);
		}
	}
}

/**
 * @brief Streams queued chunks without stalling the frame.
 *  1. Finalize any in-flight jobs whose math has finished (main-thread work: MultiMesh nodes,
 *     physics caches, Terrain3D region), stopping once generation_budget_ms of this frame is spent.
 *  2. Dispatch new jobs (nearest chunk first) up to max_jobs_in_flight; their math runs on
 *     WorkerThreadPool threads.
 *  3. If anything was finalized, upload to Terrain3D once and refresh collision.
 */
void TerrainSplineCompositor::_drain_generation_queue(uint64_t p_frame_start_usec) {
	if ((_gen_queue.empty() && _jobs_in_flight.empty()) || !terrain || !terrain->is_inside_tree()) {
		return;
	}
	Variant api_var = terrain->get("data");
	Object *target_api = (api_var.get_type() == Variant::OBJECT) ? (Object *)api_var : nullptr;
	if (!target_api) {
		return;
	}
	WorkerThreadPool *wtp = WorkerThreadPool::get_singleton();
	const uint64_t budget_usec = (uint64_t)(generation_budget_ms * 1000.0f);
	auto over_budget = [&]() { return Time::get_singleton()->get_ticks_usec() - p_frame_start_usec >= budget_usec; };

	// 1. Finalize completed jobs (at least one per frame if any is ready, so progress is guaranteed).
	int finalized = 0;
	for (size_t i = 0; i < _jobs_in_flight.size();) {
		Ref<ChunkJob> job = _jobs_in_flight[i];
		bool done = !wtp || job->task_id < 0 || wtp->is_task_completed(job->task_id);
		if (!done || (finalized > 0 && over_budget())) {
			++i;
			continue;
		}
		if (wtp && job->task_id >= 0) {
			wtp->wait_for_task_completion(job->task_id); // Already complete; releases the task.
		}
		if (job->world_offset_at_dispatch != global_world_offset) {
			// An origin shift happened mid-flight; the transforms are stale. Regenerate.
			_enqueue_chunks({ job->chunk_pos }, /*allow_existing=*/true);
		} else {
			uint64_t tf0 = Time::get_singleton()->get_ticks_usec();
			_finalize_chunk_job(job, target_api);
			if (_bench) {
				_bench_finalize_ms += (Time::get_singleton()->get_ticks_usec() - tf0) / 1000.0;
			}
			finalized++;
		}
		_jobs_in_flight.erase(_jobs_in_flight.begin() + i);
	}

	// 2. Dispatch new jobs, nearest first.
	if (!_gen_queue.empty()) {
		Vector3 target_pos = _get_player_position();
		Vector2 logical_player_pos_2d = Vector2(target_pos.x, target_pos.z) + global_world_offset;
		auto chunk_dist = [&](const Vector2i &c) {
			Vector2 center = Vector2((c.x + 0.5f) * chunk_size, (c.y + 0.5f) * chunk_size) + global_world_offset;
			return logical_player_pos_2d.distance_squared_to(center);
		};
		std::sort(_gen_queue.begin(), _gen_queue.end(), [&](const Vector2i &a, const Vector2i &b) {
			return chunk_dist(a) > chunk_dist(b); // farthest first so pop_back() yields nearest
		});

		TypedArray<Node> children = get_children();
		std::vector<ProceduralSpline3D *> splines;
		for (int i = 0; i < children.size(); ++i) {
			ProceduralSpline3D *spline = Object::cast_to<ProceduralSpline3D>(children[i]);
			if (spline) {
				splines.push_back(spline);
			}
		}

		int max_in_flight = max_jobs_in_flight > 0 ? max_jobs_in_flight : MAX(1, OS::get_singleton()->get_processor_count() - 1);
		while (!_gen_queue.empty() && (int)_jobs_in_flight.size() < max_in_flight && !over_budget()) {
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
				_finalize_chunk_job(job, target_api);
				finalized++;
			}
		}
	}

	// 3. Upload + collision refresh. Terrain3D's update_maps rebuilds the whole texture array
	// (~7 ms at 25 regions) so it is throttled: flush when the pipeline goes idle, or at most every
	// FLUSH_MAX_FRAMES frames while chunks keep streaming in.
	if (_terrain_maps_dirty) {
		_frames_since_flush++;
		bool idle = _gen_queue.empty() && _jobs_in_flight.empty();
		if (idle || _frames_since_flush >= FLUSH_MAX_FRAMES) {
			_flush_terrain_maps(target_api);
			_refresh_terrain_collision();
		}
	}
}

} //namespace godot
