/**
 * @file tr_deformer_profile.cpp
 * @brief HEIGHT_TERRAIN: derive the spline's height profile from the undeformed ground.
 *
 * The profile is a 1-D function of arc length: base terrain sampled at every baked vertex, box-filtered
 * over `profile_smoothing` metres, then limited to `max_grade`. It overwrites the job's vertex and
 * segment Y arrays, so every interpolation mode and both tile loops read it unchanged. Only the base
 * noise is sampled (never a chunk buffer), which keeps the result identical across chunk borders.
 */
#include "tr_deformer.h"
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

/// Arc length from vertex 0 to each vertex; for closed splines also the closing segment's length.
static void cumulative_arc_lengths(
		const Ref<DeformerJob> &p_job,
		std::vector<float> &r_s,
		float &r_total
) {
	const size_t n = p_job->vert_x.size();
	r_s.assign(n, 0.0f);
	for (size_t i = 1; i < n; ++i) {
		const float dx = p_job->vert_x[i] - p_job->vert_x[i - 1];
		const float dz = p_job->vert_z[i] - p_job->vert_z[i - 1];
		r_s[i] = r_s[i - 1] + Math::sqrt(dx * dx + dz * dz);
	}
	r_total = n > 0 ? r_s[n - 1] : 0.0f;
	if (p_job->spline_closed && n > 1) {
		const float dx = p_job->vert_x[0] - p_job->vert_x[n - 1];
		const float dz = p_job->vert_z[0] - p_job->vert_z[n - 1];
		r_total += Math::sqrt(dx * dx + dz * dz);
	}
}

static void box_filter_along_arc(
		const std::vector<float> &p_s,
		float p_total,
		bool p_closed,
		float p_window,
		std::vector<float> &r_h
);

/// Arc-length distance between vertices i and j (shortest way round for closed splines).
static inline float arc_distance(
		const std::vector<float> &p_s,
		float p_total,
		bool p_closed,
		size_t i,
		size_t j
) {
	float d = Math::abs(p_s[j] - p_s[i]);
	return p_closed ? Math::min(d, p_total - d) : d;
}

/// Morphological min (p_erode) or max filter of width p_window along the arc.
static void minmax_filter_along_arc(
		const std::vector<float> &p_s,
		float p_total,
		bool p_closed,
		float p_window,
		bool p_erode,
		std::vector<float> &r_h
) {
	const size_t n = r_h.size();
	if (n < 2 || p_window <= 0.0f) {
		return;
	}
	const float half = p_window * 0.5f;
	std::vector<float> out(n);
	for (size_t i = 0; i < n; ++i) {
		float v = r_h[i];
		for (size_t j = 0; j < n; ++j) {
			if (arc_distance(p_s, p_total, p_closed, i, j) <= half) {
				v = p_erode ? Math::min(v, r_h[j]) : Math::max(v, r_h[j]);
			}
		}
		out[i] = v;
	}
	r_h.swap(out);
}

/**
 * @brief Smooth profile of width p_window that never crosses the input: below it when p_below, above
 * it otherwise. Erode (or dilate) over the window first, then smooth with a triangular kernel of the
 * same total width. Every sample averaged at i comes from a window that contains i itself, so the
 * average of the eroded signal cannot exceed the original at i - smoothness without a jagged clamp.
 */
static void smooth_one_sided(
		const std::vector<float> &p_s,
		float p_total,
		bool p_closed,
		float p_window,
		bool p_below,
		std::vector<float> &r_h
) {
	minmax_filter_along_arc(p_s, p_total, p_closed, p_window, /*p_erode=*/p_below, r_h);
	box_filter_along_arc(p_s, p_total, p_closed, p_window * 0.5f, r_h);
	box_filter_along_arc(p_s, p_total, p_closed, p_window * 0.5f, r_h);
}

/// One box-filter pass of width p_window (metres of arc length). Open splines clamp at the ends; closed wrap.
static void box_filter_along_arc(
		const std::vector<float> &p_s,
		float p_total,
		bool p_closed,
		float p_window,
		std::vector<float> &r_h
) {
	const size_t n = r_h.size();
	if (n < 2 || p_window <= 0.0f) {
		return;
	}
	const float half = p_window * 0.5f;
	std::vector<float> out(n);
	for (size_t i = 0; i < n; ++i) {
		float sum = 0.0f;
		int count = 0;
		for (size_t j = 0; j < n; ++j) {
			float d = Math::abs(p_s[j] - p_s[i]);
			if (p_closed) {
				d = Math::min(d, p_total - d);
			}
			if (d <= half) {
				sum += r_h[j];
				++count;
			}
		}
		out[i] = count > 0 ? sum / count : r_h[i];
	}
	r_h.swap(out);
}

/// Limits |dh/ds| to p_grade with a forward and a backward pass (two laps for closed splines).
static void clamp_grade(
		const std::vector<float> &p_s,
		float p_total,
		bool p_closed,
		float p_grade,
		std::vector<float> &r_h
) {
	const size_t n = r_h.size();
	if (n < 2 || p_grade <= 0.0f) {
		return;
	}
	auto ds = [&](size_t a, size_t b) -> float { // Arc length from vertex a to the next vertex b
		return b > a ? p_s[b] - p_s[a] : (p_total - p_s[a]) + p_s[b];
	};
	const int laps = p_closed ? 2 : 1;
	for (int lap = 0; lap < laps; ++lap) {
		for (size_t k = 1; k < n + (p_closed ? 1 : 0); ++k) { // Forward
			const size_t prev = (k - 1) % n, cur = k % n;
			const float lim = p_grade * ds(prev, cur);
			r_h[cur] = Math::clamp(r_h[cur], r_h[prev] - lim, r_h[prev] + lim);
		}
		for (size_t k = n + (p_closed ? 1 : 0); k-- > 1;) { // Backward
			const size_t prev = (k - 1) % n, cur = k % n;
			const float lim = p_grade * ds(prev, cur);
			r_h[prev] = Math::clamp(r_h[prev], r_h[cur] - lim, r_h[cur] + lim);
		}
	}
}

/// Ground height at (x, z): base noise, then every context deformer applied in order.
float TerrainSplineDeformer::_sample_ground(
		const Ref<TerrainHeightmap> &p_heightmap,
		const std::vector<Ref<DeformerJob>> &p_context,
		float p_x,
		float p_z
) const {
	float h = p_heightmap->base_height_at(p_x, p_z);
	for (const Ref<DeformerJob> &job : p_context) {
		h = job->deformer->evaluate_height_at(job, p_x, p_z, h);
	}
	return h;
}

/**
 * @brief Grade-limited envelope that only ever moves the profile one way.
 * Lower (p_cut = true):  h'[i] = min_j (h[j] + g * d(i, j)) - never above h, slope <= g everywhere.
 * Upper (p_cut = false): h'[i] = max_j (h[j] - g * d(i, j)) - never below h, slope <= g everywhere.
 * (A slope-constrained erosion/dilation along the arc; O(n^2) over a few hundred vertices.)
 */
static void grade_envelope(
		const std::vector<float> &p_s,
		float p_total,
		bool p_closed,
		float p_grade,
		bool p_cut,
		std::vector<float> &r_h
) {
	const size_t n = r_h.size();
	if (n < 2 || p_grade <= 0.0f) {
		return;
	}
	std::vector<float> out(n);
	for (size_t i = 0; i < n; ++i) {
		float best = r_h[i];
		for (size_t j = 0; j < n; ++j) {
			float d = Math::abs(p_s[j] - p_s[i]);
			if (p_closed) {
				d = Math::min(d, p_total - d);
			}
			best = p_cut ? Math::min(best, r_h[j] + p_grade * d) : Math::max(best, r_h[j] - p_grade * d);
		}
		out[i] = best;
	}
	r_h.swap(out);
}

void TerrainSplineDeformer::_bake_terrain_profile(
		const Ref<DeformerJob> &p_job,
		const Ref<TerrainHeightmap> &p_heightmap
) const {
	const size_t n = p_job->vert_x.size();
	if (n == 0 || p_heightmap.is_null()) {
		return;
	}
	// Other splines' deformers, prepared once so each vertex sample is an O(segments) evaluation.
	std::vector<Ref<DeformerJob>> context;
	if (profile_include_splines) {
		for (const TerrainHeightmap::BaseDeformer &bd : p_heightmap->get_base_deformers()) {
			if (bd.deformer && bd.spline && bd.deformer != this) {
				context.push_back(bd.deformer->_create_deformer_job(p_heightmap, bd.spline, p_job->offset));
			}
		}
	}
	std::vector<float> ground(n);
	for (size_t i = 0; i < n; ++i) {
		ground[i] = _sample_ground(p_heightmap, context, p_job->vert_x[i], p_job->vert_z[i]);
	}
	std::vector<float> h = ground;

	std::vector<float> s;
	float total = 0.0f;
	cumulative_arc_lengths(p_job, s, total);
	const bool closed = p_job->spline_closed;
	const float grade = max_grade * 0.01f;
	switch (earthwork) {
		case EARTHWORK_CUT_ONLY: // Smooth surface under the ground, then ramp cuts into anything too steep.
			smooth_one_sided(s, total, closed, profile_smoothing, /*p_below=*/true, h);
			grade_envelope(s, total, closed, grade, /*p_cut=*/true, h);
			break;
		case EARTHWORK_FILL_ONLY:
			smooth_one_sided(s, total, closed, profile_smoothing, /*p_below=*/false, h);
			grade_envelope(s, total, closed, grade, /*p_cut=*/false, h);
			break;
		case EARTHWORK_CUT_AND_FILL:
		default:
			// Triangular kernel (two half-width boxes): a cliff becomes an S-curve, not a kinked ramp.
			box_filter_along_arc(s, total, closed, profile_smoothing * 0.5f, h);
			box_filter_along_arc(s, total, closed, profile_smoothing * 0.5f, h);
			clamp_grade(s, total, closed, grade, h);
			break;
	}
	// Depth caps win over smoothing and grade. The caps are smooth surfaces themselves (dilated / eroded
	// ground, then smoothed) so engaging one never copies the ground's bumps onto the road.
	if (max_cut_depth > 0.0f) {
		std::vector<float> floor_h = ground; // >= ground everywhere, smooth
		smooth_one_sided(s, total, closed, profile_smoothing, /*p_below=*/false, floor_h);
		for (size_t i = 0; i < n; ++i) {
			float floor_i = floor_h[i] - max_cut_depth;
			if (earthwork == EARTHWORK_CUT_ONLY) {
				floor_i = Math::min(floor_i, ground[i]); // The cap may never turn a cut into a fill
			}
			h[i] = Math::max(h[i], floor_i);
		}
	}
	if (max_fill_height > 0.0f) {
		std::vector<float> ceil_h = ground; // <= ground everywhere, smooth
		smooth_one_sided(s, total, closed, profile_smoothing, /*p_below=*/true, ceil_h);
		for (size_t i = 0; i < n; ++i) {
			float ceil_i = ceil_h[i] + max_fill_height;
			if (earthwork == EARTHWORK_FILL_ONLY) {
				ceil_i = Math::max(ceil_i, ground[i]); // The cap may never turn a fill into a cut
			}
			h[i] = Math::min(h[i], ceil_i);
		}
	}

	// Final blur: the caps above copy the ground's shape (steps, terraces) onto the road wherever they
	// engage; this rounds those off. It is deliberately allowed to violate the caps a little (a step
	// becomes a ramp half cut / half fill) - smoothness is what makes it a road.
	if (road_blur > 0.0f) {
		box_filter_along_arc(s, total, closed, road_blur * 0.5f, h);
		box_filter_along_arc(s, total, closed, road_blur * 0.5f, h);
	}

#if DEBUG
	{
		float raw_min = 1e20f, raw_max = -1e20f, out_min = 1e20f, out_max = -1e20f;
		for (size_t i = 0; i < n; ++i) {
			raw_min = Math::min(raw_min, ground[i]);
			raw_max = Math::max(raw_max, ground[i]);
			out_min = Math::min(out_min, h[i]);
			out_max = Math::max(out_max, h[i]);
		}
		float max_step = 0.0f, max_kink = 0.0f; // Largest rise between vertices / change of rise (bumpiness)
		for (size_t i = 1; i < n; ++i) {
			max_step = Math::max(max_step, Math::abs(h[i] - h[i - 1]));
			if (i + 1 < n) {
				max_kink = Math::max(max_kink, Math::abs((h[i + 1] - h[i]) - (h[i] - h[i - 1])));
			}
		}
		UtilityFunctions::print(
				"    [TerrainSplineDeformer] terrain profile over ", (int)n, " vertices, ", total, " m: ground ",
				raw_min, "..", raw_max, " -> road ", out_min, "..", out_max, " | max step ", max_step, " m, max kink ",
				max_kink, " m per vertex"
		);
	}
#endif
	p_job->vert_y = h;
	// The profile is already smooth along the arc; read it exactly from the nearest segment. IDW modes
	// mix in neighbouring vertices' heights and put periodic beads on a sloping centreline.
	p_job->interpolation_mode = ProceduralSpline3D::INTERP_NEAREST;
	const size_t nseg = p_job->seg_y0.size();
	for (size_t i = 0; i < nseg; ++i) { // Segment i runs from vertex i to vertex (i + 1) % n
		const float y0 = h[i % n];
		const float y1 = h[(i + 1) % n];
		p_job->seg_y0[i] = y0;
		p_job->seg_dy[i] = y1 - y0;
	}
}

bool TerrainSplineDeformer::bake_road_profile(
		const Ref<TerrainHeightmap> &p_heightmap,
		ProceduralSpline3D *p_spline,
		std::vector<Vector3> &r_points
) {
	r_points.clear();
	if (p_heightmap.is_null() || !p_spline || height_source != HEIGHT_TERRAIN) {
		return false;
	}
	p_spline->ensure_baked_cache();
	Ref<DeformerJob> job = _create_deformer_job(p_heightmap, p_spline, Vector2()); // Bakes the profile
	const size_t n = job->vert_x.size();
	r_points.resize(n);
	for (size_t i = 0; i < n; ++i) {
		r_points[i] = Vector3(job->vert_x[i], job->vert_y[i], job->vert_z[i]);
	}
	return n >= 2;
}

} // namespace godot
