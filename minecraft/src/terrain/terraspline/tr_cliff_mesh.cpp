/**
 * @file tr_cliff_mesh.cpp
 * @brief TerrainSplineCliff: stitches per-station cross-sections into one flat-shaded mesh.
 */
#include "tr_cliff.h"
#include <godot_cpp/classes/array_mesh.hpp>
#include <godot_cpp/classes/geometry2d.hpp>
#include <godot_cpp/variant/packed_vector2_array.hpp>

namespace godot {

namespace {

/// Accumulates flat-shaded triangles (3 unique vertices each) with per-vertex colour.
struct MeshBuilder {
	PackedVector3Array vertices;
	PackedVector3Array normals;
	PackedColorArray colors;
	PackedInt32Array indices;

	/// Adds a triangle wound clockwise as seen from `p_outward` (Godot's front face) with that normal.
	void
	tri(const Vector3 &a,
		const Vector3 &b,
		const Vector3 &c,
		const Vector3 &p_outward,
		const Color &p_color) {
		Vector3 n = (b - a).cross(c - a);
		if (n.length_squared() < 1e-12f) {
			return; // Degenerate (coincident profile points)
		}
		Vector3 v1 = b, v2 = c;
		if (n.dot(p_outward) > 0.0f) { // Counter-clockwise from outside -> swap to clockwise
			std::swap(v1, v2);
			n = -n;
		}
		n = -n; // Clockwise cross points inwards; lighting normal is outwards
		n.normalize();
		const int base = vertices.size();
		vertices.push_back(a);
		vertices.push_back(v1);
		vertices.push_back(v2);
		for (int i = 0; i < 3; ++i) {
			normals.push_back(n);
			colors.push_back(p_color);
			indices.push_back(base + i);
		}
	}

	void
	quad(const Vector3 &a,
		 const Vector3 &b,
		 const Vector3 &c,
		 const Vector3 &d,
		 const Vector3 &p_outward,
		 const Color &p_color) {
		tri(a, b, c, p_outward, p_color);
		tri(a, c, d, p_outward, p_color);
	}
};

} // namespace

/// World position of a profile corner at a station.
static inline Vector3
place(const TerrainSplineCliff::Station &p_station,
	  const TerrainSplineCliff::ProfilePoint &p_point) {
	return p_station.top + p_station.out * p_point.offset + Vector3(0, -p_point.depth, 0);
}

/**
 * @brief For every pair of consecutive stations, one quad per profile edge. The outward reference for
 * a quad is the profile edge's 2-D normal (vertical face -> out, ledge top -> up, overhang -> down)
 * expressed in 3-D at the first station. Colour is the layer's gradient sample; horizontal ledge tops
 * are lightened a touch so they read as steps. Open ends get a fan cap.
 */
Ref<ArrayMesh> TerrainSplineCliff::_build_mesh(
		const std::vector<Station> &p_stations,
		const std::vector<std::vector<ProfilePoint>> &p_profiles,
		bool p_closed
) const {
	MeshBuilder mb;
	const size_t n = p_stations.size();
	const size_t segments = p_closed ? n : n - 1;
	const int max_k = MAX(1, strata - 1);

	auto stratum_color = [&](int p_stratum, bool p_ledge_top) -> Color {
		Color c = colors.is_valid() ? colors->sample((float)p_stratum / (float)max_k) : Color(0.7f, 0.6f, 0.5f);
		return p_ledge_top ? c.lightened(0.12f) : c;
	};

	for (size_t s = 0; s < segments; ++s) {
		const Station &s0 = p_stations[s];
		const Station &s1 = p_stations[(s + 1) % n];
		const std::vector<ProfilePoint> &pr0 = p_profiles[s];
		const std::vector<ProfilePoint> &pr1 = p_profiles[(s + 1) % n];
		const size_t m = MIN(pr0.size(), pr1.size());
		for (size_t j = 0; j + 1 < m; ++j) {
			const ProfilePoint &a0 = pr0[j], &a1 = pr0[j + 1];
			const ProfilePoint &b0 = pr1[j], &b1 = pr1[j + 1];
			// 2-D outward normal of this profile edge: (-d_depth, d_offset) in (offset, up) space.
			const float d_off = a1.offset - a0.offset;
			const float d_down = a1.depth - a0.depth; // + = going down
			const Vector3 outward = s0.out * d_down + Vector3(0, 1, 0) * d_off;
			const bool ledge_top = d_off > 1e-4f && Math::abs(d_down) < 1e-4f;
			const Color col = stratum_color(a1.stratum, ledge_top);
			mb.quad(place(s0, a0), place(s0, a1), place(s1, b1), place(s1, b0), outward, col);
		}
	}

	if (!p_closed && cap_ends && n >= 2) {
		// Fan from the top-edge point to every other profile point; outward = backwards / forwards along the wall.
		for (int end = 0; end < 2; ++end) {
			const size_t idx = end == 0 ? 0 : n - 1;
			const Station &st = p_stations[idx];
			const std::vector<ProfilePoint> &pr = p_profiles[idx];
			if (pr.size() < 3) {
				continue;
			}
			const Vector3 along = (p_stations[1].top - p_stations[0].top).normalized();
			const Vector3 outward = end == 0 ? -along : along;
			const Vector3 apex = place(st, pr[0]);
			for (size_t j = 1; j + 1 < pr.size(); ++j) {
				mb.tri(apex, place(st, pr[j]), place(st, pr[j + 1]), outward, stratum_color(pr[j].stratum, false));
			}
		}
	}

	// Top cap (closed loops): a smooth membrane over the rim, as its own surface (grass / snow material).
	PackedVector3Array cap_v, cap_n;
	PackedColorArray cap_c;
	PackedInt32Array cap_i;
	if (p_closed && cap_top && n >= 3) {
		std::vector<Vector3> rim(n);
		for (size_t i = 0; i < n; ++i) {
			rim[i] = place(p_stations[i], p_profiles[i][0]); // First profile point = top edge (no lip when capped)
		}
		_build_cap(rim, cap_v, cap_n, cap_c, cap_i);
	}

	auto add_surface = [](const Ref<ArrayMesh> &p_mesh, const MeshBuilder &p_mb) {
		if (p_mb.vertices.size() < 3) {
			return;
		}
		Array arrays;
		arrays.resize(Mesh::ARRAY_MAX);
		arrays[Mesh::ARRAY_VERTEX] = p_mb.vertices;
		arrays[Mesh::ARRAY_NORMAL] = p_mb.normals;
		arrays[Mesh::ARRAY_COLOR] = p_mb.colors;
		arrays[Mesh::ARRAY_INDEX] = p_mb.indices;
		p_mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, arrays);
	};

	Ref<ArrayMesh> mesh;
	mesh.instantiate();
	add_surface(mesh, mb); // Surface 0: wall
	if (cap_v.size() >= 3) { // Surface 1: top (shared vertices, smooth normals)
		Array arrays;
		arrays.resize(Mesh::ARRAY_MAX);
		arrays[Mesh::ARRAY_VERTEX] = cap_v;
		arrays[Mesh::ARRAY_NORMAL] = cap_n;
		arrays[Mesh::ARRAY_COLOR] = cap_c;
		arrays[Mesh::ARRAY_INDEX] = cap_i;
		mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, arrays);
	}
	return mesh;
}

// ---------------------------------------------------------------------------------------------
// Top cap
// ---------------------------------------------------------------------------------------------

namespace {

bool point_in_polygon_xz(
		const std::vector<Vector3> &p_poly,
		float x,
		float z
) {
	bool inside = false;
	const size_t n = p_poly.size();
	for (size_t i = 0, j = n - 1; i < n; j = i++) {
		const Vector3 &a = p_poly[i], &b = p_poly[j];
		if (((a.z > z) != (b.z > z)) && (x < (b.x - a.x) * (z - a.z) / (b.z - a.z) + a.x)) {
			inside = !inside;
		}
	}
	return inside;
}

float distance_to_polygon_xz(
		const std::vector<Vector3> &p_poly,
		float x,
		float z
) {
	float best = 1e30f;
	const size_t n = p_poly.size();
	for (size_t i = 0, j = n - 1; i < n; j = i++) {
		const Vector2 a(p_poly[j].x, p_poly[j].z), b(p_poly[i].x, p_poly[i].z), p(x, z);
		const Vector2 ab = b - a;
		const float l2 = ab.length_squared();
		const float t = l2 > 0.0f ? CLAMP((p - a).dot(ab) / l2, 0.0f, 1.0f) : 0.0f;
		best = MIN(best, (a + ab * t).distance_to(p));
	}
	return best;
}

} // namespace

/**
 * @brief Smooth top surface. Interior sample points on a `cap_resolution` grid (kept off the rim),
 * Delaunay-triangulated with the rim and clipped to the polygon; heights start as inverse-distance
 * interpolation of the rim (the same idea the deformer uses for closed splines) and are relaxed with
 * `cap_smoothing` Laplacian passes into a membrane, then optionally domed. Shared vertices with
 * averaged normals so the surface shades smoothly instead of as a fan of slivers.
 */
void TerrainSplineCliff::_build_cap(
		const std::vector<Vector3> &p_rim,
		PackedVector3Array &r_vertices,
		PackedVector3Array &r_normals,
		PackedColorArray &r_colors,
		PackedInt32Array &r_indices
) const {
	const size_t n_rim = p_rim.size();
	std::vector<Vector3> pts(p_rim);
	std::vector<bool> fixed(n_rim, true);

	// Interior grid samples.
	Rect2 bounds(Vector2(p_rim[0].x, p_rim[0].z), Vector2());
	for (const Vector3 &v : p_rim) {
		bounds = bounds.expand(Vector2(v.x, v.z));
	}
	const float margin = cap_resolution * 0.45f;
	float max_inner_dist = 0.0f;
	for (float z = bounds.position.y; z <= bounds.position.y + bounds.size.y; z += cap_resolution) {
		for (float x = bounds.position.x; x <= bounds.position.x + bounds.size.x; x += cap_resolution) {
			if (!point_in_polygon_xz(p_rim, x, z)) {
				continue;
			}
			const float d = distance_to_polygon_xz(p_rim, x, z);
			if (d < margin) {
				continue;
			}
			max_inner_dist = MAX(max_inner_dist, d);
			// Initial height: inverse-distance weighting of the rim heights.
			float wsum = 0.0f, hsum = 0.0f;
			for (const Vector3 &r : p_rim) {
				const float d2 = (r.x - x) * (r.x - x) + (r.z - z) * (r.z - z) + 1e-3f;
				const float w = 1.0f / d2;
				wsum += w;
				hsum += w * r.y;
			}
			pts.push_back(Vector3(x, hsum / wsum, z));
			fixed.push_back(false);
		}
	}

	// Delaunay over rim + interior, clipped to the rim polygon.
	PackedVector2Array flat;
	for (const Vector3 &v : pts) {
		flat.push_back(Vector2(v.x, v.z));
	}
	PackedInt32Array tris = Geometry2D::get_singleton()->triangulate_delaunay(flat);
	std::vector<int> kept;
	for (int t = 0; t + 2 < tris.size(); t += 3) {
		const Vector3 c = (pts[tris[t]] + pts[tris[t + 1]] + pts[tris[t + 2]]) / 3.0f;
		if (point_in_polygon_xz(p_rim, c.x, c.z)) {
			kept.push_back(tris[t]);
			kept.push_back(tris[t + 1]);
			kept.push_back(tris[t + 2]);
		}
	}
	if (kept.empty()) {
		return;
	}

	// Laplacian relaxation of interior heights (rim fixed) -> smooth membrane.
	std::vector<std::vector<int>> nbrs(pts.size());
	for (size_t t = 0; t < kept.size(); t += 3) {
		for (int k = 0; k < 3; ++k) {
			const int a = kept[t + k], b = kept[t + (k + 1) % 3];
			nbrs[a].push_back(b);
			nbrs[b].push_back(a);
		}
	}
	for (int pass = 0; pass < cap_smoothing; ++pass) {
		for (size_t i = 0; i < pts.size(); ++i) {
			if (fixed[i] || nbrs[i].empty()) {
				continue;
			}
			float sum = 0.0f;
			for (int j : nbrs[i]) {
				sum += pts[j].y;
			}
			pts[i].y = sum / (float)nbrs[i].size();
		}
	}

	// Dome: lift the interior towards the middle.
	if (cap_dome != 0.0f && max_inner_dist > 0.0f) {
		for (size_t i = 0; i < pts.size(); ++i) {
			if (fixed[i]) {
				continue;
			}
			const float t = CLAMP(distance_to_polygon_xz(p_rim, pts[i].x, pts[i].z) / max_inner_dist, 0.0f, 1.0f);
			pts[i].y += cap_dome * (t * t * (3.0f - 2.0f * t));
		}
	}

	// Emit with clockwise winding (seen from above) and averaged normals.
	std::vector<Vector3> normals(pts.size(), Vector3());
	for (size_t t = 0; t < kept.size(); t += 3) {
		int i0 = kept[t], i1 = kept[t + 1], i2 = kept[t + 2];
		Vector3 nrm = (pts[i1] - pts[i0]).cross(pts[i2] - pts[i0]);
		if (nrm.y > 0.0f) { // Counter-clockwise from above -> swap to clockwise
			std::swap(i1, i2);
			nrm = -nrm;
		}
		r_indices.push_back(i0);
		r_indices.push_back(i1);
		r_indices.push_back(i2);
		nrm = -nrm; // Outward (up)
		normals[i0] += nrm;
		normals[i1] += nrm;
		normals[i2] += nrm;
	}
	for (size_t i = 0; i < pts.size(); ++i) {
		r_vertices.push_back(pts[i]);
		r_normals.push_back(normals[i].is_zero_approx() ? Vector3(0, 1, 0) : normals[i].normalized());
		r_colors.push_back(top_color);
	}
}

} // namespace godot
