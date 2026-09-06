/**
 * @file tr_deformer_legacy.cpp
 * @brief DeformerJob creation, plus the legacy tile-culling work decomposition.
 *
 * The tile culler is the fallback when the distance field is disabled (`use_distance_field = false`,
 * kept for A/B comparison via `--terraspline-no-field`) or the spline is degenerate. It assigns each
 * tile the segments within reach of its centre; interior tiles of closed splines get every segment.
 */
#include "tr_deformer.h"
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

static constexpr int CURVE_SAMPLES = 256;

/// Samples a Curve into a 256-entry lookup table (index = 255 at the core, 0 at the falloff edge).
static void bake_curve(
		const Ref<Curve> &p_curve,
		std::vector<float> &r_table
) {
	r_table.resize(CURVE_SAMPLES);
	for (int i = 0; i < CURVE_SAMPLES; ++i) {
		r_table[i] = p_curve->sample((float)i / (float)(CURVE_SAMPLES - 1));
	}
}

/// Copies the spline's baked geometry into structure-of-arrays form for the inner loops.
static void copy_spline_geometry(
		const ProceduralSpline3D *p_spline,
		Ref<DeformerJob> &p_job
) {
	p_job->interpolation_mode = (int)p_spline->get_interpolation_mode();
	p_job->ridge_steepness = p_spline->get_ridge_steepness();
	p_job->spline_closed = p_spline->get_is_closed();

	const size_t nseg = p_spline->baked_segments.size();
	p_job->seg_ax.resize(nseg);
	p_job->seg_az.resize(nseg);
	p_job->seg_abx.resize(nseg);
	p_job->seg_abz.resize(nseg);
	p_job->seg_l2.resize(nseg);
	p_job->seg_y0.resize(nseg);
	p_job->seg_dy.resize(nseg);
	for (size_t i = 0; i < nseg; ++i) {
		const ProceduralSpline3D::BakedSegment &sg = p_spline->baked_segments[i];
		p_job->seg_ax[i] = sg.a.x;
		p_job->seg_az[i] = sg.a.y;
		p_job->seg_abx[i] = sg.ab.x;
		p_job->seg_abz[i] = sg.ab.y;
		p_job->seg_l2[i] = sg.l2;
		p_job->seg_y0[i] = sg.y_start;
		p_job->seg_dy[i] = sg.y_diff;
	}

	p_job->all_segments.resize(nseg);
	for (size_t i = 0; i < nseg; ++i) {
		p_job->all_segments[i] = (int)i;
	}

	const int nvert = p_spline->baked_poly3d.size();
	p_job->vert_x.resize(nvert);
	p_job->vert_z.resize(nvert);
	p_job->vert_y.resize(nvert);
	for (int i = 0; i < nvert; ++i) {
		Vector3 v = p_spline->baked_poly3d[i];
		p_job->vert_x[i] = v.x;
		p_job->vert_z[i] = v.z;
		p_job->vert_y[i] = v.y;
	}
}

/// Captures the corridor shape (falloff curves, interior fill) and the spline's baked geometry into a
/// job: everything the distance field and the falloff weight need, nothing about heights.
void TerrainSplineDeformer::_fill_job_shape(
		Ref<DeformerJob> &p_job,
		ProceduralSpline3D *p_spline
) const {
	p_job->spline = p_spline;
	p_job->deformer = const_cast<TerrainSplineDeformer *>(this);
	p_job->has_curve = falloff_curve.is_valid();
	if (p_job->has_curve) {
		bake_curve(falloff_curve, p_job->baked_curve);
	}
	p_job->has_inner_curve = inner_falloff_curve.is_valid();
	if (p_job->has_inner_curve) {
		bake_curve(inner_falloff_curve, p_job->baked_inner_curve);
	}
	p_job->fill_interior = fill_interior;
	copy_spline_geometry(p_spline, p_job);
}

/// Captures this deformer's settings and the spline's baked geometry into a job.
Ref<DeformerJob> TerrainSplineDeformer::_create_deformer_job(
		const Ref<TerrainHeightmap> &p_heightmap,
		ProceduralSpline3D *p_spline,
		const Vector2 &p_offset
) {
	Ref<DeformerJob> job;
	job.instantiate();
	job->heightmap = p_heightmap;
	job->spline = p_spline;
	job->deformer = this;
	job->offset = p_offset;
	job->data_ptr = p_heightmap->get_data_ptrw();
	job->base_elevation = p_heightmap->get_base_elevation();

	_fill_job_shape(job, p_spline);
	if (height_source == HEIGHT_TERRAIN) {
		_bake_terrain_profile(job, p_heightmap); // Replaces the spline's Y arrays with the ground profile
	}
	return job;
}

/**
 * @brief Legacy work decomposition: tiles of `tile_size` pixels over the spline's AABB, each with the
 * segments within (search radius + tile half-diagonal) of its centre. Tiles whose centre lies inside
 * a closed spline get every segment so interior ridge/IDW evaluation stays exact. With tile culling
 * off, every row is one tile with every segment.
 */
void TerrainSplineDeformer::_compute_active_tiles_and_culling(
		Ref<DeformerJob> p_job,
		const Rect2 &p_aabb,
		int p_w,
		int p_h
) {
	int thread_min_x = Math::max(0, (int)Math::floor(p_aabb.position.x - p_job->offset.x));
	int thread_max_x = Math::min(p_w - 1, (int)Math::ceil(p_aabb.position.x + p_aabb.size.x - p_job->offset.x));
	int thread_min_z = Math::max(0, (int)Math::floor(p_aabb.position.y - p_job->offset.y));
	int thread_max_z = Math::min(p_h - 1, (int)Math::ceil(p_aabb.position.y + p_aabb.size.y - p_job->offset.y));

	float search_radius = spline_width + falloff_distance;

	if (!(use_tile_culling && tile_size > 0)) {
		std::vector<int> all_segments(p_job->spline->baked_segments.size());
		for (size_t i = 0; i < all_segments.size(); ++i) {
			all_segments[i] = (int)i;
		}
		for (int tz = thread_min_z; tz <= thread_max_z; ++tz) {
			p_job->active_tiles.push_back(Rect2i(thread_min_x, tz, thread_max_x - thread_min_x + 1, 1));
			p_job->tile_segments.push_back(all_segments);
		}
		return;
	}

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
				if (center.distance_squared_to(proj) <= combined_radius_sq) {
					overlapping_segments.push_back((int)seg_idx);
				}
			}

			// Interior tiles of closed splines have no nearby segments but still need deforming.
			bool is_inside = false;
			if (p_job->spline->get_is_closed()) {
				// An empty segment list checks interior status without evaluating any segment math.
				ProceduralSpline3D::SplineEval center_eval =
						p_job->spline->evaluate_spline_point_segmented(center, std::vector<int>());
				is_inside = center_eval.is_inside;
			}

			if (overlapping_segments.empty() && !is_inside) {
				continue;
			}
			p_job->active_tiles.push_back(Rect2i(tx, tz, t_max_x - tx + 1, t_max_z - tz + 1));
			if (is_inside) {
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

void TerrainSplineDeformer::_print_deformer_debug_info(
		ProceduralSpline3D *p_spline,
		const Rect2 &p_aabb
) {
	Ref<Curve3D> c = p_spline->get_curve();
	int ctrl_pts = c.is_valid() ? c->get_point_count() : 0;
	float baked_len = c.is_valid() ? c->get_baked_length() : 0.0f;
	UtilityFunctions::print(
			"    [TerrainSplineDeformer: ", get_name(), "] Control Points: ", ctrl_pts,
			" | Baked Points: ", (int)p_spline->baked_poly3d.size(), " | Baked Length: ", baked_len, "m",
			" | Bounding Box Size: ", p_aabb.size
	);
}

} // namespace godot
