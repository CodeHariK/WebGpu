/*
 * Module Path: src/terrain/terraspline/tr_deformer.cpp
 * System Responsibility: Implements spline-based heightmap deformation algorithms and threads.
 * Build Dependencies: terraspline.h, godot_cpp
 */

#include "terraspline.h"
#include <godot_cpp/classes/time.hpp>
#include <godot_cpp/classes/worker_thread_pool.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

/*
 * Purpose: Linearly interpolates between two float values.
 * Execution steps: Computes direct interpolation value.
 * Parameters:
 *   - a: Start value.
 *   - b: End value.
 *   - t: Blend weight fraction.
 * Behavioral bounds: Returns interpolated float.
 */
static inline float local_lerp(float a, float b, float t) {
	return a + t * (b - a);
}

void TerrainSplineDeformer::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_max_height", "max_height"), &TerrainSplineDeformer::set_max_height);
	ClassDB::bind_method(D_METHOD("get_max_height"), &TerrainSplineDeformer::get_max_height);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "max_height"), "set_max_height", "get_max_height");

	ClassDB::bind_method(D_METHOD("set_spline_width", "spline_width"), &TerrainSplineDeformer::set_spline_width);
	ClassDB::bind_method(D_METHOD("get_spline_width"), &TerrainSplineDeformer::get_spline_width);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "spline_width"), "set_spline_width", "get_spline_width");

	ClassDB::bind_method(D_METHOD("set_falloff_distance", "falloff_distance"), &TerrainSplineDeformer::set_falloff_distance);
	ClassDB::bind_method(D_METHOD("get_falloff_distance"), &TerrainSplineDeformer::get_falloff_distance);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "falloff_distance"), "set_falloff_distance", "get_falloff_distance");

	ClassDB::bind_method(D_METHOD("set_inner_falloff_distance", "dist"), &TerrainSplineDeformer::set_inner_falloff_distance);
	ClassDB::bind_method(D_METHOD("get_inner_falloff_distance"), &TerrainSplineDeformer::get_inner_falloff_distance);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "inner_falloff_distance"), "set_inner_falloff_distance", "get_inner_falloff_distance");

	ClassDB::bind_method(D_METHOD("set_blend_mode", "blend_mode"), &TerrainSplineDeformer::set_blend_mode);
	ClassDB::bind_method(D_METHOD("get_blend_mode"), &TerrainSplineDeformer::get_blend_mode);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "blend_mode", PROPERTY_HINT_ENUM, "Add,Subtract,Max,Min,Replace"), "set_blend_mode", "get_blend_mode");

	ClassDB::bind_method(D_METHOD("set_falloff_curve", "falloff_curve"), &TerrainSplineDeformer::set_falloff_curve);
	ClassDB::bind_method(D_METHOD("get_falloff_curve"), &TerrainSplineDeformer::get_falloff_curve);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "falloff_curve", PROPERTY_HINT_RESOURCE_TYPE, "Curve"), "set_falloff_curve", "get_falloff_curve");

	ClassDB::bind_method(D_METHOD("set_inner_falloff_curve", "curve"), &TerrainSplineDeformer::set_inner_falloff_curve);
	ClassDB::bind_method(D_METHOD("get_inner_falloff_curve"), &TerrainSplineDeformer::get_inner_falloff_curve);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "inner_falloff_curve", PROPERTY_HINT_RESOURCE_TYPE, "Curve"), "set_inner_falloff_curve", "get_inner_falloff_curve");

	ClassDB::bind_method(D_METHOD("set_fill_interior", "fill"), &TerrainSplineDeformer::set_fill_interior);
	ClassDB::bind_method(D_METHOD("get_fill_interior"), &TerrainSplineDeformer::get_fill_interior);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "fill_interior"), "set_fill_interior", "get_fill_interior");

	ClassDB::bind_method(D_METHOD("set_use_tile_culling", "use"), &TerrainSplineDeformer::set_use_tile_culling);
	ClassDB::bind_method(D_METHOD("get_use_tile_culling"), &TerrainSplineDeformer::get_use_tile_culling);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "use_tile_culling"), "set_use_tile_culling", "get_use_tile_culling");

	ClassDB::bind_method(D_METHOD("set_tile_size", "size"), &TerrainSplineDeformer::set_tile_size);
	ClassDB::bind_method(D_METHOD("get_tile_size"), &TerrainSplineDeformer::get_tile_size);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "tile_size"), "set_tile_size", "get_tile_size");

	ClassDB::bind_method(D_METHOD("deform_heightmap", "heightmap", "spline", "offset"), &TerrainSplineDeformer::deform_heightmap);
	ClassDB::bind_method(D_METHOD("_deform_heightmap_task", "task_idx", "job"), &TerrainSplineDeformer::_deform_heightmap_task);
	ClassDB::bind_method(D_METHOD("_on_curve_changed"), &TerrainSplineDeformer::_on_curve_changed);

	BIND_ENUM_CONSTANT(BLEND_ADD);
	BIND_ENUM_CONSTANT(BLEND_SUBTRACT);
	BIND_ENUM_CONSTANT(BLEND_MAX);
	BIND_ENUM_CONSTANT(BLEND_MIN);
	BIND_ENUM_CONSTANT(BLEND_REPLACE);
}

TerrainSplineDeformer::TerrainSplineDeformer() {
	max_height = 0.0f;
	spline_width = 2.0f;
	falloff_distance = 5.0f;
	inner_falloff_distance = 5.0f;
	fill_interior = true;
	blend_mode = BLEND_ADD;
	use_tile_culling = true;
	tile_size = 32;
}

TerrainSplineDeformer::~TerrainSplineDeformer() {
	if (falloff_curve.is_valid() && falloff_curve->is_connected("changed", Callable(this, "_on_curve_changed"))) {
		falloff_curve->disconnect("changed", Callable(this, "_on_curve_changed"));
	}
	if (inner_falloff_curve.is_valid() && inner_falloff_curve->is_connected("changed", Callable(this, "_on_curve_changed"))) {
		inner_falloff_curve->disconnect("changed", Callable(this, "_on_curve_changed"));
	}
}

void TerrainSplineDeformer::mark_dirty() {
	ProceduralSpline3D *spline = Object::cast_to<ProceduralSpline3D>(get_parent());
	if (spline) {
		spline->mark_dirty();
	}
}

void TerrainSplineDeformer::_on_curve_changed() {
	mark_dirty();
}

void TerrainSplineDeformer::_deform_heightmap_task(int p_task_idx, Ref<DeformerJob> p_job) {
	if (p_job.is_null() || p_job->heightmap.is_null() || p_job->data_ptr == nullptr || p_job->spline == nullptr)
		return;

	Rect2i tile = p_job->active_tiles[p_task_idx];
	int w = p_job->heightmap->get_width();

	for (int z = tile.position.y; z <= tile.position.y + tile.size.y - 1; ++z) {
		for (int x = tile.position.x; x <= tile.position.x + tile.size.x - 1; ++x) {
			Vector2 p((float)x + p_job->offset.x, (float)z + p_job->offset.y);
			ProceduralSpline3D::SplineEval eval = p_job->spline->evaluate_spline_point_segmented(p, p_job->tile_segments[p_task_idx]);

			float target_spline_h = eval.spline_y + max_height;
			float weight = 0.0f;

			if (eval.distance <= spline_width) {
				// 1. We are directly on the main road/rim!
				weight = 1.0f;
			} else if (eval.is_inside) {
				// 2. We are INSIDE the closed loop (The Crater/Inner Hole or Solid Plateau)
				if (p_job->fill_interior) {
					// Default: fill the interior completely
					weight = 1.0f;
				} else {
					if (eval.distance < (spline_width + inner_falloff_distance) && inner_falloff_distance > 0.0001f) {
						float t = (eval.distance - spline_width) / inner_falloff_distance;
						if (p_job->has_inner_curve) {
							int c_idx = Math::clamp((int)((1.0f - t) * 255.0f), 0, 255);
							weight = p_job->baked_inner_curve[c_idx];
						} else {
							weight = 1.0f - t;
						}
					}
				}
			} else {
				// 3. We are OUTSIDE the loop (The Outer Slopes)
				if (eval.distance < (spline_width + falloff_distance) && falloff_distance > 0.0001f) {
					float t = (eval.distance - spline_width) / falloff_distance;
					if (p_job->has_curve) {
						int c_idx = Math::clamp((int)((1.0f - t) * 255.0f), 0, 255);
						weight = p_job->baked_curve[c_idx];
					} else {
						weight = 1.0f - t;
					}
				}
			}

			if (weight <= 0.0f)
				continue;

			int idx = z * w + x;
			float current_h = p_job->data_ptr[idx];
			float new_h = current_h;

			switch (blend_mode) {
				case BLEND_ADD:
					new_h = current_h + (target_spline_h * weight);
					break;
				case BLEND_SUBTRACT:
					new_h = current_h - (target_spline_h * weight);
					break;
				case BLEND_MAX:
					new_h = Math::max(current_h, (float)local_lerp(current_h, target_spline_h, weight));
					break;
				case BLEND_MIN:
					new_h = Math::min(current_h, (float)local_lerp(current_h, target_spline_h, weight));
					break;
				case BLEND_REPLACE:
					new_h = local_lerp(current_h, target_spline_h, weight);
					break;
			}
			p_job->data_ptr[idx] = new_h;
		}
	}
}

Ref<DeformerJob> TerrainSplineDeformer::_create_deformer_job(const Ref<TerrainHeightmap> &p_heightmap, ProceduralSpline3D *p_spline, const Vector2 &p_offset) {
	Ref<DeformerJob> job;
	job.instantiate();
	job->heightmap = p_heightmap;
	job->spline = p_spline;
	job->deformer = this;
	job->offset = p_offset;
	job->data_ptr = p_heightmap->get_data_ptrw();

	job->has_curve = falloff_curve.is_valid();
	if (job->has_curve) {
		job->baked_curve.resize(256);
		for (int i = 0; i < 256; ++i) {
			job->baked_curve[i] = falloff_curve->sample((float)i / 255.0f);
		}
	}

	job->has_inner_curve = inner_falloff_curve.is_valid();
	if (job->has_inner_curve) {
		job->baked_inner_curve.resize(256);
		for (int i = 0; i < 256; ++i) {
			job->baked_inner_curve[i] = inner_falloff_curve->sample((float)i / 255.0f);
		}
	}
	job->fill_interior = fill_interior;
	return job;
}

void TerrainSplineDeformer::_compute_active_tiles_and_culling(Ref<DeformerJob> p_job, const Rect2 &p_aabb, int p_w, int p_h) {
	int thread_min_x = Math::max(0, (int)Math::floor(p_aabb.position.x - p_job->offset.x));
	int thread_max_x = Math::min(p_w - 1, (int)Math::ceil(p_aabb.position.x + p_aabb.size.x - p_job->offset.x));
	int thread_min_z = Math::max(0, (int)Math::floor(p_aabb.position.y - p_job->offset.y));
	int thread_max_z = Math::min(p_h - 1, (int)Math::ceil(p_aabb.position.y + p_aabb.size.y - p_job->offset.y));

	float search_radius = spline_width + falloff_distance;

	if (use_tile_culling && tile_size > 0) {
		float tile_radius = (tile_size * 1.41421356f) / 2.0f;
		float combined_radius = search_radius + tile_radius;
		float combined_radius_sq = combined_radius * combined_radius;

		for (int tz = thread_min_z; tz <= thread_max_z; tz += tile_size) {
			for (int tx = thread_min_x; tx <= thread_max_x; tx += tile_size) {
				int t_max_x = Math::min(tx + tile_size - 1, thread_max_x);
				int t_max_z = Math::min(tz + tile_size - 1, thread_max_z);

				Vector2 center((tx + t_max_x) / 2.0f + p_job->offset.x, (tz + t_max_z) / 2.0f + p_job->offset.y);

				std::vector<int> overlapping_segments;
				for (size_t seg_idx = 0; seg_idx < p_job->spline->baked_segments.size(); ++seg_idx) {
					const ProceduralSpline3D::BakedSegment &seg = p_job->spline->baked_segments[seg_idx];
					float t = (seg.l2 > 0.0f) ? Math::clamp((center - seg.a).dot(seg.ab) / seg.l2, 0.0f, 1.0f) : 0.0f;
					Vector2 proj = seg.a + t * seg.ab;

					float d_sq = center.distance_squared_to(proj);
					if (d_sq <= combined_radius_sq) {
						overlapping_segments.push_back((int)seg_idx);
					}
				}

				// --- NEW FIX: Bypassing the culler for interior tiles ---
				bool is_inside = false;
				if (p_job->spline->get_is_closed()) {
					// Pass an empty segment array to safely check interior status without triggering math loops
					ProceduralSpline3D::SplineEval center_eval = p_job->spline->evaluate_spline_point_segmented(center, std::vector<int>());
					is_inside = center_eval.is_inside;
				}

				if (!overlapping_segments.empty() || is_inside) {
					p_job->active_tiles.push_back(Rect2i(tx, tz, t_max_x - tx + 1, t_max_z - tz + 1));

					if (is_inside) {
						// Deep interior tiles MUST calculate against the entire boundary for accurate Ridge SDF and IDW
						std::vector<int> all_segs(p_job->spline->baked_segments.size());
						for (size_t i = 0; i < all_segs.size(); ++i) {
							all_segs[i] = (int)i;
						}
						p_job->tile_segments.push_back(all_segs);
					} else {
						p_job->tile_segments.push_back(overlapping_segments);
					}
				}
			}
		}
	} else {
		std::vector<int> all_segments(p_job->spline->baked_segments.size());
		for (size_t i = 0; i < all_segments.size(); ++i) {
			all_segments[i] = (int)i;
		}
		for (int tz = thread_min_z; tz <= thread_max_z; ++tz) {
			p_job->active_tiles.push_back(Rect2i(thread_min_x, tz, thread_max_x - thread_min_x + 1, 1));
			p_job->tile_segments.push_back(all_segments);
		}
	}
}

void TerrainSplineDeformer::_dispatch_deformer_job(Ref<DeformerJob> p_job) {
	int num_tasks = p_job->active_tiles.size();
	if (num_tasks <= 0) {
		return;
	}

	WorkerThreadPool *wtp = WorkerThreadPool::get_singleton();
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

void TerrainSplineDeformer::_print_deformer_debug_info(ProceduralSpline3D *p_spline, const Rect2 &p_aabb) {
	Ref<Curve3D> c = p_spline->get_curve();
	int ctrl_pts = c.is_valid() ? c->get_point_count() : 0;
	float baked_len = c.is_valid() ? c->get_baked_length() : 0.0f;
	UtilityFunctions::print("    [TerrainSplineDeformer: ", get_name(), "] Control Points: ", ctrl_pts,
							" | Baked Points: ", (int)p_spline->baked_poly3d.size(),
							" | Baked Length: ", baked_len, "m",
							" | Bounding Box Size: ", p_aabb.size);
}

void TerrainSplineDeformer::deform_heightmap(const Ref<TerrainHeightmap> &p_heightmap, ProceduralSpline3D *p_spline, const Vector2 &p_offset) {
	uint64_t step1_start = Time::get_singleton()->get_ticks_usec();

	if (p_heightmap.is_null() || p_spline == nullptr) {
		return;
	}

	Rect2 aabb = p_spline->get_padded_aabb();
	int w = p_heightmap->get_width();
	int h = p_heightmap->get_height();
	Rect2 chunk_rect(p_offset, Vector2(w, h));

	if (!aabb.intersects(chunk_rect)) {
		return;
	}

	p_spline->ensure_baked_cache();

	Ref<DeformerJob> job = _create_deformer_job(p_heightmap, p_spline, p_offset);

#if DEBUG
	_print_deformer_debug_info(p_spline, aabb);
#endif

	uint64_t step2_culling = Time::get_singleton()->get_ticks_usec();

	_compute_active_tiles_and_culling(job, aabb, w, h);

	uint64_t step3_threads = Time::get_singleton()->get_ticks_usec();

	_dispatch_deformer_job(job);

#if DEBUG
	uint64_t step4_end = Time::get_singleton()->get_ticks_usec();
	UtilityFunctions::print("    [TerrainSplineDeformer: ", get_name(), "] Precompute: ", (step2_culling - step1_start) / 1000.0, "ms | Culling: ", (step3_threads - step2_culling) / 1000.0, "ms | Math Threads: ", (step4_end - step3_threads) / 1000.0, "ms");
#endif
}

} //namespace godot
