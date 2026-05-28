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

	ClassDB::bind_method(D_METHOD("queue_rebuild"), &Terrain3DSplineCompositor::queue_rebuild);
	ClassDB::bind_method(D_METHOD("_execute_rebuild"), &Terrain3DSplineCompositor::_execute_rebuild);
	ClassDB::bind_method(D_METHOD("apply_all_splines"), &Terrain3DSplineCompositor::apply_all_splines);
	ClassDB::bind_method(D_METHOD("_connect_spline", "node"), &Terrain3DSplineCompositor::_connect_spline);
	ClassDB::bind_method(D_METHOD("_on_spline_changed"), &Terrain3DSplineCompositor::_on_spline_changed);
}

Terrain3DSplineCompositor::Terrain3DSplineCompositor() {}
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

void Terrain3DSplineCompositor::_notification(int p_what) {
	if (p_what == Node::NOTIFICATION_READY) {
		TypedArray<Node> children = get_children();
		for (int i = 0; i < children.size(); ++i) {
			_connect_spline(Object::cast_to<Node>(children[i]));
		}
		connect("child_entered_tree", Callable(this, "_connect_spline"));
		call_deferred("apply_all_splines");
	} else if (p_what == Node::NOTIFICATION_CHILD_ORDER_CHANGED) {
		UtilityFunctions::print("[Compositor] Child order changed! Flagging full rebuild.");
		compositor_full_rebuild = true;
		queue_rebuild();
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
					active_grid_chunks[Vector2i(cx, cz)] = true;
				}
			}
		}
		for (const KeyValue<Vector2i, Ref<TerrainHeightmap>> &E : chunk_buffers) {
			active_grid_chunks[E.key] = true;
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
				active_grid_chunks[Vector2i(cx, cz)] = true;
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

	std::vector<Vector2i> chunks_to_remove;
	Ref<Image> empty_control_map;
	empty_control_map.instantiate();

	uint64_t total_math_time = 0;
	uint64_t total_terrain3d_time = 0;

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
			for (TerrainSpline2D *spline : splines) {
				if (spline->get_padded_aabb().intersects(chunk_rect)) {
					spline->deform_heightmap(buffer, offset);
				}
			}
		} else {
			chunks_to_remove.push_back(chunk_pos);
		}
		uint64_t t_math_end = Time::get_singleton()->get_ticks_usec();
		total_math_time += (t_math_end - t_math_start);

		Ref<Image> height_image = buffer->get_image();
		Vector3 stamp_position(offset.x, 0.0f, offset.y);

		Array images;
		images.push_back(height_image);
		images.push_back(empty_control_map);
		images.push_back(empty_control_map);

		uint64_t t_t3d_start = Time::get_singleton()->get_ticks_usec();
		target_api->call("import_images", images, stamp_position, 0.0f, 1.0f);
		uint64_t t_t3d_end = Time::get_singleton()->get_ticks_usec();
		total_terrain3d_time += (t_t3d_end - t_t3d_start);

		UtilityFunctions::print("  -> Chunk [", chunk_pos.x, ", ", chunk_pos.y, "] | Math: ", (t_math_end - t_math_start) / 1000.0, " ms | Terrain3D API: ", (t_t3d_end - t_t3d_start) / 1000.0, " ms");
	}

	for (Vector2i cpos : chunks_to_remove) {
		chunk_buffers.erase(cpos);
	}

	uint64_t t_end = Time::get_singleton()->get_ticks_usec();
	UtilityFunctions::print("[Compositor] TOTAL Math Threads Time: ", total_math_time / 1000.0, " ms.");
	UtilityFunctions::print("[Compositor] TOTAL Terrain3D Upload Time: ", total_terrain3d_time / 1000.0, " ms.");
	UtilityFunctions::print("=== [Terrain Master Grid] TOTAL UPDATE TIME: ", (t_end - t_start) / 1000.0, " ms ===\n");
}

} //namespace godot
