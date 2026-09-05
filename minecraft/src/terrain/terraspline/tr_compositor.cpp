/**
 * @file tr_compositor.cpp
 * @brief TerrainSplineCompositor: bindings, properties, lifecycle and signal wiring.
 *
 * The heavier responsibilities live in sibling files; see tr_compositor.h for the map.
 */
#include "tr_compositor.h"
#include "../../game_manager/game_manager.h"
#include <algorithm>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/time.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

// ---------------------------------------------------------------------------------------------
// Bindings
// ---------------------------------------------------------------------------------------------

void TerrainSplineCompositor::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_terrain", "terrain"), &TerrainSplineCompositor::set_terrain);
	ClassDB::bind_method(D_METHOD("get_terrain"), &TerrainSplineCompositor::get_terrain);
	ADD_PROPERTY(
			PropertyInfo(Variant::OBJECT, "terrain", PROPERTY_HINT_NODE_TYPE, "Node3D"), "set_terrain", "get_terrain"
	);

	ClassDB::bind_method(D_METHOD("set_chunk_size", "p_size"), &TerrainSplineCompositor::set_chunk_size);
	ClassDB::bind_method(D_METHOD("get_chunk_size"), &TerrainSplineCompositor::get_chunk_size);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "chunk_size"), "set_chunk_size", "get_chunk_size");

	ClassDB::bind_method(
			D_METHOD("set_default_elevation", "elevation"), &TerrainSplineCompositor::set_default_elevation
	);
	ClassDB::bind_method(D_METHOD("get_default_elevation"), &TerrainSplineCompositor::get_default_elevation);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "default_elevation"), "set_default_elevation", "get_default_elevation");

	ClassDB::bind_method(D_METHOD("set_auto_apply", "auto_apply"), &TerrainSplineCompositor::set_auto_apply);
	ClassDB::bind_method(D_METHOD("get_auto_apply"), &TerrainSplineCompositor::get_auto_apply);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "auto_apply"), "set_auto_apply", "get_auto_apply");

	ClassDB::bind_method(D_METHOD("set_apply_now", "apply_now"), &TerrainSplineCompositor::set_apply_now);
	ClassDB::bind_method(D_METHOD("get_apply_now"), &TerrainSplineCompositor::get_apply_now);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "apply_now"), "set_apply_now", "get_apply_now");

	ClassDB::bind_method(
			D_METHOD("set_max_render_radius", "p_radius"), &TerrainSplineCompositor::set_max_render_radius
	);
	ClassDB::bind_method(D_METHOD("get_max_render_radius"), &TerrainSplineCompositor::get_max_render_radius);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "max_render_radius"), "set_max_render_radius", "get_max_render_radius");

	ClassDB::bind_method(
			D_METHOD("set_max_physics_radius", "p_radius"), &TerrainSplineCompositor::set_max_physics_radius
	);
	ClassDB::bind_method(D_METHOD("get_max_physics_radius"), &TerrainSplineCompositor::get_max_physics_radius);
	ADD_PROPERTY(
			PropertyInfo(Variant::FLOAT, "max_physics_radius"), "set_max_physics_radius", "get_max_physics_radius"
	);

	ClassDB::bind_method(
			D_METHOD("set_global_world_offset", "p_offset"), &TerrainSplineCompositor::set_global_world_offset
	);
	ClassDB::bind_method(D_METHOD("get_global_world_offset"), &TerrainSplineCompositor::get_global_world_offset);
	ADD_PROPERTY(
			PropertyInfo(Variant::VECTOR2, "global_world_offset"), "set_global_world_offset", "get_global_world_offset"
	);

	ClassDB::bind_method(
			D_METHOD("set_global_terrain_noise", "noise"), &TerrainSplineCompositor::set_global_terrain_noise
	);
	ClassDB::bind_method(D_METHOD("get_global_terrain_noise"), &TerrainSplineCompositor::get_global_terrain_noise);
	ADD_PROPERTY(
			PropertyInfo(Variant::OBJECT, "global_terrain_noise", PROPERTY_HINT_RESOURCE_TYPE, "Noise"),
			"set_global_terrain_noise", "get_global_terrain_noise"
	);

	ClassDB::bind_method(
			D_METHOD("set_global_terrain_amplitude", "amp"), &TerrainSplineCompositor::set_global_terrain_amplitude
	);
	ClassDB::bind_method(
			D_METHOD("get_global_terrain_amplitude"), &TerrainSplineCompositor::get_global_terrain_amplitude
	);
	ADD_PROPERTY(
			PropertyInfo(Variant::FLOAT, "global_terrain_amplitude"), "set_global_terrain_amplitude",
			"get_global_terrain_amplitude"
	);

	ClassDB::bind_method(
			D_METHOD("set_generation_budget_ms", "ms"), &TerrainSplineCompositor::set_generation_budget_ms
	);
	ClassDB::bind_method(D_METHOD("get_generation_budget_ms"), &TerrainSplineCompositor::get_generation_budget_ms);
	ADD_PROPERTY(
			PropertyInfo(Variant::FLOAT, "generation_budget_ms", PROPERTY_HINT_RANGE, "0.5,32,0.5"),
			"set_generation_budget_ms", "get_generation_budget_ms"
	);

	ClassDB::bind_method(D_METHOD("set_max_jobs_in_flight", "n"), &TerrainSplineCompositor::set_max_jobs_in_flight);
	ClassDB::bind_method(D_METHOD("get_max_jobs_in_flight"), &TerrainSplineCompositor::get_max_jobs_in_flight);
	ADD_PROPERTY(
			PropertyInfo(Variant::INT, "max_jobs_in_flight", PROPERTY_HINT_RANGE, "0,16,1"), "set_max_jobs_in_flight",
			"get_max_jobs_in_flight"
	);
	ClassDB::bind_method(D_METHOD("_run_chunk_job_task", "job"), &TerrainSplineCompositor::_run_chunk_job_task);

	ClassDB::bind_method(D_METHOD("queue_rebuild"), &TerrainSplineCompositor::queue_rebuild);
	ClassDB::bind_method(D_METHOD("_execute_rebuild"), &TerrainSplineCompositor::_execute_rebuild);
	ClassDB::bind_method(D_METHOD("apply_all_splines"), &TerrainSplineCompositor::apply_all_splines);
	ClassDB::bind_method(D_METHOD("_connect_spline", "node"), &TerrainSplineCompositor::_connect_spline);
	ClassDB::bind_method(D_METHOD("_disconnect_spline", "node"), &TerrainSplineCompositor::_disconnect_spline);
	ClassDB::bind_method(D_METHOD("_on_spline_changed"), &TerrainSplineCompositor::_on_spline_changed);
}

TerrainSplineCompositor::TerrainSplineCompositor() {}

TerrainSplineCompositor::~TerrainSplineCompositor() {}

// ---------------------------------------------------------------------------------------------
// Properties
// ---------------------------------------------------------------------------------------------

void TerrainSplineCompositor::set_global_terrain_noise(const Ref<Noise> &p_noise) {
	global_terrain_noise = p_noise;
	queue_rebuild();
}
Ref<Noise> TerrainSplineCompositor::get_global_terrain_noise() const { return global_terrain_noise; }

void TerrainSplineCompositor::set_global_terrain_amplitude(float p_amp) {
	global_terrain_amplitude = p_amp;
	queue_rebuild();
}
float TerrainSplineCompositor::get_global_terrain_amplitude() const { return global_terrain_amplitude; }

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

void TerrainSplineCompositor::set_max_render_radius(float p_radius) { max_render_radius = p_radius; }
float TerrainSplineCompositor::get_max_render_radius() const { return max_render_radius; }

void TerrainSplineCompositor::set_max_physics_radius(float p_radius) { max_physics_radius = p_radius; }
float TerrainSplineCompositor::get_max_physics_radius() const { return max_physics_radius; }

void TerrainSplineCompositor::set_generation_budget_ms(float p_ms) { generation_budget_ms = MAX(0.5f, p_ms); }
float TerrainSplineCompositor::get_generation_budget_ms() const { return generation_budget_ms; }

void TerrainSplineCompositor::set_global_world_offset(Vector2 p_offset) { global_world_offset = p_offset; }
Vector2 TerrainSplineCompositor::get_global_world_offset() const { return global_world_offset; }

// ---------------------------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------------------------

void TerrainSplineCompositor::_notification(int p_what) {
	switch (p_what) {
		case Node::NOTIFICATION_READY:
			_on_ready();
			break;
		case Node::NOTIFICATION_CHILD_ORDER_CHANGED:
			// Reordering children doesn't change the terrain; only an added/removed spline does.
			if (_spline_set_changed()) {
				UtilityFunctions::print("[Compositor] Spline set changed! Flagging full rebuild.");
				compositor_full_rebuild = true;
				queue_rebuild();
			}
			break;
		case Node::NOTIFICATION_PROCESS:
			_on_process();
			break;
		default:
			break;
	}
}

/// Creates the scatter container, wires spline signals and schedules the first rebuild.
void TerrainSplineCompositor::_on_ready() {
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
}

/// Per-frame work: retry a pending rebuild, origin shift, physics culling, periodic
/// eviction/discovery, and draining the generation queue under the frame budget.
void TerrainSplineCompositor::_on_process() {
	uint64_t t_proc_start = Time::get_singleton()->get_ticks_usec();

	if (_rebuild_retry_pending) {
		if (_rebuild_retry_frames < MAX_REBUILD_RETRY_FRAMES) {
			_rebuild_retry_frames++;
			queue_rebuild();
		} else if (_rebuild_retry_frames == MAX_REBUILD_RETRY_FRAMES) {
			_rebuild_retry_frames++; // Report once, then stop retrying.
			UtilityFunctions::printerr(
					"[Compositor] Terrain3D never became ready (is the 'terrain' node in the scene "
					"tree?). Giving up on automatic rebuild."
			);
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

// ---------------------------------------------------------------------------------------------
// Spline children and rebuild scheduling
// ---------------------------------------------------------------------------------------------

std::vector<ProceduralSpline3D *> TerrainSplineCompositor::_gather_splines() const {
	std::vector<ProceduralSpline3D *> splines;
	TypedArray<Node> children = get_children();
	for (int i = 0; i < children.size(); ++i) {
		ProceduralSpline3D *spline = Object::cast_to<ProceduralSpline3D>(children[i]);
		if (spline) {
			splines.push_back(spline);
		}
	}
	return splines;
}

/// True (and updates the cache) if the set of child spline instance ids changed.
bool TerrainSplineCompositor::_spline_set_changed() {
	std::vector<uint64_t> current;
	for (ProceduralSpline3D *spline : _gather_splines()) {
		current.push_back(spline->get_instance_id());
	}
	std::sort(current.begin(), current.end());
	if (current == _known_spline_ids) {
		return false;
	}
	_known_spline_ids = current;
	return true;
}

void TerrainSplineCompositor::_connect_spline(Node *p_node) {
	ProceduralSpline3D *spline = Object::cast_to<ProceduralSpline3D>(p_node);
	if (spline && !spline->is_connected("spline_changed", Callable(this, "_on_spline_changed"))) {
		spline->connect("spline_changed", Callable(this, "_on_spline_changed"));
	}
}

void TerrainSplineCompositor::_disconnect_spline(Node *p_node) {
	ProceduralSpline3D *spline = Object::cast_to<ProceduralSpline3D>(p_node);
	if (!spline) {
		return;
	}
	if (spline->is_connected("spline_changed", Callable(this, "_on_spline_changed"))) {
		spline->disconnect("spline_changed", Callable(this, "_on_spline_changed"));
	}
	compositor_full_rebuild = true;
	queue_rebuild();
}

void TerrainSplineCompositor::_on_spline_changed() {
	if (auto_apply) {
		queue_rebuild();
	}
}

void TerrainSplineCompositor::queue_rebuild() {
	if (!_rebuild_queued) {
		_rebuild_queued = true;
		call_deferred("_execute_rebuild");
	}
}

void TerrainSplineCompositor::_execute_rebuild() {
	_rebuild_queued = false;
	uint64_t t0 = Time::get_singleton()->get_ticks_usec();
	apply_all_splines();
	_bench_add_frame_time(Time::get_singleton()->get_ticks_usec() - t0);
}

// ---------------------------------------------------------------------------------------------
// Player position
// ---------------------------------------------------------------------------------------------

/// The position streaming is centred on: the GameManager's active target, else whatever Terrain3D
/// follows. Fixed at the origin in benchmark mode.
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

/// Player XZ in logical (pre-origin-shift) coordinates, which chunk keys and radii are measured in.
Vector2 TerrainSplineCompositor::_logical_player_position_2d() const {
	Vector3 target_pos = _get_player_position();
	return Vector2(target_pos.x, target_pos.z) + global_world_offset;
}

} // namespace godot
