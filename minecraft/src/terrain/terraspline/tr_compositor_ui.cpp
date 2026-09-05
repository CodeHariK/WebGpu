/**
 * @file tr_compositor_ui.cpp
 * @brief TerrainSplineCompositorUI: a grayscale preview of the combined spline heightmap.
 */
#include "tr_compositor_ui.h"
#include "tr_deformer.h"
#include "utils/spline3d/procedural_spline3d.h"
#include <godot_cpp/classes/image_texture.hpp>
#include <godot_cpp/classes/input_event_key.hpp>
#include <godot_cpp/classes/time.hpp>
#include <godot_cpp/classes/worker_thread_pool.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

static constexpr int PREVIEW_MAX_SIZE = 2048; // Cap to avoid freezing the main thread
static constexpr int GRAYSCALE_TASK_SIZE = 16384; // Elements per normalization task

void TerrainSplineCompositorUI::_bind_methods() {
	ClassDB::bind_method(
			D_METHOD("set_default_elevation", "elevation"), &TerrainSplineCompositorUI::set_default_elevation
	);
	ClassDB::bind_method(D_METHOD("get_default_elevation"), &TerrainSplineCompositorUI::get_default_elevation);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "default_elevation"), "set_default_elevation", "get_default_elevation");

	ClassDB::bind_method(D_METHOD("set_auto_apply", "auto_apply"), &TerrainSplineCompositorUI::set_auto_apply);
	ClassDB::bind_method(D_METHOD("get_auto_apply"), &TerrainSplineCompositorUI::get_auto_apply);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "auto_apply"), "set_auto_apply", "get_auto_apply");

	ClassDB::bind_method(D_METHOD("set_apply_now", "apply_now"), &TerrainSplineCompositorUI::set_apply_now);
	ClassDB::bind_method(D_METHOD("get_apply_now"), &TerrainSplineCompositorUI::get_apply_now);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "apply_now"), "set_apply_now", "get_apply_now");

	ClassDB::bind_method(D_METHOD("set_splines_root", "path"), &TerrainSplineCompositorUI::set_splines_root);
	ClassDB::bind_method(D_METHOD("get_splines_root"), &TerrainSplineCompositorUI::get_splines_root);
	ADD_PROPERTY(
			PropertyInfo(Variant::NODE_PATH, "splines_root", PROPERTY_HINT_NODE_PATH_VALID_TYPES, "Node"),
			"set_splines_root", "get_splines_root"
	);

	ClassDB::bind_method(D_METHOD("set_toggle_key", "key"), &TerrainSplineCompositorUI::set_toggle_key);
	ClassDB::bind_method(D_METHOD("get_toggle_key"), &TerrainSplineCompositorUI::get_toggle_key);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "toggle_key"), "set_toggle_key", "get_toggle_key");

	ClassDB::bind_method(D_METHOD("queue_rebuild"), &TerrainSplineCompositorUI::queue_rebuild);
	ClassDB::bind_method(D_METHOD("_execute_rebuild"), &TerrainSplineCompositorUI::_execute_rebuild);
	ClassDB::bind_method(D_METHOD("apply_all_splines"), &TerrainSplineCompositorUI::apply_all_splines);
	ClassDB::bind_method(D_METHOD("_connect_spline", "node"), &TerrainSplineCompositorUI::_connect_spline);
	ClassDB::bind_method(D_METHOD("_disconnect_spline", "node"), &TerrainSplineCompositorUI::_disconnect_spline);
	ClassDB::bind_method(D_METHOD("_on_spline_changed"), &TerrainSplineCompositorUI::_on_spline_changed);
	ClassDB::bind_method(
			D_METHOD("_normalize_grayscale_task", "task_idx", "job"),
			&TerrainSplineCompositorUI::_normalize_grayscale_task
	);
}

TerrainSplineCompositorUI::TerrainSplineCompositorUI() {
	set_expand_mode(TextureRect::EXPAND_IGNORE_SIZE);
	set_stretch_mode(TextureRect::STRETCH_KEEP_ASPECT_CENTERED);
}

TerrainSplineCompositorUI::~TerrainSplineCompositorUI() {}

// ---------------------------------------------------------------------------------------------
// Properties
// ---------------------------------------------------------------------------------------------

void TerrainSplineCompositorUI::set_default_elevation(float p_elev) {
	if (default_elevation != p_elev) {
		default_elevation = p_elev;
		queue_rebuild();
	}
}
float TerrainSplineCompositorUI::get_default_elevation() const { return default_elevation; }
void TerrainSplineCompositorUI::set_auto_apply(bool p_auto) { auto_apply = p_auto; }
bool TerrainSplineCompositorUI::get_auto_apply() const { return auto_apply; }
void TerrainSplineCompositorUI::set_apply_now(bool p_apply) {
	if (p_apply) {
		apply_all_splines();
	}
}
bool TerrainSplineCompositorUI::get_apply_now() const { return false; }

void TerrainSplineCompositorUI::set_splines_root(const NodePath &p_path) {
	splines_root = p_path;
	if (is_inside_tree()) {
		_watch_root(_resolve_splines_root());
		queue_rebuild();
	}
}
NodePath TerrainSplineCompositorUI::get_splines_root() const { return splines_root; }
void TerrainSplineCompositorUI::set_toggle_key(Key p_key) { toggle_key = p_key; }
Key TerrainSplineCompositorUI::get_toggle_key() const { return toggle_key; }

// ---------------------------------------------------------------------------------------------
// Lifecycle and spline wiring
// ---------------------------------------------------------------------------------------------

void TerrainSplineCompositorUI::_notification(int p_what) {
	if (p_what == Node::NOTIFICATION_READY) {
		set_process_unhandled_key_input(toggle_key != KEY_NONE);
		_watch_root(_resolve_splines_root());
		call_deferred("apply_all_splines");
	} else if (p_what == Node::NOTIFICATION_EXIT_TREE) {
		_unwatch_root();
	} else if (p_what == Node::NOTIFICATION_CHILD_ORDER_CHANGED) {
		queue_rebuild();
	}
}

void TerrainSplineCompositorUI::_unhandled_key_input(const Ref<InputEvent> &p_event) {
	Ref<InputEventKey> key = p_event;
	if (key.is_valid() && key->is_pressed() && !key->is_echo() && key->get_keycode() == toggle_key) {
		set_visible(!is_visible());
		if (is_visible()) {
			queue_rebuild();
		}
	}
}

/// The node whose ProceduralSpline3D children are previewed: `splines_root`, or this node if unset/invalid.
Node *TerrainSplineCompositorUI::_resolve_splines_root() const {
	if (!splines_root.is_empty()) {
		Node *root = get_node_or_null(splines_root);
		if (root) {
			return root;
		}
		UtilityFunctions::push_warning(
				"[CompositorUI] splines_root '", splines_root, "' not found; using own children."
		);
	}
	return const_cast<TerrainSplineCompositorUI *>(this);
}

/// Connects to every spline under p_root and to its child add/remove signals; drops the previous root.
void TerrainSplineCompositorUI::_watch_root(Node *p_root) {
	if (p_root == _watched_root) {
		return;
	}
	_unwatch_root();
	if (!p_root) {
		return;
	}
	_watched_root = p_root;
	TypedArray<Node> children = p_root->get_children();
	for (int i = 0; i < children.size(); ++i) {
		_connect_spline(Object::cast_to<Node>(children[i]));
	}
	p_root->connect("child_entered_tree", Callable(this, "_connect_spline"));
	p_root->connect("child_exiting_tree", Callable(this, "_disconnect_spline"));
}

void TerrainSplineCompositorUI::_unwatch_root() {
	if (!_watched_root) {
		return;
	}
	TypedArray<Node> children = _watched_root->get_children();
	for (int i = 0; i < children.size(); ++i) {
		ProceduralSpline3D *spline = Object::cast_to<ProceduralSpline3D>(children[i]);
		if (spline && spline->is_connected("spline_changed", Callable(this, "_on_spline_changed"))) {
			spline->disconnect("spline_changed", Callable(this, "_on_spline_changed"));
		}
	}
	_watched_root->disconnect("child_entered_tree", Callable(this, "_connect_spline"));
	_watched_root->disconnect("child_exiting_tree", Callable(this, "_disconnect_spline"));
	_watched_root = nullptr;
}

void TerrainSplineCompositorUI::_connect_spline(Node *p_node) {
	ProceduralSpline3D *spline = Object::cast_to<ProceduralSpline3D>(p_node);
	if (spline && !spline->is_connected("spline_changed", Callable(this, "_on_spline_changed"))) {
		spline->connect("spline_changed", Callable(this, "_on_spline_changed"));
	}
}

void TerrainSplineCompositorUI::_disconnect_spline(Node *p_node) {
	ProceduralSpline3D *spline = Object::cast_to<ProceduralSpline3D>(p_node);
	if (!spline) {
		return;
	}
	if (spline->is_connected("spline_changed", Callable(this, "_on_spline_changed"))) {
		spline->disconnect("spline_changed", Callable(this, "_on_spline_changed"));
	}
	queue_rebuild();
}

void TerrainSplineCompositorUI::_on_spline_changed() {
	if (auto_apply) {
		queue_rebuild();
	}
}

void TerrainSplineCompositorUI::queue_rebuild() {
	if (!_rebuild_queued) {
		_rebuild_queued = true;
		call_deferred("_execute_rebuild");
	}
}

void TerrainSplineCompositorUI::_execute_rebuild() {
	_rebuild_queued = false;
	apply_all_splines();
}

// ---------------------------------------------------------------------------------------------
// Preview generation
// ---------------------------------------------------------------------------------------------

void TerrainSplineCompositorUI::apply_all_splines() {
	uint64_t t_start = Time::get_singleton()->get_ticks_usec();
	UtilityFunctions::print("=== [CompositorUI] apply_all_splines() STARTED ===");

	Rect2 global_bounds;
	std::vector<ProceduralSpline3D *> splines = _gather_splines(global_bounds);
	if (splines.empty()) {
		UtilityFunctions::print(
				"[CompositorUI] ABORT: No splines found under '",
				_watched_root ? _watched_root->get_path() : get_path(), "' (splines_root = '", splines_root, "')."
		);
		return;
	}
	uint64_t t_bounds = Time::get_singleton()->get_ticks_usec();
	UtilityFunctions::print(
			"[CompositorUI] Gathered ", (int)splines.size(),
			" splines and calculated bounds in: ", (t_bounds - t_start) / 1000.0, " ms."
	);

	int w = 0, h = 0;
	Ref<TerrainHeightmap> unified = _deform_unified_heightmap(splines, global_bounds, w, h);
	if (unified.is_null()) {
		return;
	}
	uint64_t t_deform = Time::get_singleton()->get_ticks_usec();

	_show_as_grayscale(unified, w, h);
	uint64_t t_end = Time::get_singleton()->get_ticks_usec();
	UtilityFunctions::print(
			"[CompositorUI] Converted array to UI Grayscale Image in: ", (t_end - t_deform) / 1000.0, " ms."
	);
	UtilityFunctions::print("=== [CompositorUI] END TOTAL TIME: ", (t_end - t_start) / 1000.0, " ms ===\n");
}

/// Splines under the watched root and the union of their padded AABBs.
std::vector<ProceduralSpline3D *> TerrainSplineCompositorUI::_gather_splines(Rect2 &r_bounds) const {
	std::vector<ProceduralSpline3D *> splines;
	const Node *root = _watched_root ? _watched_root : this;
	TypedArray<Node> children = root->get_children();
	for (int i = 0; i < children.size(); ++i) {
		ProceduralSpline3D *spline = Object::cast_to<ProceduralSpline3D>(children[i]);
		if (!spline) {
			continue;
		}
		r_bounds = splines.empty() ? spline->get_padded_aabb() : r_bounds.merge(spline->get_padded_aabb());
		splines.push_back(spline);
	}
	return splines;
}

/// One heightmap covering the bounds (capped to PREVIEW_MAX_SIZE), with every deformer applied.
Ref<TerrainHeightmap> TerrainSplineCompositorUI::_deform_unified_heightmap(
		const std::vector<ProceduralSpline3D *> &p_splines,
		const Rect2 &p_bounds,
		int &r_w,
		int &r_h
) {
	int min_x = (int)Math::floor(p_bounds.position.x);
	int min_z = (int)Math::floor(p_bounds.position.y);
	int max_x = (int)Math::ceil(p_bounds.position.x + p_bounds.size.x);
	int max_z = (int)Math::ceil(p_bounds.position.y + p_bounds.size.y);
	r_w = max_x - min_x + 1;
	r_h = max_z - min_z + 1;
	if (r_w <= 0 || r_h <= 0) {
		return Ref<TerrainHeightmap>();
	}
	if (r_w > PREVIEW_MAX_SIZE || r_h > PREVIEW_MAX_SIZE) {
		UtilityFunctions::print(
				"[CompositorUI] Bounding box is too large (", r_w, "x", r_h, "). Capping preview to ", PREVIEW_MAX_SIZE,
				" to avoid main thread freeze."
		);
		r_w = Math::min(r_w, PREVIEW_MAX_SIZE);
		r_h = Math::min(r_h, PREVIEW_MAX_SIZE);
	}

	Ref<TerrainHeightmap> buffer;
	buffer.instantiate();
	buffer->initialize(r_w, r_h, default_elevation);
	Vector2 offset(min_x, min_z);

	uint64_t t0 = Time::get_singleton()->get_ticks_usec();
	for (ProceduralSpline3D *spline : p_splines) {
		TypedArray<Node> children = spline->get_children();
		for (int i = 0; i < children.size(); ++i) {
			TerrainSplineDeformer *deformer = Object::cast_to<TerrainSplineDeformer>(children[i]);
			if (deformer) {
				deformer->deform_heightmap(buffer, spline, offset);
			}
		}
	}
	UtilityFunctions::print(
			"[CompositorUI] All Spline Thread Tasks completed in: ",
			(Time::get_singleton()->get_ticks_usec() - t0) / 1000.0, " ms."
	);
	return buffer;
}

/// Normalizes the buffer to 0..255 (in parallel) and sets it as this TextureRect's texture.
void TerrainSplineCompositorUI::_show_as_grayscale(
		const Ref<TerrainHeightmap> &p_buffer,
		int p_w,
		int p_h
) {
	int sz = p_w * p_h;
	float *ptr = p_buffer->get_data_ptrw();
	float min_h = 1e20f, max_h = -1e20f;
	for (int i = 0; i < sz; ++i) {
		min_h = Math::min(min_h, ptr[i]);
		max_h = Math::max(max_h, ptr[i]);
	}
	float range = max_h - min_h;
	if (range < 0.0001f) {
		range = 1.0f;
	}
	UtilityFunctions::print("[CompositorUI] preview ", p_w, "x", p_h, " height range ", min_h, " .. ", max_h);

	PackedByteArray img_data;
	img_data.resize(sz);

	Ref<GrayscaleJob> job;
	job.instantiate();
	job->ptr = ptr;
	job->byte_ptr = img_data.ptrw();
	job->min_h = min_h;
	job->range = range;
	job->size = sz;
	job->chunk_size = GRAYSCALE_TASK_SIZE;

	int num_tasks = (sz + job->chunk_size - 1) / job->chunk_size;
	WorkerThreadPool *wtp = WorkerThreadPool::get_singleton();
	if (wtp && num_tasks > 0) {
		Callable task_callable = Callable(this, "_normalize_grayscale_task").bind(job);
		int group_id = wtp->add_group_task(task_callable, num_tasks, -1, true, "TerraSplineUI_Grayscale_Normalize");
		wtp->wait_for_group_task_completion(group_id);
	} else {
		for (int i = 0; i < num_tasks; ++i) {
			_normalize_grayscale_task(i, job);
		}
	}

	Ref<Image> ui_img = Image::create_empty(p_w, p_h, false, Image::FORMAT_L8);
	ui_img->set_data(p_w, p_h, false, Image::FORMAT_L8, img_data);
	set_texture(ImageTexture::create_from_image(ui_img));
}

void TerrainSplineCompositorUI::_normalize_grayscale_task(
		int p_task_idx,
		Ref<GrayscaleJob> p_job
) {
	if (p_job.is_null() || p_job->ptr == nullptr || p_job->byte_ptr == nullptr) {
		return;
	}
	int start = p_task_idx * p_job->chunk_size;
	int end = Math::min(start + p_job->chunk_size, p_job->size);
	const float *src = p_job->ptr;
	uint8_t *dest = p_job->byte_ptr;
	float min_h = p_job->min_h;
	float inv_range = 1.0f / p_job->range;
	for (int i = start; i < end; ++i) {
		float normalized = (src[i] - min_h) * inv_range;
		dest[i] = (uint8_t)(Math::clamp(normalized, 0.0f, 1.0f) * 255.0f);
	}
}

} // namespace godot
