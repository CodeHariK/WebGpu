/**
 * @file tr_deformer.cpp
 * @brief TerrainSplineDeformer: bindings, curve wiring, entry points and task dispatch.
 *
 * The evaluation itself lives in tr_deformer_field.cpp (distance field), tr_deformer_legacy.cpp
 * (job creation, tile-culling fallback) and tr_deformer_pixel.cpp (per-pixel weight/height/blend).
 */
#include "tr_deformer.h"
#include <godot_cpp/classes/time.hpp>
#include <godot_cpp/classes/worker_thread_pool.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

void TerrainSplineDeformer::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_max_height", "max_height"), &TerrainSplineDeformer::set_max_height);
	ClassDB::bind_method(D_METHOD("get_max_height"), &TerrainSplineDeformer::get_max_height);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "max_height"), "set_max_height", "get_max_height");

	ClassDB::bind_method(D_METHOD("set_spline_width", "spline_width"), &TerrainSplineDeformer::set_spline_width);
	ClassDB::bind_method(D_METHOD("get_spline_width"), &TerrainSplineDeformer::get_spline_width);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "spline_width"), "set_spline_width", "get_spline_width");

	ClassDB::bind_method(
			D_METHOD("set_falloff_distance", "falloff_distance"), &TerrainSplineDeformer::set_falloff_distance
	);
	ClassDB::bind_method(D_METHOD("get_falloff_distance"), &TerrainSplineDeformer::get_falloff_distance);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "falloff_distance"), "set_falloff_distance", "get_falloff_distance");

	ClassDB::bind_method(
			D_METHOD("set_inner_falloff_distance", "dist"), &TerrainSplineDeformer::set_inner_falloff_distance
	);
	ClassDB::bind_method(D_METHOD("get_inner_falloff_distance"), &TerrainSplineDeformer::get_inner_falloff_distance);
	ADD_PROPERTY(
			PropertyInfo(Variant::FLOAT, "inner_falloff_distance"), "set_inner_falloff_distance",
			"get_inner_falloff_distance"
	);

	ClassDB::bind_method(D_METHOD("set_blend_mode", "blend_mode"), &TerrainSplineDeformer::set_blend_mode);
	ClassDB::bind_method(D_METHOD("get_blend_mode"), &TerrainSplineDeformer::get_blend_mode);
	ADD_PROPERTY(
			PropertyInfo(
					Variant::INT, "blend_mode", PROPERTY_HINT_ENUM,
					"Add (terrain + (spline Y + max_height - base) * weight),Subtract (terrain - (spline Y + "
					"max_height - base) * weight),Max (raise to spline Y + max_height),Min (lower to spline Y + "
					"max_height),Replace (blend to spline Y + max_height)"
			),
			"set_blend_mode", "get_blend_mode"
	);

	ClassDB::bind_method(D_METHOD("set_height_source", "source"), &TerrainSplineDeformer::set_height_source);
	ClassDB::bind_method(D_METHOD("get_height_source"), &TerrainSplineDeformer::get_height_source);
	ADD_PROPERTY(
			PropertyInfo(
					Variant::INT, "height_source", PROPERTY_HINT_ENUM,
					"Spline (control point Y),Terrain (follow the ground; road)"
			),
			"set_height_source", "get_height_source"
	);
	ClassDB::bind_method(D_METHOD("set_profile_smoothing", "metres"), &TerrainSplineDeformer::set_profile_smoothing);
	ClassDB::bind_method(D_METHOD("get_profile_smoothing"), &TerrainSplineDeformer::get_profile_smoothing);
	ADD_PROPERTY(
			PropertyInfo(Variant::FLOAT, "profile_smoothing", PROPERTY_HINT_RANGE, "0,500,1,suffix:m"),
			"set_profile_smoothing", "get_profile_smoothing"
	);
	ClassDB::bind_method(D_METHOD("set_max_grade", "percent"), &TerrainSplineDeformer::set_max_grade);
	ClassDB::bind_method(D_METHOD("get_max_grade"), &TerrainSplineDeformer::get_max_grade);
	ADD_PROPERTY(
			PropertyInfo(Variant::FLOAT, "max_grade", PROPERTY_HINT_RANGE, "0,100,0.5,suffix:%"), "set_max_grade",
			"get_max_grade"
	);

	ClassDB::bind_method(
			D_METHOD("set_profile_include_splines", "include"), &TerrainSplineDeformer::set_profile_include_splines
	);
	ClassDB::bind_method(D_METHOD("get_profile_include_splines"), &TerrainSplineDeformer::get_profile_include_splines);
	ADD_PROPERTY(
			PropertyInfo(Variant::BOOL, "profile_include_splines"), "set_profile_include_splines",
			"get_profile_include_splines"
	);

	ClassDB::bind_method(D_METHOD("set_earthwork", "mode"), &TerrainSplineDeformer::set_earthwork);
	ClassDB::bind_method(D_METHOD("get_earthwork"), &TerrainSplineDeformer::get_earthwork);
	ADD_PROPERTY(
			PropertyInfo(
					Variant::INT, "earthwork", PROPERTY_HINT_ENUM,
					"Cut and fill (embankments and cuttings),Cut only (never above the ground),Fill only (never below "
					"the ground)"
			),
			"set_earthwork", "get_earthwork"
	);

	ClassDB::bind_method(D_METHOD("set_max_cut_depth", "metres"), &TerrainSplineDeformer::set_max_cut_depth);
	ClassDB::bind_method(D_METHOD("get_max_cut_depth"), &TerrainSplineDeformer::get_max_cut_depth);
	ADD_PROPERTY(
			PropertyInfo(Variant::FLOAT, "max_cut_depth", PROPERTY_HINT_RANGE, "0,200,0.1,suffix:m"),
			"set_max_cut_depth", "get_max_cut_depth"
	);
	ClassDB::bind_method(D_METHOD("set_max_fill_height", "metres"), &TerrainSplineDeformer::set_max_fill_height);
	ClassDB::bind_method(D_METHOD("get_max_fill_height"), &TerrainSplineDeformer::get_max_fill_height);
	ADD_PROPERTY(
			PropertyInfo(Variant::FLOAT, "max_fill_height", PROPERTY_HINT_RANGE, "0,200,0.1,suffix:m"),
			"set_max_fill_height", "get_max_fill_height"
	);

	ClassDB::bind_method(D_METHOD("set_road_blur", "metres"), &TerrainSplineDeformer::set_road_blur);
	ClassDB::bind_method(D_METHOD("get_road_blur"), &TerrainSplineDeformer::get_road_blur);
	ADD_PROPERTY(
			PropertyInfo(Variant::FLOAT, "road_blur", PROPERTY_HINT_RANGE, "0,200,0.5,suffix:m"), "set_road_blur",
			"get_road_blur"
	);

	ClassDB::bind_method(D_METHOD("set_falloff_curve", "falloff_curve"), &TerrainSplineDeformer::set_falloff_curve);
	ClassDB::bind_method(D_METHOD("get_falloff_curve"), &TerrainSplineDeformer::get_falloff_curve);
	ADD_PROPERTY(
			PropertyInfo(Variant::OBJECT, "falloff_curve", PROPERTY_HINT_RESOURCE_TYPE, "Curve"), "set_falloff_curve",
			"get_falloff_curve"
	);

	ClassDB::bind_method(D_METHOD("set_inner_falloff_curve", "curve"), &TerrainSplineDeformer::set_inner_falloff_curve);
	ClassDB::bind_method(D_METHOD("get_inner_falloff_curve"), &TerrainSplineDeformer::get_inner_falloff_curve);
	ADD_PROPERTY(
			PropertyInfo(Variant::OBJECT, "inner_falloff_curve", PROPERTY_HINT_RESOURCE_TYPE, "Curve"),
			"set_inner_falloff_curve", "get_inner_falloff_curve"
	);

	ClassDB::bind_method(D_METHOD("set_fill_interior", "fill"), &TerrainSplineDeformer::set_fill_interior);
	ClassDB::bind_method(D_METHOD("get_fill_interior"), &TerrainSplineDeformer::get_fill_interior);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "fill_interior"), "set_fill_interior", "get_fill_interior");

	ClassDB::bind_method(D_METHOD("set_use_tile_culling", "use"), &TerrainSplineDeformer::set_use_tile_culling);
	ClassDB::bind_method(D_METHOD("get_use_tile_culling"), &TerrainSplineDeformer::get_use_tile_culling);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "use_tile_culling"), "set_use_tile_culling", "get_use_tile_culling");

	ClassDB::bind_method(D_METHOD("set_use_distance_field", "use"), &TerrainSplineDeformer::set_use_distance_field);
	ClassDB::bind_method(D_METHOD("get_use_distance_field"), &TerrainSplineDeformer::get_use_distance_field);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "use_distance_field"), "set_use_distance_field", "get_use_distance_field");

	ClassDB::bind_method(D_METHOD("set_tile_size", "size"), &TerrainSplineDeformer::set_tile_size);
	ClassDB::bind_method(D_METHOD("get_tile_size"), &TerrainSplineDeformer::get_tile_size);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "tile_size"), "set_tile_size", "get_tile_size");

	ClassDB::bind_method(
			D_METHOD("deform_heightmap", "heightmap", "spline", "offset"), &TerrainSplineDeformer::deform_heightmap
	);
	ClassDB::bind_method(
			D_METHOD("_deform_heightmap_task", "task_idx", "job"), &TerrainSplineDeformer::_deform_heightmap_task
	);
	ClassDB::bind_method(D_METHOD("_on_curve_changed"), &TerrainSplineDeformer::_on_curve_changed);

	BIND_ENUM_CONSTANT(BLEND_ADD);
	BIND_ENUM_CONSTANT(BLEND_SUBTRACT);
	BIND_ENUM_CONSTANT(BLEND_MAX);
	BIND_ENUM_CONSTANT(BLEND_MIN);
	BIND_ENUM_CONSTANT(BLEND_REPLACE);
	BIND_ENUM_CONSTANT(HEIGHT_SPLINE);
	BIND_ENUM_CONSTANT(HEIGHT_TERRAIN);
	BIND_ENUM_CONSTANT(EARTHWORK_CUT_AND_FILL);
	BIND_ENUM_CONSTANT(EARTHWORK_CUT_ONLY);
	BIND_ENUM_CONSTANT(EARTHWORK_FILL_ONLY);
}

TerrainSplineDeformer::TerrainSplineDeformer() {}

TerrainSplineDeformer::~TerrainSplineDeformer() {
	if (falloff_curve.is_valid() && falloff_curve->is_connected("changed", Callable(this, "_on_curve_changed"))) {
		falloff_curve->disconnect("changed", Callable(this, "_on_curve_changed"));
	}
	if (inner_falloff_curve.is_valid() &&
		inner_falloff_curve->is_connected("changed", Callable(this, "_on_curve_changed"))) {
		inner_falloff_curve->disconnect("changed", Callable(this, "_on_curve_changed"));
	}
}

// ---------------------------------------------------------------------------------------------
// Curves and dirty propagation
// ---------------------------------------------------------------------------------------------

void TerrainSplineDeformer::set_falloff_curve(const Ref<Curve> &p_curve) {
	if (falloff_curve.is_valid() && falloff_curve->is_connected("changed", Callable(this, "_on_curve_changed"))) {
		falloff_curve->disconnect("changed", Callable(this, "_on_curve_changed"));
	}
	falloff_curve = p_curve;
	if (falloff_curve.is_valid()) {
		falloff_curve->connect("changed", Callable(this, "_on_curve_changed"));
	}
	mark_dirty();
}

void TerrainSplineDeformer::set_inner_falloff_curve(const Ref<Curve> &p_curve) {
	if (inner_falloff_curve.is_valid() &&
		inner_falloff_curve->is_connected("changed", Callable(this, "_on_curve_changed"))) {
		inner_falloff_curve->disconnect("changed", Callable(this, "_on_curve_changed"));
	}
	inner_falloff_curve = p_curve;
	if (inner_falloff_curve.is_valid()) {
		inner_falloff_curve->connect("changed", Callable(this, "_on_curve_changed"));
	}
	mark_dirty();
}

void TerrainSplineDeformer::mark_dirty() {
	ProceduralSpline3D *spline = Object::cast_to<ProceduralSpline3D>(get_parent());
	if (spline) {
		spline->mark_dirty();
	}
}

void TerrainSplineDeformer::_on_curve_changed() { mark_dirty(); }

// ---------------------------------------------------------------------------------------------
// Entry points
// ---------------------------------------------------------------------------------------------

void TerrainSplineDeformer::deform_heightmap(
		const Ref<TerrainHeightmap> &p_heightmap,
		ProceduralSpline3D *p_spline,
		const Vector2 &p_offset
) {
	if (p_heightmap.is_null() || p_spline == nullptr) {
		return;
	}
	p_spline->ensure_baked_cache();
	deform_heightmap_prepared(p_heightmap, p_spline, p_offset, p_spline->get_padded_aabb(), true);
}

void TerrainSplineDeformer::deform_heightmap_prepared(
		const Ref<TerrainHeightmap> &p_heightmap,
		ProceduralSpline3D *p_spline,
		const Vector2 &p_offset,
		const Rect2 &p_padded_aabb,
		bool p_threaded
) {
	uint64_t step1_start = Time::get_singleton()->get_ticks_usec();
	if (p_heightmap.is_null() || p_spline == nullptr) {
		return;
	}
	int w = p_heightmap->get_width();
	int h = p_heightmap->get_height();
	Rect2 chunk_rect(p_offset, Vector2(w, h));
	if (!p_padded_aabb.intersects(chunk_rect)) {
		return;
	}

	Ref<DeformerJob> job = _create_deformer_job(p_heightmap, p_spline, p_offset);
#if DEBUG
	if (p_threaded) { // get_name()/get_curve() are Node/Resource reads; keep them on the main thread.
		_print_deformer_debug_info(p_spline, p_padded_aabb);
	}
#endif
	uint64_t step2_culling = Time::get_singleton()->get_ticks_usec();

	if (!(use_distance_field && _compute_distance_field(job, p_padded_aabb, w, h))) {
		_compute_active_tiles_and_culling(job, p_padded_aabb, w, h);
	}
	uint64_t step3_threads = Time::get_singleton()->get_ticks_usec();

	_dispatch_deformer_job(job, p_threaded);
#if DEBUG
	if (p_threaded) {
		uint64_t step4_end = Time::get_singleton()->get_ticks_usec();
		UtilityFunctions::print(
				"    [TerrainSplineDeformer: ", get_name(), "] Precompute: ", (step2_culling - step1_start) / 1000.0,
				"ms | Culling: ", (step3_threads - step2_culling) / 1000.0,
				"ms | Math Threads: ", (step4_end - step3_threads) / 1000.0, "ms"
		);
	}
#endif
}

/// Runs one _deform_heightmap_task per active tile: as a WorkerThreadPool group when p_threaded,
/// else serially on the calling thread (a worker running a whole chunk job).
void TerrainSplineDeformer::_dispatch_deformer_job(
		Ref<DeformerJob> p_job,
		bool p_threaded
) {
	int num_tasks = p_job->active_tiles.size();
	if (num_tasks <= 0) {
		return;
	}
	WorkerThreadPool *wtp = p_threaded ? WorkerThreadPool::get_singleton() : nullptr;
	if (wtp) {
		Callable task_callable = Callable(this, "_deform_heightmap_task").bind(p_job);
		int group_id = wtp->add_group_task(task_callable, num_tasks, -1, true, "TerraSpline_Unified_Deform");
		wtp->wait_for_group_task_completion(group_id);
	} else {
		for (int r = 0; r < num_tasks; ++r) {
			_deform_heightmap_task(r, p_job);
		}
	}
}

/// Group-task entry: deforms one active tile, via the distance field when available.
void TerrainSplineDeformer::_deform_heightmap_task(
		int p_task_idx,
		Ref<DeformerJob> p_job
) {
	if (p_job.is_null() || p_job->heightmap.is_null() || p_job->data_ptr == nullptr || p_job->spline == nullptr) {
		return;
	}
	const Rect2i tile = p_job->active_tiles[p_task_idx];
	const int w = p_job->heightmap->get_width();
	if (p_job->field_valid) {
		_deform_tile_field(p_job, tile, w);
	} else {
		_deform_tile_fallback(p_job, tile, p_task_idx, w);
	}
}

} // namespace godot
