#include "terraspline.h"
#include <godot_cpp/classes/time.hpp> // NEW: For high-precision profiling
#include <godot_cpp/classes/worker_thread_pool.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/object.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

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

	ClassDB::bind_method(D_METHOD("queue_rebuild"), &TerrainSplineCompositor::queue_rebuild);
	ClassDB::bind_method(D_METHOD("_execute_rebuild"), &TerrainSplineCompositor::_execute_rebuild);
	ClassDB::bind_method(D_METHOD("apply_all_splines"), &TerrainSplineCompositor::apply_all_splines);
	ClassDB::bind_method(D_METHOD("_connect_spline", "node"), &TerrainSplineCompositor::_connect_spline);
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
}

/**
 * @brief Default Destructor.
 */
TerrainSplineCompositor::~TerrainSplineCompositor() {}
void TerrainSplineCompositor::set_terrain(Node *p_terrain) { terrain = p_terrain; }
Node *TerrainSplineCompositor::get_terrain() const { return terrain; }
void TerrainSplineCompositor::set_chunk_size(int p_size) { chunk_size = MAX(64, p_size); }
int TerrainSplineCompositor::get_chunk_size() const { return chunk_size; }
void TerrainSplineCompositor::set_default_elevation(float p_elev) { default_elevation = p_elev; }
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

void TerrainSplineCompositor::set_global_world_offset(Vector2 p_offset) {
	global_world_offset = p_offset;
}
Vector2 TerrainSplineCompositor::get_global_world_offset() const {
	return global_world_offset;
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
	apply_all_splines();
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

		for (ProceduralSpline3D *spline : splines) {
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
		for (ProceduralSpline3D *spline : splines) {
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

/**
 * @brief Floating Origin Precision System.
 * Triggers when the player is more than 4096 meters from the center. Shifts camera targets, player
 * visual nodes, splines, and chunk buffer coordinates by the threshold to maintain single-float precision.
 */
void TerrainSplineCompositor::_check_origin_shift() {
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
 * @brief Generates, deforms, and scatters assets for specific chunk coordinates.
 * Clears old instances, invokes all child spline deformers, scatters models using the thread pool,
 * and calls Terrain3D's image importer to update GPU heightmaps.
 */
void TerrainSplineCompositor::_generate_chunks(const std::vector<Vector2i> &p_chunks, const std::vector<ProceduralSpline3D *> &p_splines, Object *p_target_api) {
	Ref<Image> empty_control_map;
	empty_control_map.instantiate();

	for (Vector2i chunk_pos : p_chunks) {
#if DEBUG
		UtilityFunctions::print("  --- Chunk [", chunk_pos.x, ", ", chunk_pos.y, "] Generation ---");
#endif
		Vector2 offset(chunk_pos.x * chunk_size, chunk_pos.y * chunk_size);
		Rect2 chunk_rect(offset, Vector2(chunk_size, chunk_size));

		bool has_splines = false;
		for (ProceduralSpline3D *spline : p_splines) {
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
			for (ProceduralSpline3D *spline : p_splines) {
				if (spline->get_padded_aabb().intersects(chunk_rect)) {
					TypedArray<Node> children = spline->get_children();
					for (int i = 0; i < children.size(); ++i) {
						TerrainSplineDeformer *deformer = Object::cast_to<TerrainSplineDeformer>(children[i]);
						if (deformer) {
							deformer->deform_heightmap(buffer, spline, offset);
						}
					}
				}
			}
		}
		uint64_t t_math_end = Time::get_singleton()->get_ticks_usec();

		// Run procedural scattering for this chunk!
		if (has_splines) {
			TerrainSplineScatter::scatter_chunk(chunk, p_splines, chunk_rect, offset, chunk_size, scatter_container, this);
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

} //namespace godot
