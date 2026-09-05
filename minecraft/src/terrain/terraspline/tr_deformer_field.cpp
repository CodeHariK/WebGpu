/**
 * @file tr_deformer_field.cpp
 * @brief The distance-field pass (Terraspline.md, step 4).
 *
 * For one (deformer, chunk) pair, computes per pixel the nearest spline segment and the nearest
 * point on it, plus inside/outside for closed splines, on a grid padded by the search radius so
 * segments just outside the chunk still count. The per-pixel deformation then gets distance and
 * inside in O(1) and only evaluates the (expensive) spline height where the falloff weight is > 0.
 *
 * Steps, one function each:
 *  1. _field_allocate            size the grid and reset it
 *  2. _field_seed_segments       rasterize every nearby segment as seeds (exact sample points)
 *  3. _field_sweep_nearest       two 8-neighbour sweeps propagate the nearest seed (8SSEDT)
 *  4. _field_refine_adjacent     re-project onto the adopted segment and its two neighbours
 *  5. _field_exact_band          brute-force the true nearest segment where weight can be > 0
 *  6. _field_fill_interior       even-odd scanline fill for closed splines
 *  7. _field_collect_active_tiles tiles containing any pixel within reach or inside
 * Output is bit-identical to the legacy per-pixel evaluation (verified with --terraspline-dump).
 */
#include "tr_deformer.h"
#include <algorithm>

namespace godot {

// ---------------------------------------------------------------------------------------------
// Grid geometry helpers (all in world units; one pixel per metre)
// ---------------------------------------------------------------------------------------------

/// World X of grid column i.
static inline float
grid_x(const Ref<DeformerJob> &p_job,
	   int i) {
	return p_job->offset.x - p_job->field_margin + i;
}
/// World Z of grid row j.
static inline float
grid_z(const Ref<DeformerJob> &p_job,
	   int j) {
	return p_job->offset.y - p_job->field_margin + j;
}

/// Nearest point on segment `si` to (px, pz).
static inline void project_onto_segment(
		const Ref<DeformerJob> &p_job,
		int si,
		float px,
		float pz,
		float &r_nx,
		float &r_nz
) {
	const float l2 = p_job->seg_l2[si];
	float t = 0.0f;
	if (l2 > 0.0f) {
		t = ((px - p_job->seg_ax[si]) * p_job->seg_abx[si] + (pz - p_job->seg_az[si]) * p_job->seg_abz[si]) / l2;
		t = Math::clamp(t, 0.0f, 1.0f);
	}
	r_nx = p_job->seg_ax[si] + p_job->seg_abx[si] * t;
	r_nz = p_job->seg_az[si] + p_job->seg_abz[si] * t;
}

static inline float
dist2(float ax,
	  float az,
	  float bx,
	  float bz) {
	return (ax - bx) * (ax - bx) + (az - bz) * (az - bz);
}

// ---------------------------------------------------------------------------------------------
// Orchestration
// ---------------------------------------------------------------------------------------------

bool TerrainSplineDeformer::_compute_distance_field(
		Ref<DeformerJob> p_job,
		const Rect2 &p_aabb,
		int p_w,
		int p_h
) {
	if (p_job->seg_ax.empty() || p_job->vert_x.size() < 2) {
		return false;
	}
	const float search_radius = spline_width + falloff_distance;

	_field_allocate(p_job, p_w, p_h, search_radius);
	_field_seed_segments(p_job);
	_field_sweep_nearest(p_job);
	_field_refine_adjacent(p_job);
	_field_exact_band(p_job, search_radius);
	_field_fill_interior(p_job);
	_field_collect_active_tiles(p_job, p_aabb, p_w, p_h, search_radius);

	p_job->field_valid = true;
	return true;
}

// ---------------------------------------------------------------------------------------------
// 1. Allocate
// ---------------------------------------------------------------------------------------------

void TerrainSplineDeformer::_field_allocate(
		Ref<DeformerJob> p_job,
		int p_w,
		int p_h,
		float p_search_radius
) {
	const int m = (int)Math::ceil(p_search_radius) + 1;
	p_job->field_margin = m;
	p_job->field_w = p_w + 2 * m;
	p_job->field_h = p_h + 2 * m;
	const size_t n = (size_t)p_job->field_w * p_job->field_h;
	p_job->field_seg.assign(n, -1);
	p_job->field_nx.assign(n, 0.0f);
	p_job->field_nz.assign(n, 0.0f);
	p_job->field_inside.assign(n, 0);
}

// ---------------------------------------------------------------------------------------------
// 2. Seeds: sample each nearby segment every <= 0.5 px; a pixel keeps the closest sample
// ---------------------------------------------------------------------------------------------

void TerrainSplineDeformer::_field_seed_segments(Ref<DeformerJob> p_job) {
	const int gw = p_job->field_w, gh = p_job->field_h;
	const float gx0 = grid_x(p_job, 0), gz0 = grid_z(p_job, 0);
	const Rect2 grid_rect(Vector2(gx0, gz0), Vector2(gw, gh));
	int32_t *fseg = p_job->field_seg.data();
	float *fnx = p_job->field_nx.data();
	float *fnz = p_job->field_nz.data();

	const int nseg = (int)p_job->seg_ax.size();
	for (int si = 0; si < nseg; ++si) {
		const float ax = p_job->seg_ax[si], az = p_job->seg_az[si];
		const float bx = ax + p_job->seg_abx[si], bz = az + p_job->seg_abz[si];
		Rect2 seg_rect(Vector2(Math::min(ax, bx), Math::min(az, bz)), Vector2(Math::abs(bx - ax), Math::abs(bz - az)));
		if (!seg_rect.intersects(grid_rect)) {
			continue;
		}
		const float len = Math::sqrt(p_job->seg_l2[si]);
		const int steps = Math::max(1, (int)Math::ceil(len * 2.0f));
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
			if (fseg[idx] < 0 || dist2(px, pz, cx, cz) < dist2(fnx[idx], fnz[idx], cx, cz)) {
				fseg[idx] = si;
				fnx[idx] = px;
				fnz[idx] = pz;
			}
		}
	}
}

// ---------------------------------------------------------------------------------------------
// 3. Sweeps: forward then backward, each pixel adopts a neighbour's seed if it is closer
// ---------------------------------------------------------------------------------------------

void TerrainSplineDeformer::_field_sweep_nearest(Ref<DeformerJob> p_job) {
	const int gw = p_job->field_w, gh = p_job->field_h;
	const float gx0 = grid_x(p_job, 0), gz0 = grid_z(p_job, 0);
	int32_t *fseg = p_job->field_seg.data();
	float *fnx = p_job->field_nx.data();
	float *fnz = p_job->field_nz.data();

	auto consider = [&](size_t idx, float cx, float cz, size_t nidx) {
		const int32_t ns = fseg[nidx];
		if (ns < 0) {
			return;
		}
		if (fseg[idx] < 0 || dist2(fnx[nidx], fnz[nidx], cx, cz) < dist2(fnx[idx], fnz[idx], cx, cz)) {
			fseg[idx] = ns;
			fnx[idx] = fnx[nidx];
			fnz[idx] = fnz[nidx];
		}
	};

	for (int j = 0; j < gh; ++j) {
		for (int i = 0; i < gw; ++i) {
			const size_t idx = (size_t)j * gw + i;
			const float cx = gx0 + i, cz = gz0 + j;
			if (i > 0)
				consider(idx, cx, cz, idx - 1);
			if (j > 0) {
				consider(idx, cx, cz, idx - gw);
				if (i > 0)
					consider(idx, cx, cz, idx - gw - 1);
				if (i + 1 < gw)
					consider(idx, cx, cz, idx - gw + 1);
			}
		}
	}
	for (int j = gh - 1; j >= 0; --j) {
		for (int i = gw - 1; i >= 0; --i) {
			const size_t idx = (size_t)j * gw + i;
			const float cx = gx0 + i, cz = gz0 + j;
			if (i + 1 < gw)
				consider(idx, cx, cz, idx + 1);
			if (j + 1 < gh) {
				consider(idx, cx, cz, idx + gw);
				if (i + 1 < gw)
					consider(idx, cx, cz, idx + gw + 1);
				if (i > 0)
					consider(idx, cx, cz, idx + gw - 1);
			}
		}
	}
}

// ---------------------------------------------------------------------------------------------
// 4. Refine: exact projection onto the adopted segment and its sequence neighbours
// ---------------------------------------------------------------------------------------------

void TerrainSplineDeformer::_field_refine_adjacent(Ref<DeformerJob> p_job) {
	const int gw = p_job->field_w, gh = p_job->field_h;
	const float gx0 = grid_x(p_job, 0), gz0 = grid_z(p_job, 0);
	const int nseg = (int)p_job->seg_ax.size();
	int32_t *fseg = p_job->field_seg.data();
	float *fnx = p_job->field_nx.data();
	float *fnz = p_job->field_nz.data();

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
			project_onto_segment(p_job, s0, cx, cz, bnx, bnz);
			float bd = dist2(bnx, bnz, cx, cz);
			for (int dsi = -1; dsi <= 1; dsi += 2) {
				int si = s0 + dsi;
				if (p_job->spline_closed) {
					si = (si + nseg) % nseg;
				} else if (si < 0 || si >= nseg) {
					continue;
				}
				float nx, nz;
				project_onto_segment(p_job, si, cx, cz, nx, nz);
				const float d = dist2(nx, nz, cx, cz);
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
}

// ---------------------------------------------------------------------------------------------
// 5. Exact band: the sweeps can lose the true nearest segment at polyline folds, and inside the
//    falloff band distance drives the weight ramp, so resolve those pixels by brute force over
//    bbox-culled segments (the same cull the legacy path applied). The band is a thin strip along
//    the spline, so this stays cheap; the far interior keeps the O(1) field.
// ---------------------------------------------------------------------------------------------

void TerrainSplineDeformer::_field_exact_band(
		Ref<DeformerJob> p_job,
		float p_search_radius
) {
	const int gw = p_job->field_w, gh = p_job->field_h;
	const float gx0 = grid_x(p_job, 0), gz0 = grid_z(p_job, 0);
	const int nseg = (int)p_job->seg_ax.size();
	int32_t *fseg = p_job->field_seg.data();
	float *fnx = p_job->field_nx.data();
	float *fnz = p_job->field_nz.data();

	const float band = p_search_radius + 1.5f;
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
			float bd = dist2(fnx[idx], fnz[idx], cx, cz);
			if (bd > band2) {
				continue;
			}
			for (int si = 0; si < nseg; ++si) {
				if (cx < smin_x[si] || cx > smax_x[si] || cz < smin_z[si] || cz > smax_z[si]) {
					continue;
				}
				float nx, nz;
				project_onto_segment(p_job, si, cx, cz, nx, nz);
				const float d = dist2(nx, nz, cx, cz);
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

// ---------------------------------------------------------------------------------------------
// 6. Interior: even-odd scanline fill of the closed polygon
// ---------------------------------------------------------------------------------------------

void TerrainSplineDeformer::_field_fill_interior(Ref<DeformerJob> p_job) {
	if (!p_job->spline_closed) {
		return;
	}
	const int gw = p_job->field_w, gh = p_job->field_h;
	const float gx0 = grid_x(p_job, 0), gz0 = grid_z(p_job, 0);
	uint8_t *fin = p_job->field_inside.data();
	const int nv = (int)p_job->vert_x.size();

	std::vector<float> xs;
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

// ---------------------------------------------------------------------------------------------
// 7. Active tiles: a tile is active if any of its pixels is within reach or inside
// ---------------------------------------------------------------------------------------------

void TerrainSplineDeformer::_field_collect_active_tiles(
		Ref<DeformerJob> p_job,
		const Rect2 &p_aabb,
		int p_w,
		int p_h,
		float p_search_radius
) {
	const int32_t *fseg = p_job->field_seg.data();
	const float *fnx = p_job->field_nx.data();
	const float *fnz = p_job->field_nz.data();
	const uint8_t *fin = p_job->field_inside.data();
	const float r2 = p_search_radius * p_search_radius;

	const int min_x = Math::max(0, (int)Math::floor(p_aabb.position.x - p_job->offset.x));
	const int max_x = Math::min(p_w - 1, (int)Math::ceil(p_aabb.position.x + p_aabb.size.x - p_job->offset.x));
	const int min_z = Math::max(0, (int)Math::floor(p_aabb.position.y - p_job->offset.y));
	const int max_z = Math::min(p_h - 1, (int)Math::ceil(p_aabb.position.y + p_aabb.size.y - p_job->offset.y));
	const int ts = Math::max(8, tile_size);

	for (int tz = min_z; tz <= max_z; tz += ts) {
		for (int tx = min_x; tx <= max_x; tx += ts) {
			const int t_max_x = Math::min(tx + ts - 1, max_x);
			const int t_max_z = Math::min(tz + ts - 1, max_z);
			bool active = false;
			for (int z = tz; z <= t_max_z && !active; ++z) {
				for (int x = tx; x <= t_max_x; ++x) {
					const size_t idx = p_job->field_index(x, z);
					if (fin[idx]) {
						active = true;
						break;
					}
					if (fseg[idx] >= 0 && dist2(fnx[idx], fnz[idx], p_job->offset.x + x, p_job->offset.y + z) <= r2) {
						active = true;
						break;
					}
				}
			}
			if (active) {
				p_job->active_tiles.push_back(Rect2i(tx, tz, t_max_x - tx + 1, t_max_z - tz + 1));
			}
		}
	}
}

} // namespace godot
