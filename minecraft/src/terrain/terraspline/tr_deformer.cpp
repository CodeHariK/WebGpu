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
#include <vector>
#include <algorithm>

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
	ADD_PROPERTY(PropertyInfo(Variant::INT, "blend_mode", PROPERTY_HINT_ENUM, "Add (terrain + (spline Y + max_height) * weight),Subtract (terrain - (spline Y + max_height) * weight),Max (raise to spline Y + max_height),Min (lower to spline Y + max_height),Replace (blend to spline Y + max_height)"), "set_blend_mode", "get_blend_mode");

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

	ClassDB::bind_method(D_METHOD("set_use_distance_field", "use"), &TerrainSplineDeformer::set_use_distance_field);
	ClassDB::bind_method(D_METHOD("get_use_distance_field"), &TerrainSplineDeformer::get_use_distance_field);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "use_distance_field"), "set_use_distance_field", "get_use_distance_field");
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

// Falloff weight for a pixel at `distance` from the spline. Shared by both per-pixel paths.
static inline float _falloff_weight(const Ref<DeformerJob> &p_job, float distance, bool is_inside,
		float spline_width, float falloff_distance, float inner_falloff_distance) {
	if (distance <= spline_width) {
		return 1.0f; // On the core footprint.
	}
	if (is_inside) {
		if (p_job->fill_interior) {
			return 1.0f;
		}
		if (distance < (spline_width + inner_falloff_distance) && inner_falloff_distance > 0.0001f) {
			float t = (distance - spline_width) / inner_falloff_distance;
			if (p_job->has_inner_curve) {
				int c_idx = Math::clamp((int)((1.0f - t) * 255.0f), 0, 255);
				return p_job->baked_inner_curve[c_idx];
			}
			return 1.0f - t;
		}
		return 0.0f;
	}
	if (distance < (spline_width + falloff_distance) && falloff_distance > 0.0001f) {
		float t = (distance - spline_width) / falloff_distance;
		if (p_job->has_curve) {
			int c_idx = Math::clamp((int)((1.0f - t) * 255.0f), 0, 255);
			return p_job->baked_curve[c_idx];
		}
		return 1.0f - t;
	}
	return 0.0f;
}

static inline void _blend_pixel(float *data, int idx, int blend_mode, float weight, float target_spline_h) {
	float current_h = data[idx];
	float new_h = current_h;
	switch (blend_mode) {
		case TerrainSplineDeformer::BLEND_ADD:
			new_h = current_h + (target_spline_h * weight);
			break;
		case TerrainSplineDeformer::BLEND_SUBTRACT:
			new_h = current_h - (target_spline_h * weight);
			break;
		case TerrainSplineDeformer::BLEND_MAX:
			new_h = Math::max(current_h, (float)local_lerp(current_h, target_spline_h, weight));
			break;
		case TerrainSplineDeformer::BLEND_MIN:
			new_h = Math::min(current_h, (float)local_lerp(current_h, target_spline_h, weight));
			break;
		case TerrainSplineDeformer::BLEND_REPLACE:
			new_h = local_lerp(current_h, target_spline_h, weight);
			break;
	}
	data[idx] = new_h;
}

// spline_y from the distance field, matching ProceduralSpline3D::evaluate_spline_point_segmented for
// the same mode. Only called where the falloff weight is > 0.
static inline float _field_spline_y(const Ref<DeformerJob> &p_job, int seg, float px, float pz, float nx, float nz, float distance, bool is_inside) {
	// Height at the nearest point on the nearest segment.
	float closest_y = 0.0f;
	if (seg >= 0) {
		const float l2 = p_job->seg_l2[seg];
		float t = 0.0f;
		if (l2 > 0.0f) {
			t = Math::clamp(((px - p_job->seg_ax[seg]) * p_job->seg_abx[seg] + (pz - p_job->seg_az[seg]) * p_job->seg_abz[seg]) / l2, 0.0f, 1.0f);
		}
		closest_y = p_job->seg_y0[seg] + t * p_job->seg_dy[seg];
	}
	switch (p_job->interpolation_mode) {
		case ProceduralSpline3D::INTERP_IDW_LINE: {
			// Inverse-distance weighting over all segments (the tile-culled original used only
			// nearby segments; far ones contribute ~1/d^2 so the difference is negligible).
			const int n = (int)p_job->seg_ax.size();
			const float *ax = p_job->seg_ax.data(), *az = p_job->seg_az.data();
			const float *abx = p_job->seg_abx.data(), *abz = p_job->seg_abz.data();
			const float *l2s = p_job->seg_l2.data(), *y0 = p_job->seg_y0.data(), *dy = p_job->seg_dy.data();
			float tw = 0.0f, by = 0.0f;
			for (int i = 0; i < n; ++i) {
				float t = 0.0f;
				if (l2s[i] > 0.0f) {
					t = ((px - ax[i]) * abx[i] + (pz - az[i]) * abz[i]) / l2s[i];
					t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
				}
				const float qx = ax[i] + abx[i] * t - px;
				const float qz = az[i] + abz[i] * t - pz;
				const float w = 1.0f / (qx * qx + qz * qz + 0.001f);
				by += (y0[i] + t * dy[i]) * w;
				tw += w;
			}
			return tw > 0.0f ? by / tw : closest_y;
		}
		case ProceduralSpline3D::INTERP_IDW_VERTEX: {
			const int n = (int)p_job->vert_x.size();
			const float *vx = p_job->vert_x.data(), *vz = p_job->vert_z.data(), *vy = p_job->vert_y.data();
			float tw = 0.0f, by = 0.0f;
			for (int i = 0; i < n; ++i) {
				const float qx = vx[i] - px, qz = vz[i] - pz;
				const float w = 1.0f / (qx * qx + qz * qz + 0.001f);
				by += vy[i] * w;
				tw += w;
			}
			return tw > 0.0f ? by / tw : closest_y;
		}
		case ProceduralSpline3D::INTERP_PEAK_RIDGE:
			return is_inside ? closest_y + distance * p_job->ridge_steepness : closest_y;
		case ProceduralSpline3D::INTERP_NEAREST:
		default:
			return closest_y;
	}
}

void TerrainSplineDeformer::_deform_pixel_fallback(Ref<DeformerJob> p_job, int p_x, int p_z, int p_task_idx, int p_w) {
	Vector2 p((float)p_x + p_job->offset.x, (float)p_z + p_job->offset.y);
	ProceduralSpline3D::SplineEval eval = p_job->spline->evaluate_spline_point_segmented(p, p_job->tile_segments[p_task_idx]);
	float weight = _falloff_weight(p_job, eval.distance, eval.is_inside, spline_width, falloff_distance, inner_falloff_distance);
	if (weight <= 0.0f) {
		return;
	}
	_blend_pixel(p_job->data_ptr, p_z * p_w + p_x, blend_mode, weight, eval.spline_y + max_height);
}

void TerrainSplineDeformer::_deform_heightmap_task(int p_task_idx, Ref<DeformerJob> p_job) {
	if (p_job.is_null() || p_job->heightmap.is_null() || p_job->data_ptr == nullptr || p_job->spline == nullptr)
		return;
	Rect2i tile = p_job->active_tiles[p_task_idx];
	int w = p_job->heightmap->get_width();

	if (!p_job->field_valid) {
		for (int z = tile.position.y; z <= tile.position.y + tile.size.y - 1; ++z) {
			for (int x = tile.position.x; x <= tile.position.x + tile.size.x - 1; ++x) {
				_deform_pixel_fallback(p_job, x, z, p_task_idx, w);
			}
		}
		return;
	}

	// Distance-field path: O(1) distance + inside per pixel; spline_y only where weight > 0.
	const int m = p_job->field_margin;
	const int gw = p_job->field_w;
	const int32_t *fseg = p_job->field_seg.data();
	const float *fnx = p_job->field_nx.data();
	const float *fnz = p_job->field_nz.data();
	const uint8_t *fin = p_job->field_inside.data();
	for (int z = tile.position.y; z <= tile.position.y + tile.size.y - 1; ++z) {
		const size_t row = (size_t)(z + m) * gw;
		const float pz = (float)z + p_job->offset.y;
		for (int x = tile.position.x; x <= tile.position.x + tile.size.x - 1; ++x) {
			const size_t fidx = row + (x + m);
			const float px = (float)x + p_job->offset.x;
			const int seg = fseg[fidx];
			const bool is_inside = fin[fidx] != 0;
			float distance = 1e20f;
			if (seg >= 0) {
				const float dx = fnx[fidx] - px, dz = fnz[fidx] - pz;
				distance = Math::sqrt(dx * dx + dz * dz);
			}
			const float weight = _falloff_weight(p_job, distance, is_inside, spline_width, falloff_distance, inner_falloff_distance);
			if (weight <= 0.0f) {
				continue;
			}
			const float spline_y = _field_spline_y(p_job, seg, px, pz, fnx[fidx], fnz[fidx], distance, is_inside);
			_blend_pixel(p_job->data_ptr, z * w + x, blend_mode, weight, spline_y + max_height);
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

	// SoA geometry for the distance-field path (tight loops, no Vector2 temporaries).
	job->interpolation_mode = (int)p_spline->get_interpolation_mode();
	job->ridge_steepness = p_spline->get_ridge_steepness();
	job->spline_closed = p_spline->get_is_closed();
	const size_t nseg = p_spline->baked_segments.size();
	job->seg_ax.resize(nseg);
	job->seg_az.resize(nseg);
	job->seg_abx.resize(nseg);
	job->seg_abz.resize(nseg);
	job->seg_l2.resize(nseg);
	job->seg_y0.resize(nseg);
	job->seg_dy.resize(nseg);
	for (size_t i = 0; i < nseg; ++i) {
		const ProceduralSpline3D::BakedSegment &sg = p_spline->baked_segments[i];
		job->seg_ax[i] = sg.a.x;
		job->seg_az[i] = sg.a.y;
		job->seg_abx[i] = sg.ab.x;
		job->seg_abz[i] = sg.ab.y;
		job->seg_l2[i] = sg.l2;
		job->seg_y0[i] = sg.y_start;
		job->seg_dy[i] = sg.y_diff;
	}
	const int nvert = p_spline->baked_poly3d.size();
	job->vert_x.resize(nvert);
	job->vert_z.resize(nvert);
	job->vert_y.resize(nvert);
	for (int i = 0; i < nvert; ++i) {
		Vector3 v = p_spline->baked_poly3d[i];
		job->vert_x[i] = v.x;
		job->vert_z[i] = v.z;
		job->vert_y[i] = v.y;
	}
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

				// Interior tiles of closed splines have no nearby segments but still need deforming.
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

/*
 * Step 4 (Terraspline.md): distance field.
 * 1. Rasterize every segment near the chunk into a padded grid as "seeds" carrying the exact sample
 *    point and segment index.
 * 2. Propagate nearest seeds with two 8-neighbour sweeps (8SSEDT).
 * 3. Refine: project each pixel onto the adopted segment and its two neighbours, keep the closest.
 *    Distances are then exact for the identified segment; identity errors are rare and bounded.
 * 4. Closed splines: even-odd scanline fill for inside/outside.
 * 5. Active tiles = tiles containing any pixel within the search radius or inside the polygon.
 * The whole pass is O(grid) ~ (chunk + 2*margin)^2, independent of segment count.
 */
bool TerrainSplineDeformer::_compute_distance_field(Ref<DeformerJob> p_job, const Rect2 &p_aabb, int p_w, int p_h) {
	const int nseg = (int)p_job->seg_ax.size();
	if (nseg == 0 || p_job->vert_x.size() < 2) {
		return false;
	}
	const float search_radius = spline_width + falloff_distance;
	const int m = (int)Math::ceil(search_radius) + 1;
	const int gw = p_w + 2 * m;
	const int gh = p_h + 2 * m;
	const float gx0 = p_job->offset.x - m; // world coords of grid pixel (0,0)
	const float gz0 = p_job->offset.y - m;
	const Rect2 grid_rect(Vector2(gx0, gz0), Vector2(gw, gh));

	p_job->field_w = gw;
	p_job->field_h = gh;
	p_job->field_margin = m;
	p_job->field_seg.assign((size_t)gw * gh, -1);
	p_job->field_nx.assign((size_t)gw * gh, 0.0f);
	p_job->field_nz.assign((size_t)gw * gh, 0.0f);
	p_job->field_inside.assign((size_t)gw * gh, 0);

	int32_t *fseg = p_job->field_seg.data();
	float *fnx = p_job->field_nx.data();
	float *fnz = p_job->field_nz.data();

	// 1. Seeds.
	for (int si = 0; si < nseg; ++si) {
		const float ax = p_job->seg_ax[si], az = p_job->seg_az[si];
		const float bx = ax + p_job->seg_abx[si], bz = az + p_job->seg_abz[si];
		Rect2 seg_rect(Vector2(Math::min(ax, bx), Math::min(az, bz)), Vector2(Math::abs(bx - ax), Math::abs(bz - az)));
		if (!seg_rect.intersects(grid_rect)) {
			continue;
		}
		const float len = Math::sqrt(p_job->seg_l2[si]);
		const int steps = Math::max(1, (int)Math::ceil(len * 2.0f)); // <= 0.5 px between samples
		for (int k = 0; k <= steps; ++k) {
			const float t = (float)k / (float)steps;
			const float px = ax + p_job->seg_abx[si] * t;
			const float pz = az + p_job->seg_abz[si] * t;
			const int i = (int)Math::round(px - gx0);
			const int j = (int)Math::round(pz - gz0);
			if (i < 0 || j < 0 || i >= gw || j >= gh) {
				continue;
			}
			const size_t idx = (size_t)j * gw + i;
			const float cx = gx0 + i, cz = gz0 + j;
			const float d_new = (px - cx) * (px - cx) + (pz - cz) * (pz - cz);
			if (fseg[idx] < 0) {
				fseg[idx] = si;
				fnx[idx] = px;
				fnz[idx] = pz;
			} else {
				const float d_old = (fnx[idx] - cx) * (fnx[idx] - cx) + (fnz[idx] - cz) * (fnz[idx] - cz);
				if (d_new < d_old) {
					fseg[idx] = si;
					fnx[idx] = px;
					fnz[idx] = pz;
				}
			}
		}
	}

	// 2. 8SSEDT propagation of nearest seed (two sweeps).
	auto consider = [&](size_t idx, float cx, float cz, size_t nidx) {
		const int32_t ns = fseg[nidx];
		if (ns < 0) {
			return;
		}
		const float dn = (fnx[nidx] - cx) * (fnx[nidx] - cx) + (fnz[nidx] - cz) * (fnz[nidx] - cz);
		if (fseg[idx] < 0) {
			fseg[idx] = ns;
			fnx[idx] = fnx[nidx];
			fnz[idx] = fnz[nidx];
			return;
		}
		const float dc = (fnx[idx] - cx) * (fnx[idx] - cx) + (fnz[idx] - cz) * (fnz[idx] - cz);
		if (dn < dc) {
			fseg[idx] = ns;
			fnx[idx] = fnx[nidx];
			fnz[idx] = fnz[nidx];
		}
	};
	for (int j = 0; j < gh; ++j) {
		for (int i = 0; i < gw; ++i) {
			const size_t idx = (size_t)j * gw + i;
			const float cx = gx0 + i, cz = gz0 + j;
			if (i > 0) consider(idx, cx, cz, idx - 1);
			if (j > 0) {
				consider(idx, cx, cz, idx - gw);
				if (i > 0) consider(idx, cx, cz, idx - gw - 1);
				if (i + 1 < gw) consider(idx, cx, cz, idx - gw + 1);
			}
		}
	}
	for (int j = gh - 1; j >= 0; --j) {
		for (int i = gw - 1; i >= 0; --i) {
			const size_t idx = (size_t)j * gw + i;
			const float cx = gx0 + i, cz = gz0 + j;
			if (i + 1 < gw) consider(idx, cx, cz, idx + 1);
			if (j + 1 < gh) {
				consider(idx, cx, cz, idx + gw);
				if (i + 1 < gw) consider(idx, cx, cz, idx + gw + 1);
				if (i > 0) consider(idx, cx, cz, idx + gw - 1);
			}
		}
	}

	// 3. Refine against the adopted segment and its neighbours (exact projection).
	auto project = [&](int si, float px, float pz, float &r_nx, float &r_nz) {
		const float l2 = p_job->seg_l2[si];
		float t = 0.0f;
		if (l2 > 0.0f) {
			t = ((px - p_job->seg_ax[si]) * p_job->seg_abx[si] + (pz - p_job->seg_az[si]) * p_job->seg_abz[si]) / l2;
			t = Math::clamp(t, 0.0f, 1.0f);
		}
		r_nx = p_job->seg_ax[si] + p_job->seg_abx[si] * t;
		r_nz = p_job->seg_az[si] + p_job->seg_abz[si] * t;
	};
	for (int j = 0; j < gh; ++j) {
		for (int i = 0; i < gw; ++i) {
			const size_t idx = (size_t)j * gw + i;
			const int32_t s0 = fseg[idx];
			if (s0 < 0) {
				continue;
			}
			const float cx = gx0 + i, cz = gz0 + j;
			int best = s0;
			float bnx, bnz;
			project(s0, cx, cz, bnx, bnz);
			float bd = (bnx - cx) * (bnx - cx) + (bnz - cz) * (bnz - cz);
			for (int dsi = -1; dsi <= 1; dsi += 2) {
				int si = s0 + dsi;
				if (p_job->spline_closed) {
					si = (si + nseg) % nseg;
				} else if (si < 0 || si >= nseg) {
					continue;
				}
				float nx, nz;
				project(si, cx, cz, nx, nz);
				const float d = (nx - cx) * (nx - cx) + (nz - cz) * (nz - cz);
				if (d < bd) {
					bd = d;
					best = si;
					bnx = nx;
					bnz = nz;
				}
			}
			fseg[idx] = best;
			fnx[idx] = bnx;
			fnz[idx] = bnz;
		}
	}

	// 3b. Exact nearest segment inside the falloff band. The two sweeps can lose the true nearest
	// segment's identity at polyline folds, and here distance drives the weight ramp, so every pixel
	// that can carry weight is resolved by brute force over bbox-culled segments (the same cull the
	// legacy path applied). The band is a thin strip along the spline, so this stays cheap; the far
	// interior keeps the O(1) field.
	{
		const float band = search_radius + 1.5f;
		const float band2 = band * band;
		std::vector<float> smin_x(nseg), smax_x(nseg), smin_z(nseg), smax_z(nseg);
		for (int si = 0; si < nseg; ++si) {
			const float ax = p_job->seg_ax[si], az = p_job->seg_az[si];
			const float bx = ax + p_job->seg_abx[si], bz = az + p_job->seg_abz[si];
			smin_x[si] = Math::min(ax, bx) - band;
			smax_x[si] = Math::max(ax, bx) + band;
			smin_z[si] = Math::min(az, bz) - band;
			smax_z[si] = Math::max(az, bz) + band;
		}
		for (int j = 0; j < gh; ++j) {
			const float cz = gz0 + j;
			for (int i = 0; i < gw; ++i) {
				const size_t idx = (size_t)j * gw + i;
				if (fseg[idx] < 0) {
					continue;
				}
				const float cx = gx0 + i;
				float bd = (fnx[idx] - cx) * (fnx[idx] - cx) + (fnz[idx] - cz) * (fnz[idx] - cz);
				if (bd > band2) {
					continue;
				}
				for (int si = 0; si < nseg; ++si) {
					if (cx < smin_x[si] || cx > smax_x[si] || cz < smin_z[si] || cz > smax_z[si]) {
						continue;
					}
					float nx, nz;
					project(si, cx, cz, nx, nz);
					const float d = (nx - cx) * (nx - cx) + (nz - cz) * (nz - cz);
					if (d < bd) {
						bd = d;
						fseg[idx] = si;
						fnx[idx] = nx;
						fnz[idx] = nz;
					}
				}
			}
		}
	}

	// 4. Even-odd scanline fill for closed splines.
	if (p_job->spline_closed) {
		std::vector<float> xs;
		uint8_t *fin = p_job->field_inside.data();
		const int nv = (int)p_job->vert_x.size();
		for (int j = 0; j < gh; ++j) {
			const float zc = gz0 + j;
			xs.clear();
			for (int v = 0; v < nv; ++v) {
				const int w2 = (v + 1) % nv;
				const float z1 = p_job->vert_z[v], z2 = p_job->vert_z[w2];
				if ((z1 <= zc) != (z2 <= zc)) {
					const float x1 = p_job->vert_x[v], x2 = p_job->vert_x[w2];
					xs.push_back(x1 + (zc - z1) / (z2 - z1) * (x2 - x1));
				}
			}
			if (xs.size() < 2) {
				continue;
			}
			std::sort(xs.begin(), xs.end());
			for (size_t k = 0; k + 1 < xs.size(); k += 2) {
				int i0 = Math::max(0, (int)Math::ceil(xs[k] - gx0));
				int i1 = Math::min(gw - 1, (int)Math::floor(xs[k + 1] - gx0));
				for (int i = i0; i <= i1; ++i) {
					fin[(size_t)j * gw + i] = 1;
				}
			}
		}
	}

	// 5. Active tiles: any pixel within reach or inside.
	const float r2 = search_radius * search_radius;
	const int thread_min_x = Math::max(0, (int)Math::floor(p_aabb.position.x - p_job->offset.x));
	const int thread_max_x = Math::min(p_w - 1, (int)Math::ceil(p_aabb.position.x + p_aabb.size.x - p_job->offset.x));
	const int thread_min_z = Math::max(0, (int)Math::floor(p_aabb.position.y - p_job->offset.y));
	const int thread_max_z = Math::min(p_h - 1, (int)Math::ceil(p_aabb.position.y + p_aabb.size.y - p_job->offset.y));
	const int ts = Math::max(8, tile_size);
	for (int tz = thread_min_z; tz <= thread_max_z; tz += ts) {
		for (int tx = thread_min_x; tx <= thread_max_x; tx += ts) {
			const int t_max_x = Math::min(tx + ts - 1, thread_max_x);
			const int t_max_z = Math::min(tz + ts - 1, thread_max_z);
			bool active = false;
			for (int z = tz; z <= t_max_z && !active; ++z) {
				const size_t row = (size_t)(z + m) * gw;
				for (int x = tx; x <= t_max_x; ++x) {
					const size_t idx = row + (x + m);
					if (p_job->field_inside[idx]) {
						active = true;
						break;
					}
					if (fseg[idx] >= 0) {
						const float px = p_job->offset.x + x, pz = p_job->offset.y + z;
						const float d2 = (fnx[idx] - px) * (fnx[idx] - px) + (fnz[idx] - pz) * (fnz[idx] - pz);
						if (d2 <= r2) {
							active = true;
							break;
						}
					}
				}
			}
			if (active) {
				p_job->active_tiles.push_back(Rect2i(tx, tz, t_max_x - tx + 1, t_max_z - tz + 1));
			}
		}
	}
	p_job->field_valid = true;
	return true;
}

void TerrainSplineDeformer::_dispatch_deformer_job(Ref<DeformerJob> p_job, bool p_threaded) {
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
	if (p_heightmap.is_null() || p_spline == nullptr) {
		return;
	}
	p_spline->ensure_baked_cache();
	deform_heightmap_prepared(p_heightmap, p_spline, p_offset, p_spline->get_padded_aabb(), true);
}

void TerrainSplineDeformer::deform_heightmap_prepared(const Ref<TerrainHeightmap> &p_heightmap, ProceduralSpline3D *p_spline, const Vector2 &p_offset, const Rect2 &p_padded_aabb, bool p_threaded) {
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
		UtilityFunctions::print("    [TerrainSplineDeformer: ", get_name(), "] Precompute: ", (step2_culling - step1_start) / 1000.0, "ms | Culling: ", (step3_threads - step2_culling) / 1000.0, "ms | Math Threads: ", (step4_end - step3_threads) / 1000.0, "ms");
	}
#endif
}

} //namespace godot
