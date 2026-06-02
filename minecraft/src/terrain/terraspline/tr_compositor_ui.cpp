#include "godot_cpp/classes/image_texture.hpp"
#include "terraspline.h"
#include <godot_cpp/classes/time.hpp> // NEW: For high-precision profiling
#include <godot_cpp/classes/worker_thread_pool.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

/**
 * @brief Binds TerrainSplineCompositorUI methods and properties to Godot's ClassDB.
 * Exposes target default elevation, auto apply state, apply now actions, and connection callbacks.
 */
void TerrainSplineCompositorUI::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_default_elevation", "elevation"), &TerrainSplineCompositorUI::set_default_elevation);
	ClassDB::bind_method(D_METHOD("get_default_elevation"), &TerrainSplineCompositorUI::get_default_elevation);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "default_elevation"), "set_default_elevation", "get_default_elevation");


	ClassDB::bind_method(D_METHOD("set_auto_apply", "auto_apply"), &TerrainSplineCompositorUI::set_auto_apply);
	ClassDB::bind_method(D_METHOD("get_auto_apply"), &TerrainSplineCompositorUI::get_auto_apply);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "auto_apply"), "set_auto_apply", "get_auto_apply");

	ClassDB::bind_method(D_METHOD("set_apply_now", "apply_now"), &TerrainSplineCompositorUI::set_apply_now);
	ClassDB::bind_method(D_METHOD("get_apply_now"), &TerrainSplineCompositorUI::get_apply_now);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "apply_now"), "set_apply_now", "get_apply_now");

	ClassDB::bind_method(D_METHOD("queue_rebuild"), &TerrainSplineCompositorUI::queue_rebuild);
	ClassDB::bind_method(D_METHOD("_execute_rebuild"), &TerrainSplineCompositorUI::_execute_rebuild);
	ClassDB::bind_method(D_METHOD("apply_all_splines"), &TerrainSplineCompositorUI::apply_all_splines);
	ClassDB::bind_method(D_METHOD("_connect_spline", "node"), &TerrainSplineCompositorUI::_connect_spline);
	ClassDB::bind_method(D_METHOD("_disconnect_spline", "node"), &TerrainSplineCompositorUI::_disconnect_spline);
	ClassDB::bind_method(D_METHOD("_on_spline_changed"), &TerrainSplineCompositorUI::_on_spline_changed);
	ClassDB::bind_method(D_METHOD("_normalize_grayscale_task", "task_idx", "job"), &TerrainSplineCompositorUI::_normalize_grayscale_task);
}

/**
 * @brief Default Constructor. Configures default TextureRect stretch and expansion settings.
 */
TerrainSplineCompositorUI::TerrainSplineCompositorUI() {
	// Make sure the TextureRect expands to fit the screen
	set_expand_mode(TextureRect::EXPAND_IGNORE_SIZE);
	set_stretch_mode(TextureRect::STRETCH_KEEP_ASPECT_CENTERED);
}
/**
 * @brief Default Destructor.
 */
TerrainSplineCompositorUI::~TerrainSplineCompositorUI() {}

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

/**
 * @brief Handles engine-level notifications (Ready state).
 * Gathers existing splines, connects change callbacks, and schedules initial UI generation.
 */
void TerrainSplineCompositorUI::_notification(int p_what) {
	if (p_what == Node::NOTIFICATION_READY) {
		TypedArray<Node> children = get_children();
		for (int i = 0; i < children.size(); ++i) {
			_connect_spline(Object::cast_to<Node>(children[i]));
		}
		connect("child_entered_tree", Callable(this, "_connect_spline"));
		connect("child_exiting_tree", Callable(this, "_disconnect_spline"));
		call_deferred("apply_all_splines");
	} else if (p_what == Node::NOTIFICATION_CHILD_ORDER_CHANGED) {
		queue_rebuild();
	}
}

/**
 * @brief Subscribes to the change signals of child ProceduralSpline3D nodes.
 */
void TerrainSplineCompositorUI::_connect_spline(Node *p_node) {
	ProceduralSpline3D *spline = Object::cast_to<ProceduralSpline3D>(p_node);
	if (spline) {
		if (!spline->is_connected("spline_changed", Callable(this, "_on_spline_changed"))) {
			spline->connect("spline_changed", Callable(this, "_on_spline_changed"));
		}
	}
}

void TerrainSplineCompositorUI::_disconnect_spline(Node *p_node) {
	ProceduralSpline3D *spline = Object::cast_to<ProceduralSpline3D>(p_node);
	if (spline) {
		if (spline->is_connected("spline_changed", Callable(this, "_on_spline_changed"))) {
			spline->disconnect("spline_changed", Callable(this, "_on_spline_changed"));
		}
		queue_rebuild();
	}
}

/**
 * @brief Callback triggered when a tracked spline's coordinates or shape are updated.
 */
void TerrainSplineCompositorUI::_on_spline_changed() {
	if (auto_apply) {
		queue_rebuild();
	}
}

/**
 * @brief Queues a deferred UI update to avoid redundant drawing frames.
 */
void TerrainSplineCompositorUI::queue_rebuild() {
	if (!_rebuild_queued) {
		_rebuild_queued = true;
		call_deferred("_execute_rebuild");
	}
}

/**
 * @brief Executes the deferred UI drawing operation.
 */
void TerrainSplineCompositorUI::_execute_rebuild() {
	_rebuild_queued = false;
	apply_all_splines();
}

/**
 * @brief Regenerates the grayscale terrain elevation preview image.
 * Gathers all spline deformers, allocates a single unified heightmap buffer, sculpts it,
 * normalizes the height ranges to a grayscale space, and draws the output to the UI TextureRect.
 */
void TerrainSplineCompositorUI::apply_all_splines() {
	uint64_t step1_start = Time::get_singleton()->get_ticks_usec();
	UtilityFunctions::print("=== [CompositorUI] apply_all_splines() STARTED ===");

	// 1. GATHER SPLINES & CALCULATE ONE MASSIVE BOUNDING BOX
	TypedArray<Node> children = get_children();
	std::vector<ProceduralSpline3D *> splines;
	Rect2 global_bounds;
	bool first = true;

	for (int i = 0; i < children.size(); ++i) {
		ProceduralSpline3D *spline = Object::cast_to<ProceduralSpline3D>(children[i]);
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

	if (first) {
		UtilityFunctions::print("[CompositorUI] ABORT: No splines found.");
		return;
	}

	uint64_t step2_bounds = Time::get_singleton()->get_ticks_usec();
	UtilityFunctions::print("[CompositorUI] Gathered ", (int)splines.size(), " splines and calculated bounds in: ", (step2_bounds - step1_start) / 1000.0, " ms.");

	// 2. SNAP BOUNDS TO INTEGERS
	int min_x = (int)Math::floor(global_bounds.position.x);
	int min_z = (int)Math::floor(global_bounds.position.y);
	int max_x = (int)Math::ceil(global_bounds.position.x + global_bounds.size.x);
	int max_z = (int)Math::ceil(global_bounds.position.y + global_bounds.size.y);

	int w = max_x - min_x + 1;
	int h = max_z - min_z + 1;

	if (w <= 0 || h <= 0)
		return;

	if (w > 2048 || h > 2048) {
		UtilityFunctions::print("[CompositorUI] Bounding box is too large (", w, "x", h, "). Capping preview to 2048 to avoid main thread freeze.");
		w = Math::min(w, 2048);
		h = Math::min(h, 2048);
	}

	// 3. ALLOCATE SINGLE BUFFER
	Ref<TerrainHeightmap> unified_buffer;
	unified_buffer.instantiate();
	unified_buffer->initialize(w, h, default_elevation);
	Vector2 offset(min_x, min_z);

	uint64_t step3_alloc = Time::get_singleton()->get_ticks_usec();
	UtilityFunctions::print("[CompositorUI] Allocated ", w, "x", h, " buffer in: ", (step3_alloc - step2_bounds) / 1000.0, " ms.");

	// 4. PARAMETRIC BLENDING (ALL SPLINES ONTO ONE BUFFER)
	for (ProceduralSpline3D *spline : splines) {
		TypedArray<Node> children = spline->get_children();
		for (int i = 0; i < children.size(); ++i) {
			TerrainSplineDeformer *deformer = Object::cast_to<TerrainSplineDeformer>(children[i]);
			if (deformer) {
				deformer->deform_heightmap(unified_buffer, spline, offset);
			}
		}
	}

	uint64_t step4_deform = Time::get_singleton()->get_ticks_usec();
	UtilityFunctions::print("[CompositorUI] All Spline Thread Tasks completed in: ", (step4_deform - step3_alloc) / 1000.0, " ms.");

	// 5. NORMALIZE FLOAT HEIGHTMAP TO GRAYSCALE UI TEXTURE
	int sz = w * h;
	float *ptr = unified_buffer->get_data_ptrw();

	float min_h = 1e20f;
	float max_h = -1e20f;
	for (int i = 0; i < sz; ++i) {
		if (ptr[i] < min_h)
			min_h = ptr[i];
		if (ptr[i] > max_h)
			max_h = ptr[i];
	}

	float range = max_h - min_h;
	if (range < 0.0001f)
		range = 1.0f;

	Ref<Image> ui_img = Image::create_empty(w, h, false, Image::FORMAT_L8);
	PackedByteArray img_data;
	img_data.resize(sz);
	uint8_t *byte_ptr = img_data.ptrw();

	Ref<GrayscaleJob> job;
	job.instantiate();
	job->ptr = ptr;
	job->byte_ptr = byte_ptr;
	job->min_h = min_h;
	job->range = range;
	job->size = sz;
	job->chunk_size = 16384;

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

	ui_img->set_data(w, h, false, Image::FORMAT_L8, img_data);

	uint64_t step5_norm = Time::get_singleton()->get_ticks_usec();
	UtilityFunctions::print("[CompositorUI] Converted array to UI Grayscale Image in: ", (step5_norm - step4_deform) / 1000.0, " ms.");

	// 6. DRAW TO UI
	Ref<ImageTexture> tex = ImageTexture::create_from_image(ui_img);
	set_texture(tex);

	uint64_t step6_end = Time::get_singleton()->get_ticks_usec();
	UtilityFunctions::print("=== [CompositorUI] END TOTAL TIME: ", (step6_end - step1_start) / 1000.0, " ms ===\n");
}

void TerrainSplineCompositorUI::_normalize_grayscale_task(int p_task_idx, Ref<GrayscaleJob> p_job) {
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

} //namespace godot
