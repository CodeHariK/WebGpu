/**
 * @file tr_deformer_weights.cpp
 * @brief TerrainSplineDeformer::compute_weight_field — the corridor footprint as a weight grid.
 *
 * Same distance field and falloff as the height blend (tr_deformer_field.cpp, _falloff_weight), but
 * without a heightmap, a profile bake or any height maths: a light DeformerJob is built, the field is
 * computed, and the weights of the active tiles are read off. Used by TerrainSplinePainter so painted
 * corridors match the earthwork exactly.
 */
#include "tr_deformer.h"
#include <cmath>

namespace godot {

bool TerrainSplineDeformer::compute_weight_field(
		ProceduralSpline3D *p_spline,
		const Vector2 &p_offset,
		int p_size,
		const Rect2 &p_padded_aabb,
		std::vector<float> &r_weights
) {
	if (p_spline == nullptr || p_size <= 0) {
		return false;
	}
	const Rect2 chunk_rect(p_offset, Vector2(p_size, p_size));
	if (!p_padded_aabb.intersects(chunk_rect)) {
		return false;
	}

	Ref<DeformerJob> job;
	job.instantiate();
	job->offset = p_offset;
	_fill_job_shape(job, p_spline);

	const bool has_field = use_distance_field && _compute_distance_field(job, p_padded_aabb, p_size, p_size);
	if (!has_field) {
		_compute_active_tiles_and_culling(job, p_padded_aabb, p_size, p_size);
	}
	if (job->active_tiles.empty()) {
		return false;
	}

	r_weights.assign((size_t)p_size * p_size, 0.0f);
	for (size_t t = 0; t < job->active_tiles.size(); ++t) {
		const Rect2i &tile = job->active_tiles[t];
		for (int z = tile.position.y; z <= tile.position.y + tile.size.y - 1; ++z) {
			const float pz = (float)z + p_offset.y;
			for (int x = tile.position.x; x <= tile.position.x + tile.size.x - 1; ++x) {
				const float px = (float)x + p_offset.x;
				float distance = 1e20f;
				bool is_inside = false;
				if (has_field) {
					const size_t fidx = job->field_index(x, z);
					const int seg = job->field_seg[fidx];
					is_inside = job->field_inside[fidx] != 0;
					if (seg >= 0) {
						const float dx = job->field_nx[fidx] - px, dz = job->field_nz[fidx] - pz;
						distance = Math::sqrt(dx * dx + dz * dz);
					}
				} else {
					ProceduralSpline3D::SplineEval eval =
							p_spline->evaluate_spline_point_segmented(Vector2(px, pz), job->tile_segments[t]);
					distance = eval.distance;
					is_inside = eval.is_inside;
				}
				r_weights[(size_t)z * p_size + x] = _falloff_weight(job, distance, is_inside);
			}
		}
	}
	return true;
}

} // namespace godot
