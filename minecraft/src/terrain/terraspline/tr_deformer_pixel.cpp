/**
 * @file tr_deformer_pixel.cpp
 * @brief Per-pixel deformation: falloff weight, spline height, blend. Runs on worker threads.
 *
 * Two tile loops share the weight and blend helpers:
 *  - _deform_tile_field     reads distance / inside from the precomputed field (O(1) per pixel) and
 *                           evaluates the spline height only where the weight is > 0
 *  - _deform_tile_fallback  legacy path: full ProceduralSpline3D evaluation per pixel
 */
#include "tr_deformer.h"
#include <cmath>

namespace godot {

static inline float local_lerp(
		float a,
		float b,
		float t
) {
	return a + t * (b - a);
}

/**
 * @brief Falloff weight in [0, 1] for a pixel at `p_distance` from the spline.
 * 1 on the core footprint; ramps to 0 over inner_falloff_distance inside a closed loop (unless
 * fill_interior) or over falloff_distance outside, optionally shaped by the baked curves.
 */
float TerrainSplineDeformer::_falloff_weight(
		const Ref<DeformerJob> &p_job,
		float p_distance,
		bool p_is_inside
) const {
	if (p_distance <= spline_width) {
		return 1.0f;
	}
	if (p_is_inside) {
		if (p_job->fill_interior) {
			return 1.0f;
		}
		if (p_distance < (spline_width + inner_falloff_distance) && inner_falloff_distance > 0.0001f) {
			float t = (p_distance - spline_width) / inner_falloff_distance;
			if (p_job->has_inner_curve) {
				int c_idx = Math::clamp((int)((1.0f - t) * 255.0f), 0, 255);
				return p_job->baked_inner_curve[c_idx];
			}
			return 1.0f - t;
		}
		return 0.0f;
	}
	if (p_distance < (spline_width + falloff_distance) && falloff_distance > 0.0001f) {
		float t = (p_distance - spline_width) / falloff_distance;
		if (p_job->has_curve) {
			int c_idx = Math::clamp((int)((1.0f - t) * 255.0f), 0, 255);
			return p_job->baked_curve[c_idx];
		}
		return 1.0f - t;
	}
	return 0.0f;
}

/**
 * @brief Combines the target height into the terrain according to the blend mode.
 * `p_target_h` is the absolute height the spline asks for (spline Y + max_height). ADD/SUBTRACT
 * apply its offset from the chunk's base elevation, so on flat ground the surface meets the spline
 * exactly and any noise underneath is preserved; MAX/MIN/REPLACE move toward the absolute target.
 */
static inline void blend_pixel(
		float *p_data,
		int p_idx,
		int p_blend_mode,
		float p_weight,
		float p_target_h,
		float p_base_h
) {
	float current_h = p_data[p_idx];
	float new_h = current_h;
	switch (p_blend_mode) {
		case TerrainSplineDeformer::BLEND_ADD:
			new_h = current_h + ((p_target_h - p_base_h) * p_weight);
			break;
		case TerrainSplineDeformer::BLEND_SUBTRACT:
			new_h = current_h - ((p_target_h - p_base_h) * p_weight);
			break;
		case TerrainSplineDeformer::BLEND_MAX:
			new_h = Math::max(current_h, (float)local_lerp(current_h, p_target_h, p_weight));
			break;
		case TerrainSplineDeformer::BLEND_MIN:
			new_h = Math::min(current_h, (float)local_lerp(current_h, p_target_h, p_weight));
			break;
		case TerrainSplineDeformer::BLEND_REPLACE:
			new_h = local_lerp(current_h, p_target_h, p_weight);
			break;
	}
	p_data[p_idx] = new_h;
}

/// Height of segment `p_seg` at the projection of (px, pz) onto it.
static inline float segment_height_at(
		const Ref<DeformerJob> &p_job,
		int p_seg,
		float px,
		float pz
) {
	const float l2 = p_job->seg_l2[p_seg];
	float t = 0.0f;
	if (l2 > 0.0f) {
		t = Math::clamp(
				((px - p_job->seg_ax[p_seg]) * p_job->seg_abx[p_seg] +
				 (pz - p_job->seg_az[p_seg]) * p_job->seg_abz[p_seg]) /
						l2,
				0.0f, 1.0f
		);
	}
	return p_job->seg_y0[p_seg] + t * p_job->seg_dy[p_seg];
}

/// Inverse-distance weighting over all segments (INTERP_IDW_LINE).
static inline float idw_over_segments(
		const Ref<DeformerJob> &p_job,
		float px,
		float pz,
		float p_fallback
) {
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
	return tw > 0.0f ? by / tw : p_fallback;
}

/// Inverse-distance weighting over all baked vertices (INTERP_IDW_VERTEX).
static inline float idw_over_vertices(
		const Ref<DeformerJob> &p_job,
		float px,
		float pz,
		float p_fallback
) {
	const int n = (int)p_job->vert_x.size();
	const float *vx = p_job->vert_x.data(), *vz = p_job->vert_z.data(), *vy = p_job->vert_y.data();
	float tw = 0.0f, by = 0.0f;
	for (int i = 0; i < n; ++i) {
		const float qx = vx[i] - px, qz = vz[i] - pz;
		const float w = 1.0f / (qx * qx + qz * qz + 0.001f);
		by += vy[i] * w;
		tw += w;
	}
	return tw > 0.0f ? by / tw : p_fallback;
}

/**
 * @brief Spline height at a pixel, from the distance field, matching
 * ProceduralSpline3D::evaluate_spline_point_segmented for the same interpolation mode.
 * Only called where the falloff weight is > 0, which is what makes the field path fast.
 * (INTERP_IDW_LINE sums over all segments; the legacy path used a tile-culled subset - far
 * segments contribute ~1/d^2, so the difference is negligible.)
 */
static inline float field_spline_y(
		const Ref<DeformerJob> &p_job,
		int p_seg,
		float px,
		float pz,
		float p_distance,
		bool p_is_inside
) {
	const float closest_y = p_seg >= 0 ? segment_height_at(p_job, p_seg, px, pz) : 0.0f;
	switch (p_job->interpolation_mode) {
		case ProceduralSpline3D::INTERP_IDW_LINE:
			return idw_over_segments(p_job, px, pz, closest_y);
		case ProceduralSpline3D::INTERP_IDW_VERTEX:
			return idw_over_vertices(p_job, px, pz, closest_y);
		case ProceduralSpline3D::INTERP_PEAK_RIDGE:
			return p_is_inside ? closest_y + p_distance * p_job->ridge_steepness : closest_y;
		case ProceduralSpline3D::INTERP_NEAREST:
		default:
			return closest_y;
	}
}

// ---------------------------------------------------------------------------------------------
// Tile loops
// ---------------------------------------------------------------------------------------------

void TerrainSplineDeformer::_deform_tile_field(
		Ref<DeformerJob> p_job,
		const Rect2i &p_tile,
		int p_w
) {
	const int32_t *fseg = p_job->field_seg.data();
	const float *fnx = p_job->field_nx.data();
	const float *fnz = p_job->field_nz.data();
	const uint8_t *fin = p_job->field_inside.data();

	for (int z = p_tile.position.y; z <= p_tile.position.y + p_tile.size.y - 1; ++z) {
		const float pz = (float)z + p_job->offset.y;
		for (int x = p_tile.position.x; x <= p_tile.position.x + p_tile.size.x - 1; ++x) {
			const size_t fidx = p_job->field_index(x, z);
			const float px = (float)x + p_job->offset.x;
			const int seg = fseg[fidx];
			const bool is_inside = fin[fidx] != 0;
			float distance = 1e20f;
			if (seg >= 0) {
				const float dx = fnx[fidx] - px, dz = fnz[fidx] - pz;
				distance = Math::sqrt(dx * dx + dz * dz);
			}
			const float weight = _falloff_weight(p_job, distance, is_inside);
			if (weight <= 0.0f) {
				continue;
			}
			const float spline_y = field_spline_y(p_job, seg, px, pz, distance, is_inside);
			blend_pixel(p_job->data_ptr, z * p_w + x, blend_mode, weight, spline_y + max_height, p_job->base_elevation);
		}
	}
}

void TerrainSplineDeformer::_deform_tile_fallback(
		Ref<DeformerJob> p_job,
		const Rect2i &p_tile,
		int p_task_idx,
		int p_w
) {
	const std::vector<int> &segments = p_job->tile_segments[p_task_idx];
	for (int z = p_tile.position.y; z <= p_tile.position.y + p_tile.size.y - 1; ++z) {
		for (int x = p_tile.position.x; x <= p_tile.position.x + p_tile.size.x - 1; ++x) {
			Vector2 p((float)x + p_job->offset.x, (float)z + p_job->offset.y);
			ProceduralSpline3D::SplineEval eval = p_job->spline->evaluate_spline_point_segmented(p, segments);
			float weight = _falloff_weight(p_job, eval.distance, eval.is_inside);
			if (weight <= 0.0f) {
				continue;
			}
			blend_pixel(
					p_job->data_ptr, z * p_w + x, blend_mode, weight, eval.spline_y + max_height, p_job->base_elevation
			);
		}
	}
}

} // namespace godot
