#include "terraspline.h"
#include <cmath>
#include <godot_cpp/classes/time.hpp> // NEW: For high-precision profiling
#include <godot_cpp/classes/worker_thread_pool.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/object.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

// =========================================================
// TerrainChunk Implementation
// =========================================================

void TerrainChunk::_bind_methods() {}

TerrainChunk::TerrainChunk() {
	current_state = STATE_UNLOADED;
	physics_body_rid = RID();
}

TerrainChunk::~TerrainChunk() {
	// Clean up visuals safely using ObjectDB to prevent use-after-free
	for (uint64_t id : visual_nodes) {
		Object *obj = ObjectDB::get_instance(id);
		if (obj) {
			MultiMeshInstance3D *node = Object::cast_to<MultiMeshInstance3D>(obj);
			if (node) {
				if (node->is_inside_tree()) {
					node->queue_free();
				} else {
					memdelete(node);
				}
			}
		}
	}
	visual_nodes.clear();

	// Clean up physics
	if (physics_body_rid.is_valid()) {
		PhysicsServer3D::get_singleton()->free_rid(physics_body_rid);
		physics_body_rid = RID();
	}
}

// =========================================================
// ScatterJob Implementation
// =========================================================

void ScatterJob::_bind_methods() {}
ScatterJob::ScatterJob() {}
ScatterJob::~ScatterJob() {}

// =========================================================
// TerrainSplineScatter Implementation
// =========================================================

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
}

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

void Terrain3DSplineCompositor::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_terrain", "terrain"), &Terrain3DSplineCompositor::set_terrain);
	ClassDB::bind_method(D_METHOD("get_terrain"), &Terrain3DSplineCompositor::get_terrain);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "terrain", PROPERTY_HINT_NODE_TYPE, "Node3D"), "set_terrain", "get_terrain");

	ClassDB::bind_method(D_METHOD("set_chunk_size", "p_size"), &Terrain3DSplineCompositor::set_chunk_size);
	ClassDB::bind_method(D_METHOD("get_chunk_size"), &Terrain3DSplineCompositor::get_chunk_size);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "chunk_size"), "set_chunk_size", "get_chunk_size");

	ClassDB::bind_method(D_METHOD("set_default_elevation", "elevation"), &Terrain3DSplineCompositor::set_default_elevation);
	ClassDB::bind_method(D_METHOD("get_default_elevation"), &Terrain3DSplineCompositor::get_default_elevation);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "default_elevation"), "set_default_elevation", "get_default_elevation");

	ClassDB::bind_method(D_METHOD("set_auto_apply", "auto_apply"), &Terrain3DSplineCompositor::set_auto_apply);
	ClassDB::bind_method(D_METHOD("get_auto_apply"), &Terrain3DSplineCompositor::get_auto_apply);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "auto_apply"), "set_auto_apply", "get_auto_apply");

	ClassDB::bind_method(D_METHOD("set_apply_now", "apply_now"), &Terrain3DSplineCompositor::set_apply_now);
	ClassDB::bind_method(D_METHOD("get_apply_now"), &Terrain3DSplineCompositor::get_apply_now);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "apply_now"), "set_apply_now", "get_apply_now");

	ClassDB::bind_method(D_METHOD("set_max_render_radius", "p_radius"), &Terrain3DSplineCompositor::set_max_render_radius);
	ClassDB::bind_method(D_METHOD("get_max_render_radius"), &Terrain3DSplineCompositor::get_max_render_radius);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "max_render_radius"), "set_max_render_radius", "get_max_render_radius");

	ClassDB::bind_method(D_METHOD("set_max_physics_radius", "p_radius"), &Terrain3DSplineCompositor::set_max_physics_radius);
	ClassDB::bind_method(D_METHOD("get_max_physics_radius"), &Terrain3DSplineCompositor::get_max_physics_radius);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "max_physics_radius"), "set_max_physics_radius", "get_max_physics_radius");

	ClassDB::bind_method(D_METHOD("set_global_world_offset", "p_offset"), &Terrain3DSplineCompositor::set_global_world_offset);
	ClassDB::bind_method(D_METHOD("get_global_world_offset"), &Terrain3DSplineCompositor::get_global_world_offset);
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "global_world_offset"), "set_global_world_offset", "get_global_world_offset");

	ClassDB::bind_method(D_METHOD("queue_rebuild"), &Terrain3DSplineCompositor::queue_rebuild);
	ClassDB::bind_method(D_METHOD("_execute_rebuild"), &Terrain3DSplineCompositor::_execute_rebuild);
	ClassDB::bind_method(D_METHOD("apply_all_splines"), &Terrain3DSplineCompositor::apply_all_splines);
	ClassDB::bind_method(D_METHOD("_connect_spline", "node"), &Terrain3DSplineCompositor::_connect_spline);
	ClassDB::bind_method(D_METHOD("_on_spline_changed"), &Terrain3DSplineCompositor::_on_spline_changed);
	ClassDB::bind_method(D_METHOD("_run_scatter_job", "job"), &Terrain3DSplineCompositor::_run_scatter_job);
}

Terrain3DSplineCompositor::Terrain3DSplineCompositor() {
	max_render_radius = 2048.0f;
	max_physics_radius = 150.0f;
	last_eviction_check_time = 0;
	global_world_offset = Vector2(0.0f, 0.0f);
	scatter_container = nullptr;
}
Terrain3DSplineCompositor::~Terrain3DSplineCompositor() {}
void Terrain3DSplineCompositor::set_terrain(Node *p_terrain) { terrain = p_terrain; }
Node *Terrain3DSplineCompositor::get_terrain() const { return terrain; }
void Terrain3DSplineCompositor::set_chunk_size(int p_size) { chunk_size = MAX(64, p_size); }
int Terrain3DSplineCompositor::get_chunk_size() const { return chunk_size; }
void Terrain3DSplineCompositor::set_default_elevation(float p_elev) { default_elevation = p_elev; }
float Terrain3DSplineCompositor::get_default_elevation() const { return default_elevation; }
void Terrain3DSplineCompositor::set_auto_apply(bool p_auto) { auto_apply = p_auto; }
bool Terrain3DSplineCompositor::get_auto_apply() const { return auto_apply; }
void Terrain3DSplineCompositor::set_apply_now(bool p_apply) {
	if (p_apply) {
		compositor_full_rebuild = true;
		apply_all_splines();
	}
}
bool Terrain3DSplineCompositor::get_apply_now() const { return false; }

void Terrain3DSplineCompositor::set_max_render_radius(float p_radius) {
	max_render_radius = p_radius;
}
float Terrain3DSplineCompositor::get_max_render_radius() const {
	return max_render_radius;
}

void Terrain3DSplineCompositor::set_max_physics_radius(float p_radius) {
	max_physics_radius = p_radius;
}
float Terrain3DSplineCompositor::get_max_physics_radius() const {
	return max_physics_radius;
}

void Terrain3DSplineCompositor::set_global_world_offset(Vector2 p_offset) {
	global_world_offset = p_offset;
}
Vector2 Terrain3DSplineCompositor::get_global_world_offset() const {
	return global_world_offset;
}

void Terrain3DSplineCompositor::_notification(int p_what) {
	if (p_what == Node::NOTIFICATION_READY) {
		scatter_container = memnew(Node3D);
		scatter_container->set_name("InternalScatterContainer");
		add_child(scatter_container, false, Node::INTERNAL_MODE_BACK);

		TypedArray<Node> children = get_children();
		for (int i = 0; i < children.size(); ++i) {
			_connect_spline(Object::cast_to<Node>(children[i]));
		}
		connect("child_entered_tree", Callable(this, "_connect_spline"));
		set_process(true);
		call_deferred("apply_all_splines");
	} else if (p_what == Node::NOTIFICATION_CHILD_ORDER_CHANGED) {
		UtilityFunctions::print("[Compositor] Child order changed! Flagging full rebuild.");
		compositor_full_rebuild = true;
		queue_rebuild();
	} else if (p_what == Node::NOTIFICATION_PROCESS) {
		_check_origin_shift();
		_check_chunk_physics_culling();
		uint64_t msec = Time::get_singleton()->get_ticks_msec();
		if (msec - last_eviction_check_time >= 3000) {
			last_eviction_check_time = msec;
			_check_and_evict_far_chunks();
		}
	}
}

void Terrain3DSplineCompositor::_connect_spline(Node *p_node) {
	TerrainSpline2D *spline = Object::cast_to<TerrainSpline2D>(p_node);
	if (spline) {
		if (!spline->is_connected("spline_changed", Callable(this, "_on_spline_changed"))) {
			spline->connect("spline_changed", Callable(this, "_on_spline_changed"));
		}
	}
}

void Terrain3DSplineCompositor::_on_spline_changed() {
	if (auto_apply)
		queue_rebuild();
}

void Terrain3DSplineCompositor::queue_rebuild() {
	if (!_rebuild_queued) {
		_rebuild_queued = true;
		call_deferred("_execute_rebuild");
	}
}

void Terrain3DSplineCompositor::_execute_rebuild() {
	_rebuild_queued = false;
	apply_all_splines();
}

void Terrain3DSplineCompositor::apply_all_splines() {
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

	TypedArray<Node> children = get_children();
	std::vector<TerrainSpline2D *> splines;
	for (int i = 0; i < children.size(); ++i) {
		TerrainSpline2D *spline = Object::cast_to<TerrainSpline2D>(children[i]);
		if (spline)
			splines.push_back(spline);
	}

	uint64_t t_gather = Time::get_singleton()->get_ticks_usec();

	Rect2 master_dirty_rect;
	bool has_master_dirty = false;

	for (TerrainSpline2D *spline : splines) {
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

	Vector3 target_pos = terrain->call("get_collision_target_position");
	Vector2 player_pos_2d(target_pos.x, target_pos.z);
	Vector2 logical_player_pos_2d = player_pos_2d + global_world_offset;

	if (compositor_full_rebuild) {
#if DEBUG
		UtilityFunctions::print("\n=== [Terrain Master Grid] GLOBAL REBUILD STARTED ===");
		UtilityFunctions::print("[Compositor] Chunks in buffer: ", (int)chunk_buffers.size(),
								" | Player logical pos: ", logical_player_pos_2d,
								" | Render radius: ", max_render_radius, "m");
#endif

		for (TerrainSpline2D *spline : splines) {
			Rect2 aabb = spline->get_padded_aabb();
			if (!aabb.has_area())
				continue;
			int min_cx = (int)Math::floor(aabb.position.x / chunk_size);
			int max_cx = (int)Math::floor((aabb.position.x + aabb.size.x) / chunk_size);
			int min_cz = (int)Math::floor(aabb.position.y / chunk_size);
			int max_cz = (int)Math::floor((aabb.position.y + aabb.size.y) / chunk_size);

			for (int cx = min_cx; cx <= max_cx; ++cx) {
				for (int cz = min_cz; cz <= max_cz; ++cz) {
					Vector2 c_center((cx + 0.5f) * chunk_size, (cz + 0.5f) * chunk_size);
					Vector2 logical_c_center = c_center + global_world_offset;
					if (logical_player_pos_2d.distance_to(logical_c_center) <= max_render_radius) {
						active_grid_chunks[Vector2i(cx, cz)] = true;
					}
				}
			}
		}
		for (const KeyValue<Vector2i, Ref<TerrainChunk>> &E : chunk_buffers) {
			Vector2 c_center((E.key.x + 0.5f) * chunk_size, (E.key.y + 0.5f) * chunk_size);
			Vector2 logical_c_center = c_center + global_world_offset;
			if (logical_player_pos_2d.distance_to(logical_c_center) <= max_render_radius) {
				active_grid_chunks[E.key] = true;
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
				Vector2 c_center((cx + 0.5f) * chunk_size, (cz + 0.5f) * chunk_size);
				Vector2 logical_c_center = c_center + global_world_offset;
				if (logical_player_pos_2d.distance_to(logical_c_center) <= max_render_radius) {
					active_grid_chunks[Vector2i(cx, cz)] = true;
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
	std::vector<Vector2i> chunks_to_remove;

	for (const KeyValue<Vector2i, bool> &E : active_grid_chunks) {
		Vector2i chunk_pos = E.key;
		Vector2 offset(chunk_pos.x * chunk_size, chunk_pos.y * chunk_size);
		Rect2 chunk_rect(offset, Vector2(chunk_size, chunk_size));

		bool has_splines = false;
		for (TerrainSpline2D *spline : splines) {
			if (spline->get_padded_aabb().intersects(chunk_rect)) {
				has_splines = true;
				break;
			}
		}

		if (has_splines) {
			chunks_to_generate.push_back(chunk_pos);
		} else {
			chunks_to_remove.push_back(chunk_pos);
		}
	}

	if (!chunks_to_generate.empty()) {
		_generate_chunks(chunks_to_generate, splines, target_api);
	}

	for (Vector2i cpos : chunks_to_remove) {
		chunk_buffers.erase(cpos);
	}

	terrain->set("show_checkered", false);

	// Update physics culling to immediately activate physics inside the radius
	_check_chunk_physics_culling();

#if DEBUG
	uint64_t t_end = Time::get_singleton()->get_ticks_usec();
	UtilityFunctions::print("=== [Terrain Master Grid] TOTAL UPDATE TIME: ", (t_end - t_start) / 1000.0, " ms ===\n");
#endif
}

void Terrain3DSplineCompositor::_check_origin_shift() {
	if (!terrain) {
		return;
	}

	Vector3 target_pos = terrain->call("get_collision_target_position");

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

		// 3. Loop through every TerrainSpline2D child and subtract shift from physical position
		TypedArray<Node> children = get_children();
		for (int i = 0; i < children.size(); ++i) {
			TerrainSpline2D *spline = Object::cast_to<TerrainSpline2D>(children[i]);
			if (spline) {
				spline->set_global_position(spline->get_global_position() - shift);
			}
		}

		// 4. Subtract shift from Terrain3D node's physical position
		Node3D *terrain_3d = Object::cast_to<Node3D>(terrain);
		if (terrain_3d) {
			terrain_3d->set_global_position(terrain_3d->get_global_position() - shift);
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

void Terrain3DSplineCompositor::_check_and_evict_far_chunks() {
	if (!terrain) {
		return;
	}

	Variant api_var = terrain->get("data");
	if (api_var.get_type() == Variant::NIL || (api_var.get_type() == Variant::OBJECT && (Object *)api_var == nullptr)) {
		api_var = terrain->get("storage");
	}
	Object *target_api = (api_var.get_type() == Variant::OBJECT) ? (Object *)api_var : nullptr;
	if (!target_api) {
		return;
	}

	// 1. Get player position
	Vector3 target_pos = terrain->call("get_collision_target_position");
	Vector2 player_pos_2d(target_pos.x, target_pos.z);
	Vector2 logical_player_pos_2d = player_pos_2d + global_world_offset;

	// 2. Erase far-away chunks from compositor's chunk_buffers
	std::vector<Vector2i> chunks_to_erase;
	for (const KeyValue<Vector2i, Ref<TerrainChunk>> &E : chunk_buffers) {
		Vector2i cpos = E.key;
		Vector2 c_center((cpos.x + 0.5f) * chunk_size, (cpos.y + 0.5f) * chunk_size);
		Vector2 logical_c_center = c_center + global_world_offset;
		if (logical_player_pos_2d.distance_to(logical_c_center) > max_render_radius) {
			chunks_to_erase.push_back(cpos);
		}
	}
	for (Vector2i cpos : chunks_to_erase) {
		chunk_buffers.erase(cpos);
	}

	// 3. Find and evict far-away regions from Terrain3D
	TypedArray<Vector2i> region_locations = target_api->call("get_region_locations");
	int region_size = terrain->call("get_region_size");
	float vertex_spacing = terrain->call("get_vertex_spacing");
	float region_world_size = region_size * vertex_spacing;

	for (int i = 0; i < region_locations.size(); ++i) {
		Vector2i rloc = region_locations[i];
		Vector2 r_center((rloc.x + 0.5f) * region_world_size, (rloc.y + 0.5f) * region_world_size);
		Vector2 logical_r_center = r_center + global_world_offset;

		// If region center is beyond player's max render radius (with extra margin for boundary safety)
		if (logical_player_pos_2d.distance_to(logical_r_center) > max_render_radius + region_world_size * 0.5f) {
			// Ensure no active compositor chunk is inside this region
			bool region_is_used = false;
			for (const KeyValue<Vector2i, Ref<TerrainChunk>> &E : chunk_buffers) {
				Vector2i cpos = E.key;
				Vector3 chunk_pos_3d(cpos.x * chunk_size, 0.0f, cpos.y * chunk_size);
				Vector2i chunk_rloc = target_api->call("get_region_location", chunk_pos_3d);
				if (chunk_rloc == rloc) {
					region_is_used = true;
					break;
				}
			}
			if (!region_is_used) {
#if DEBUG
				UtilityFunctions::print("[Compositor] Evicting far region from GPU VRAM: ", rloc);
#endif
				target_api->call("remove_regionl", rloc, true);
			}
		}
	}

	// 4. Generate new chunks that have entered the render radius
	TypedArray<Node> children = get_children();
	std::vector<TerrainSpline2D *> splines;
	for (int i = 0; i < children.size(); ++i) {
		TerrainSpline2D *spline = Object::cast_to<TerrainSpline2D>(children[i]);
		if (spline) {
			splines.push_back(spline);
		}
	}

	int min_cx = (int)Math::floor((logical_player_pos_2d.x - max_render_radius) / chunk_size);
	int max_cx = (int)Math::floor((logical_player_pos_2d.x + max_render_radius) / chunk_size);
	int min_cz = (int)Math::floor((logical_player_pos_2d.y - max_render_radius) / chunk_size);
	int max_cz = (int)Math::floor((logical_player_pos_2d.y + max_render_radius) / chunk_size);

	std::vector<Vector2i> chunks_to_generate_physical;
	for (int cx = min_cx; cx <= max_cx; ++cx) {
		for (int cz = min_cz; cz <= max_cz; ++cz) {
			Vector2i logical_cpos(cx, cz);
			Vector2i physical_cpos = logical_cpos - Vector2i(global_world_offset.x / chunk_size, global_world_offset.y / chunk_size);

			if (chunk_buffers.has(physical_cpos)) {
				continue; // Already active/generated
			}

			Vector2 logical_c_center((cx + 0.5f) * chunk_size, (cz + 0.5f) * chunk_size);
			if (logical_player_pos_2d.distance_to(logical_c_center) > max_render_radius) {
				continue; // Outside range
			}

			Vector2 offset(physical_cpos.x * chunk_size, physical_cpos.y * chunk_size);
			Rect2 chunk_rect(offset, Vector2(chunk_size, chunk_size));

			bool has_splines = false;
			for (TerrainSpline2D *spline : splines) {
				if (spline->get_padded_aabb().intersects(chunk_rect)) {
					has_splines = true;
					break;
				}
			}

			if (has_splines) {
				chunks_to_generate_physical.push_back(physical_cpos);
			}
		}
	}

	if (!chunks_to_generate_physical.empty()) {
#if DEBUG
		UtilityFunctions::print("[Compositor] Dynamic generation of ", (int)chunks_to_generate_physical.size(), " new chunks inside render radius...");
#endif
		_generate_chunks(chunks_to_generate_physical, splines, target_api);
	}
}

void Terrain3DSplineCompositor::_generate_chunks(const std::vector<Vector2i> &p_chunks, const std::vector<TerrainSpline2D *> &p_splines, Object *p_target_api) {
	Ref<Image> empty_control_map;
	empty_control_map.instantiate();

	for (Vector2i chunk_pos : p_chunks) {
#if DEBUG
		UtilityFunctions::print("  --- Chunk [", chunk_pos.x, ", ", chunk_pos.y, "] Generation ---");
#endif
		Vector2 offset(chunk_pos.x * chunk_size, chunk_pos.y * chunk_size);
		Rect2 chunk_rect(offset, Vector2(chunk_size, chunk_size));

		bool has_splines = false;
		for (TerrainSpline2D *spline : p_splines) {
			if (spline->get_padded_aabb().intersects(chunk_rect)) {
				has_splines = true;
				break;
			}
		}

		Ref<TerrainChunk> chunk;
		if (chunk_buffers.has(chunk_pos)) {
			chunk = chunk_buffers[chunk_pos];
		} else {
			if (!has_splines)
				continue;
			chunk.instantiate();
			Ref<TerrainHeightmap> buffer;
			buffer.instantiate();
			buffer->initialize(chunk_size, chunk_size, default_elevation);
			chunk->set_heightmap(buffer);
			chunk->set_chunk_coords(chunk_pos);
			chunk_buffers[chunk_pos] = chunk;
		}

		Ref<TerrainHeightmap> buffer = chunk->get_heightmap();
		buffer->clear(default_elevation);

		// Clean up existing visual/physics for this chunk in case of regeneration
		{
			std::vector<uint64_t> &nodes = chunk->get_visual_nodes();
			for (uint64_t id : nodes) {
				Object *obj = ObjectDB::get_instance(id);
				if (obj) {
					MultiMeshInstance3D *node = Object::cast_to<MultiMeshInstance3D>(obj);
					if (node) {
						if (node->is_inside_tree()) {
							node->queue_free();
						} else {
							memdelete(node);
						}
					}
				}
			}
			nodes.clear();

			RID body_rid = chunk->get_physics_body_rid();
			if (body_rid.is_valid()) {
				PhysicsServer3D::get_singleton()->free_rid(body_rid);
				chunk->set_physics_body_rid(RID());
			}
			chunk->get_physics_caches().clear();
			chunk->set_state(TerrainChunk::STATE_GENERATING);
		}

		uint64_t t_math_start = Time::get_singleton()->get_ticks_usec();
		if (has_splines) {
			for (TerrainSpline2D *spline : p_splines) {
				if (spline->get_padded_aabb().intersects(chunk_rect)) {
					spline->deform_heightmap(buffer, offset);
				}
			}
		}
		uint64_t t_math_end = Time::get_singleton()->get_ticks_usec();

		// Run procedural scattering for this chunk!
		if (has_splines) {
			std::vector<Ref<ScatterJob>> jobs;
			std::vector<int> task_ids;
			WorkerThreadPool *wtp = WorkerThreadPool::get_singleton();

			for (TerrainSpline2D *spline : p_splines) {
				if (!spline->get_padded_aabb().intersects(chunk_rect)) {
					continue;
				}

				TypedArray<Node> spline_children = spline->get_children();
				for (int i = 0; i < spline_children.size(); ++i) {
					TerrainSplineScatter *scatterer = Object::cast_to<TerrainSplineScatter>(spline_children[i]);
					if (scatterer) {
						Ref<ScatterJob> job;
						job.instantiate();
						job->chunk = chunk;
						job->spline = spline;
						job->scatterer = scatterer;
						job->offset = offset;

						jobs.push_back(job);

						if (wtp) {
							Callable callable = Callable(this, "_run_scatter_job").bind(job);
							int task_id = wtp->add_task(callable, "TerrainScatterJob");
							task_ids.push_back(task_id);
						} else {
							_run_scatter_job(job);
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
				if (scatter_container) {
					scatter_container->add_child(mmi);
				} else {
					add_child(mmi);
				}
				chunk->get_visual_nodes().push_back(mmi->get_instance_id());

				// 2. Cache physics transforms if collision shape is set
				if (scatterer->get_collision_shape().is_valid()) {
					TerrainChunk::PhysicsCache pc;
					pc.shape_rid = scatterer->get_collision_shape()->get_rid();
					pc.transforms = job->transforms;
					chunk->get_physics_caches().push_back(pc);
				}
			}
		}

		// Set chunk state to visual only
		chunk->set_state(TerrainChunk::STATE_VISUAL_ONLY);

		Ref<Image> height_image = buffer->get_image();
		Vector3 stamp_position(offset.x, 0.0f, offset.y);

		Array images;
		images.push_back(height_image);
		images.push_back(empty_control_map);
		images.push_back(empty_control_map);

		uint64_t t_t3d_start = Time::get_singleton()->get_ticks_usec();
		p_target_api->call("import_images", images, stamp_position, 0.0f, 1.0f);
		uint64_t t_t3d_end = Time::get_singleton()->get_ticks_usec();

#if DEBUG
		UtilityFunctions::print("  -> Dynamic Chunk [", chunk_pos.x, ", ", chunk_pos.y, "] | Math: ", (t_math_end - t_math_start) / 1000.0, " ms | Terrain3D API: ", (t_t3d_end - t_t3d_start) / 1000.0, " ms");
#endif
	}
}

static inline float local_lerp(float a, float b, float t) {
	return a + t * (b - a);
}

void Terrain3DSplineCompositor::_run_scatter_job(const Ref<ScatterJob> &p_job) {
	if (p_job.is_null() || p_job->chunk.is_null() || p_job->spline == nullptr || p_job->scatterer == nullptr) {
		return;
	}

#if DEBUG
	uint64_t t_start = Time::get_singleton()->get_ticks_usec();
#endif
	TerrainSplineScatter *scatterer = p_job->scatterer;
	TerrainSpline2D *spline = p_job->spline;
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

	int num_cells = (int)Math::floor(chunk_size / spacing);
	if (num_cells <= 0) {
		return;
	}

	p_job->debug_total_cells = num_cells * num_cells;
	p_job->debug_density_skipped = 0;
	p_job->debug_noise_skipped = 0;
	p_job->debug_spline_skipped = 0;
	p_job->debug_slope_skipped = 0;

	std::vector<Transform3D> transforms;
	uint64_t base_seed = 1234567ULL;

	for (int cz = 0; cz < num_cells; ++cz) {
		for (int cx = 0; cx < num_cells; ++cx) {
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
			TerrainSpline2D::SplineEval eval = spline->_evaluate_spline_point(test_point);
			if (eval.distance < scatterer->get_min_spline_dist() || eval.distance > scatterer->get_max_spline_dist()) {
				p_job->debug_spline_skipped++;
				continue;
			}

			// Local heights check to determine normal and slope
			float local_x = random_x - offset.x;
			float local_z = random_z - offset.y;

			int lx = (int)local_x;
			int lz = (int)local_z;

			// clamp coordinates for safety
			int lx_min = Math::clamp(lx, 0, chunk_size - 1);
			int lx_max = Math::clamp(lx + 1, 0, chunk_size - 1);
			int lz_min = Math::clamp(lz, 0, chunk_size - 1);
			int lz_max = Math::clamp(lz + 1, 0, chunk_size - 1);

			float fx = local_x - lx;
			float fz = local_z - lz;

			// Get the 4 surrounding heights
			float h00 = heightmap_data[lz_min * chunk_size + lx_min];
			float h10 = heightmap_data[lz_min * chunk_size + lx_max];
			float h01 = heightmap_data[lz_max * chunk_size + lx_min];
			float h11 = heightmap_data[lz_max * chunk_size + lx_max];

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

	p_job->transforms = transforms;
#if DEBUG
	p_job->debug_time_spent_usec = Time::get_singleton()->get_ticks_usec() - t_start;
#endif
}

void Terrain3DSplineCompositor::_update_chunk_physics(const Ref<TerrainChunk> &p_chunk) {
	if (!terrain) {
		return;
	}
	Node3D *terrain_3d = Object::cast_to<Node3D>(terrain);
	if (!terrain_3d) {
		return;
	}
	RID space_rid = terrain_3d->get_world_3d()->get_space();

	// Wake up physics
	if (p_chunk->get_state() == TerrainChunk::STATE_VISUAL_ONLY) {
		if (p_chunk->get_physics_caches().empty()) {
			p_chunk->set_state(TerrainChunk::STATE_VISUAL_AND_PHYSICS);
			return;
		}

		RID body_rid = PhysicsServer3D::get_singleton()->body_create();
		PhysicsServer3D::get_singleton()->body_set_mode(body_rid, PhysicsServer3D::BODY_MODE_STATIC);
		PhysicsServer3D::get_singleton()->body_set_space(body_rid, space_rid);

		for (const TerrainChunk::PhysicsCache &pc : p_chunk->get_physics_caches()) {
			for (const Transform3D &t : pc.transforms) {
				PhysicsServer3D::get_singleton()->body_add_shape(body_rid, pc.shape_rid, t);
			}
		}

		p_chunk->set_physics_body_rid(body_rid);
		p_chunk->set_state(TerrainChunk::STATE_VISUAL_AND_PHYSICS);
	}
	// Sleep physics
	else if (p_chunk->get_state() == TerrainChunk::STATE_VISUAL_AND_PHYSICS) {
		RID body_rid = p_chunk->get_physics_body_rid();
		if (body_rid.is_valid()) {
			PhysicsServer3D::get_singleton()->free_rid(body_rid);
			p_chunk->set_physics_body_rid(RID());
		}
		p_chunk->set_state(TerrainChunk::STATE_VISUAL_ONLY);
	}
}

void Terrain3DSplineCompositor::_check_chunk_physics_culling() {
	if (!terrain) {
		return;
	}

	Vector3 target_pos = terrain->call("get_collision_target_position");
	Vector2 player_pos_2d(target_pos.x, target_pos.z);
	Vector2 logical_player_pos_2d = player_pos_2d + global_world_offset;

	int active_physics_count = 0;
	int visual_only_count = 0;

	for (const KeyValue<Vector2i, Ref<TerrainChunk>> &E : chunk_buffers) {
		Ref<TerrainChunk> chunk = E.value;
		if (chunk.is_null()) {
			continue;
		}

		Vector2i cpos = chunk->get_chunk_coords();
		Vector2 c_center((cpos.x + 0.5f) * chunk_size, (cpos.y + 0.5f) * chunk_size);
		Vector2 logical_c_center = c_center + global_world_offset;

		float dist = logical_player_pos_2d.distance_to(logical_c_center);

		// If inside physics radius and not yet wake, wake it up!
		if (dist <= max_physics_radius) {
			if (chunk->get_state() == TerrainChunk::STATE_VISUAL_ONLY) {
				_update_chunk_physics(chunk);
			}
		} else {
			// Outside physics radius and awake, sleep it!
			if (chunk->get_state() == TerrainChunk::STATE_VISUAL_AND_PHYSICS) {
				_update_chunk_physics(chunk);
			}
		}

		if (chunk->get_state() == TerrainChunk::STATE_VISUAL_AND_PHYSICS) {
			active_physics_count++;
		} else if (chunk->get_state() == TerrainChunk::STATE_VISUAL_ONLY) {
			visual_only_count++;
		}
	}
}

} //namespace godot
