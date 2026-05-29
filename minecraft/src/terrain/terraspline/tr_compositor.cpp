#include "terraspline.h"
#include <cmath>
#include <godot_cpp/classes/time.hpp> // NEW: For high-precision profiling
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

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

	ClassDB::bind_method(D_METHOD("set_global_world_offset", "p_offset"), &Terrain3DSplineCompositor::set_global_world_offset);
	ClassDB::bind_method(D_METHOD("get_global_world_offset"), &Terrain3DSplineCompositor::get_global_world_offset);
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "global_world_offset"), "set_global_world_offset", "get_global_world_offset");

	ClassDB::bind_method(D_METHOD("queue_rebuild"), &Terrain3DSplineCompositor::queue_rebuild);
	ClassDB::bind_method(D_METHOD("_execute_rebuild"), &Terrain3DSplineCompositor::_execute_rebuild);
	ClassDB::bind_method(D_METHOD("apply_all_splines"), &Terrain3DSplineCompositor::apply_all_splines);
	ClassDB::bind_method(D_METHOD("_connect_spline", "node"), &Terrain3DSplineCompositor::_connect_spline);
	ClassDB::bind_method(D_METHOD("_on_spline_changed"), &Terrain3DSplineCompositor::_on_spline_changed);
}

Terrain3DSplineCompositor::Terrain3DSplineCompositor() {
	max_render_radius = 2048.0f;
	last_eviction_check_time = 0;
	global_world_offset = Vector2(0.0f, 0.0f);
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

void Terrain3DSplineCompositor::set_global_world_offset(Vector2 p_offset) {
	global_world_offset = p_offset;
}
Vector2 Terrain3DSplineCompositor::get_global_world_offset() const {
	return global_world_offset;
}

void Terrain3DSplineCompositor::_notification(int p_what) {
	if (p_what == Node::NOTIFICATION_READY) {
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
		UtilityFunctions::print("\n=== [Terrain Master Grid] GLOBAL REBUILD STARTED ===");
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
		for (const KeyValue<Vector2i, Ref<TerrainHeightmap>> &E : chunk_buffers) {
			Vector2 c_center((E.key.x + 0.5f) * chunk_size, (E.key.y + 0.5f) * chunk_size);
			Vector2 logical_c_center = c_center + global_world_offset;
			if (logical_player_pos_2d.distance_to(logical_c_center) <= max_render_radius) {
				active_grid_chunks[E.key] = true;
			}
		}
	} else {
		if (!has_master_dirty)
			return;
		UtilityFunctions::print("\n=== [Terrain Master Grid] LOCAL DIRTY RECT UPDATE ===");

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

	uint64_t t_grid_calc = Time::get_singleton()->get_ticks_usec();
	UtilityFunctions::print("[Compositor] Grid chunk calculation time: ", (t_grid_calc - t_gather) / 1000.0, " ms.");

	if (active_grid_chunks.size() == 0) {
		UtilityFunctions::print("[Compositor] 0 active chunks to update. Aborting.");
		return;
	}

	UtilityFunctions::print(" -> Recalculating exactly ", (int)active_grid_chunks.size(), " isolated chunks...");

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

	uint64_t t_end = Time::get_singleton()->get_ticks_usec();
	UtilityFunctions::print("=== [Terrain Master Grid] TOTAL UPDATE TIME: ", (t_end - t_start) / 1000.0, " ms ===\n");
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

		UtilityFunctions::print("[Compositor] Origin shift triggered! Shifting world by: ", shift);

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
			HashMap<Vector2i, Ref<TerrainHeightmap>> new_chunk_buffers;
			for (const KeyValue<Vector2i, Ref<TerrainHeightmap>> &E : chunk_buffers) {
				new_chunk_buffers[E.key - chunk_shift] = E.value;
			}
			chunk_buffers = new_chunk_buffers;
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
	for (const KeyValue<Vector2i, Ref<TerrainHeightmap>> &E : chunk_buffers) {
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
			for (const KeyValue<Vector2i, Ref<TerrainHeightmap>> &E : chunk_buffers) {
				Vector2i cpos = E.key;
				Vector3 chunk_pos_3d(cpos.x * chunk_size, 0.0f, cpos.y * chunk_size);
				Vector2i chunk_rloc = target_api->call("get_region_location", chunk_pos_3d);
				if (chunk_rloc == rloc) {
					region_is_used = true;
					break;
				}
			}
			if (!region_is_used) {
				UtilityFunctions::print("[Compositor] Evicting far region from GPU VRAM: ", rloc);
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
		UtilityFunctions::print("[Compositor] Dynamic generation of ", (int)chunks_to_generate_physical.size(), " new chunks inside render radius...");
		_generate_chunks(chunks_to_generate_physical, splines, target_api);
	}
}

void Terrain3DSplineCompositor::_generate_chunks(const std::vector<Vector2i> &p_chunks, const std::vector<TerrainSpline2D *> &p_splines, Object *p_target_api) {
	Ref<Image> empty_control_map;
	empty_control_map.instantiate();

	for (Vector2i chunk_pos : p_chunks) {
		Vector2 offset(chunk_pos.x * chunk_size, chunk_pos.y * chunk_size);
		Rect2 chunk_rect(offset, Vector2(chunk_size, chunk_size));

		bool has_splines = false;
		for (TerrainSpline2D *spline : p_splines) {
			if (spline->get_padded_aabb().intersects(chunk_rect)) {
				has_splines = true;
				break;
			}
		}

		Ref<TerrainHeightmap> buffer;
		if (chunk_buffers.has(chunk_pos)) {
			buffer = chunk_buffers[chunk_pos];
		} else {
			if (!has_splines)
				continue;
			buffer.instantiate();
			buffer->initialize(chunk_size, chunk_size, default_elevation);
			chunk_buffers[chunk_pos] = buffer;
		}

		buffer->clear(default_elevation);

		uint64_t t_math_start = Time::get_singleton()->get_ticks_usec();
		if (has_splines) {
			for (TerrainSpline2D *spline : p_splines) {
				if (spline->get_padded_aabb().intersects(chunk_rect)) {
					spline->deform_heightmap(buffer, offset);
				}
			}
		}
		uint64_t t_math_end = Time::get_singleton()->get_ticks_usec();

		Ref<Image> height_image = buffer->get_image();
		Vector3 stamp_position(offset.x, 0.0f, offset.y);

		Array images;
		images.push_back(height_image);
		images.push_back(empty_control_map);
		images.push_back(empty_control_map);

		uint64_t t_t3d_start = Time::get_singleton()->get_ticks_usec();
		p_target_api->call("import_images", images, stamp_position, 0.0f, 1.0f);
		uint64_t t_t3d_end = Time::get_singleton()->get_ticks_usec();

		UtilityFunctions::print("  -> Dynamic Chunk [", chunk_pos.x, ", ", chunk_pos.y, "] | Math: ", (t_math_end - t_math_start) / 1000.0, " ms | Terrain3D API: ", (t_t3d_end - t_t3d_start) / 1000.0, " ms");
	}
}

} //namespace godot
