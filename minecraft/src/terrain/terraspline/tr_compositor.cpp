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
void Terrain3DSplineCompositor::set_default_elevation(float p_elev) { default_elevation = p_elev; }
float Terrain3DSplineCompositor::get_default_elevation() const { return default_elevation; }
void Terrain3DSplineCompositor::set_auto_apply(bool p_auto) { auto_apply = p_auto; }
bool Terrain3DSplineCompositor::get_auto_apply() const { return auto_apply; }
void Terrain3DSplineCompositor::set_apply_now(bool p_apply) {
	if (p_apply)
		apply_all_splines();
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
	UtilityFunctions::print("\n=== [Terrain Master Grid] UPDATE STARTED ===");

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

	// 1. Gather all splines
	TypedArray<Node> children = get_children();
	std::vector<TerrainSpline2D *> splines;
	for (int i = 0; i < children.size(); ++i) {
		TerrainSpline2D *spline = Object::cast_to<TerrainSpline2D>(children[i]);
		if (spline)
			splines.push_back(spline);
	}

	uint64_t t_gather = Time::get_singleton()->get_ticks_usec();
	UtilityFunctions::print("[Compositor] Gathered ", (int)splines.size(), " splines in: ", (t_gather - t_start) / 1000.0, " ms.");

	// 2. Identify all Grid Chunks currently touched by splines
	HashMap<Vector2i, bool> active_grid_chunks;
	for (TerrainSpline2D *spline : splines) {
		Rect2 aabb = spline->get_padded_aabb();
		int min_cx = (int)Math::floor(aabb.position.x / CHUNK_SIZE);
		int max_cx = (int)Math::floor((aabb.position.x + aabb.size.x) / CHUNK_SIZE);
		int min_cz = (int)Math::floor(aabb.position.y / CHUNK_SIZE);
		int max_cz = (int)Math::floor((aabb.position.y + aabb.size.y) / CHUNK_SIZE);

		for (int cx = min_cx; cx <= max_cx; ++cx) {
			for (int cz = min_cz; cz <= max_cz; ++cz) {
				active_grid_chunks[Vector2i(cx, cz)] = true;
			}
		}
	}

	// 3. Include previously generated chunks (so we can erase them if a spline moved away)
	for (const KeyValue<Vector2i, Ref<TerrainHeightmap>> &E : chunk_buffers) {
		active_grid_chunks[E.key] = true;
	}

	uint64_t t_grid = Time::get_singleton()->get_ticks_usec();
	UtilityFunctions::print("[Compositor] Calculated ", active_grid_chunks.size(), " active grid chunks in: ", (t_grid - t_gather) / 1000.0, " ms.");

	if (active_grid_chunks.size() == 0)
		return;

	// 4. Process all active chunks in the grid
	std::vector<Vector2i> chunks_to_remove;
	Ref<Image> empty_control_map;
	empty_control_map.instantiate();

	uint64_t total_math_time = 0;
	uint64_t total_terrain3d_time = 0;

	for (const KeyValue<Vector2i, bool> &E : active_grid_chunks) {
		Vector2i chunk_pos = E.key;
		Vector2 offset(chunk_pos.x * CHUNK_SIZE, chunk_pos.y * CHUNK_SIZE);
		Rect2 chunk_rect(offset, Vector2(CHUNK_SIZE, CHUNK_SIZE));

		// Check if this chunk still has splines intersecting it
		bool has_splines = false;
		for (TerrainSpline2D *spline : splines) {
			if (spline->get_padded_aabb().intersects(chunk_rect)) {
				has_splines = true;
				break;
			}
		}

		// Get or Create the C++ Memory Buffer
		Ref<TerrainHeightmap> buffer;
		if (chunk_buffers.has(chunk_pos)) {
			buffer = chunk_buffers[chunk_pos];
		} else {
			if (!has_splines)
				continue; // Don't create new memory for empty space
			buffer.instantiate();
			buffer->initialize(CHUNK_SIZE, CHUNK_SIZE, default_elevation);
			chunk_buffers[chunk_pos] = buffer;
		}

		// Reset the chunk canvas
		buffer->clear(default_elevation);

		uint64_t t_math_start = Time::get_singleton()->get_ticks_usec();
		if (has_splines) {
			// Parametric Blending: Draw all intersecting splines onto this fixed chunk
			for (TerrainSpline2D *spline : splines) {
				if (spline->get_padded_aabb().intersects(chunk_rect)) {
					spline->deform_heightmap(buffer, offset);
				}
			}
		} else {
			// The spline moved away! We flattened it via clear(), now flag it for memory deletion
			chunks_to_remove.push_back(chunk_pos);
		}
		uint64_t t_math_end = Time::get_singleton()->get_ticks_usec();
		total_math_time += (t_math_end - t_math_start);

		// 5. Inject the Fixed Grid Chunk into Terrain3D
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

	// 6. Memory Management: Delete abandoned chunks from RAM
	for (Vector2i cpos : chunks_to_remove) {
		chunk_buffers.erase(cpos);
	}

	uint64_t t_end = Time::get_singleton()->get_ticks_usec();
	UtilityFunctions::print("[Compositor] TOTAL Math Threads Time: ", total_math_time / 1000.0, " ms.");
	UtilityFunctions::print("[Compositor] TOTAL Terrain3D Upload Time: ", total_terrain3d_time / 1000.0, " ms.");
	UtilityFunctions::print("=== [Terrain Master Grid] TOTAL UPDATE TIME: ", (t_end - t_start) / 1000.0, " ms ===\n");
}

} //namespace godot