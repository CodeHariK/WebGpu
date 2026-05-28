#include "terraspline.h"
#include <cmath>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

// =========================================================
// Terrain3DSplineCompositor Implementation
// =========================================================

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
void Terrain3DSplineCompositor::set_chunk_size(int p_size) { chunk_size = MAX(64, p_size); }
int Terrain3DSplineCompositor::get_chunk_size() const { return chunk_size; }
void Terrain3DSplineCompositor::set_default_elevation(float p_elev) { default_elevation = p_elev; }
float Terrain3DSplineCompositor::get_default_elevation() const { return default_elevation; }
void Terrain3DSplineCompositor::set_auto_apply(bool p_auto) { auto_apply = p_auto; }
bool Terrain3DSplineCompositor::get_auto_apply() const { return auto_apply; }

void Terrain3DSplineCompositor::set_apply_now(bool p_apply) {
	if (p_apply) {
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
	if (auto_apply) {
		queue_rebuild();
	}
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
	if (!terrain)
		return;

	Variant api_var = terrain->get("data");
	if (api_var.get_type() == Variant::NIL || (api_var.get_type() == Variant::OBJECT && (Object *)api_var == nullptr)) {
		api_var = terrain->get("storage");
	}

	Object *target_api = nullptr;
	if (api_var.get_type() == Variant::OBJECT) {
		target_api = api_var;
	}
	if (!target_api) {
		UtilityFunctions::printerr("Terrain3DSplineCompositor: Terrain3D Data/Storage is not initialized!");
		return;
	}

	// 1. GATHER SPLINES & CALCULATE ONE MASSIVE BOUNDING BOX
	TypedArray<Node> children = get_children();
	std::vector<TerrainSpline2D *> splines;
	Rect2 global_bounds;
	bool first = true;

	for (int i = 0; i < children.size(); ++i) {
		TerrainSpline2D *spline = Object::cast_to<TerrainSpline2D>(children[i]);
		if (spline) {
			splines.push_back(spline);
			if (first) {
				global_bounds = spline->get_padded_aabb();
				first = false;
			} else {
				global_bounds = global_bounds.merge(spline->get_padded_aabb());
			}
		}
	}

	if (first)
		return; // No valid splines found

	// 2. SNAP BOUNDS TO INTEGERS
	int min_x = (int)Math::floor(global_bounds.position.x);
	int min_z = (int)Math::floor(global_bounds.position.y);
	int max_x = (int)Math::ceil(global_bounds.position.x + global_bounds.size.x);
	int max_z = (int)Math::ceil(global_bounds.position.y + global_bounds.size.y);

	int w = max_x - min_x + 1;
	int h = max_z - min_z + 1;

	if (w <= 0 || h <= 0)
		return;

	// 3. ALLOCATE SINGLE BUFFER
	Ref<TerrainHeightmap> unified_buffer;
	unified_buffer.instantiate();
	unified_buffer->initialize(w, h, default_elevation);
	Vector2 offset(min_x, min_z);

	// 4. PARAMETRIC BLENDING (ALL SPLINES ONTO ONE BUFFER)
	for (TerrainSpline2D *spline : splines) {
		spline->deform_heightmap(unified_buffer, offset);
	}

	// 5. INJECT INTO TERRAIN3D EXACTLY ONCE
	Ref<Image> height_image = unified_buffer->get_image();
	Vector3 stamp_position(min_x, 0.0f, min_z);

	Ref<Image> empty_img;
	empty_img.instantiate();

	Array images;
	images.push_back(height_image);
	images.push_back(empty_img); // Control Map
	images.push_back(empty_img); // Color Map

	target_api->call("import_images", images, stamp_position, 0.0f, 1.0f);
}

} //namespace godot