/**
 * @file stylized_terrain_mesh.cpp
 * @brief StylizedTerrainMesh: height field, terrace / faceted meshing, per-band vertex colours.
 */
#include "stylized_terrain_mesh.h"
#include "environment/prop_geometry.h"
#include <cmath>
#include <cstdint>
#include <godot_cpp/core/class_db.hpp>
#include <map>
#include <unordered_map>
#include <utility>
#include <vector>

namespace godot {

// clang-format off
#define ST_BIND(m_variant, m_name, m_hint, m_hint_str)                                                                \
	ClassDB::bind_method(D_METHOD("set_" #m_name, "value"), &StylizedTerrainMesh::set_##m_name);                       \
	ClassDB::bind_method(D_METHOD("get_" #m_name), &StylizedTerrainMesh::get_##m_name);                                \
	ADD_PROPERTY(PropertyInfo(Variant::m_variant, #m_name, m_hint, m_hint_str), "set_" #m_name, "get_" #m_name)

void StylizedTerrainMesh::_bind_methods() {
	ADD_GROUP("Extent", "");
	ST_BIND(INT, seed, PROPERTY_HINT_NONE, "");
	ST_BIND(VECTOR2, size, PROPERTY_HINT_NONE, "");
	ST_BIND(FLOAT, cell_size, PROPERTY_HINT_RANGE, "0.5,16,0.1");
	ST_BIND(FLOAT, height_scale, PROPERTY_HINT_RANGE, "0,512,0.5");
	ST_BIND(FLOAT, base_height, PROPERTY_HINT_NONE, "");
	ADD_GROUP("Noise", "noise_");
	ST_BIND(FLOAT, noise_frequency, PROPERTY_HINT_RANGE, "0.0005,0.5,0.0005");
	ST_BIND(INT, noise_octaves, PROPERTY_HINT_RANGE, "1,8,1");
	ST_BIND(FLOAT, noise_lacunarity, PROPERTY_HINT_RANGE, "1.2,4,0.05");
	ST_BIND(FLOAT, noise_gain, PROPERTY_HINT_RANGE, "0.1,0.9,0.01");
	ST_BIND(BOOL, ridged, PROPERTY_HINT_NONE, "");
	ST_BIND(FLOAT, island_falloff, PROPERTY_HINT_RANGE, "0,1,0.01");
	ADD_GROUP("Style", "");
	ST_BIND(BOOL, terrace, PROPERTY_HINT_NONE, "");
	ST_BIND(FLOAT, step_height, PROPERTY_HINT_RANGE, "0.25,64,0.25");
	ADD_GROUP("Colour", "");
	ST_BIND(COLOR, low_color, PROPERTY_HINT_NONE, "");
	ST_BIND(COLOR, high_color, PROPERTY_HINT_NONE, "");
	ST_BIND(COLOR, edge_color, PROPERTY_HINT_NONE, "");
	ST_BIND(FLOAT, riser_darken, PROPERTY_HINT_RANGE, "0,1,0.01");
	ST_BIND(FLOAT, color_jitter, PROPERTY_HINT_RANGE, "0,0.5,0.01");
	ClassDB::bind_method(D_METHOD("rebuild"), &StylizedTerrainMesh::rebuild);
	ClassDB::bind_method(D_METHOD("sample_height", "world"), &StylizedTerrainMesh::sample_height);
	ClassDB::bind_method(D_METHOD("get_contours"), &StylizedTerrainMesh::get_contours);
}
#undef ST_BIND
// clang-format on

StylizedTerrainMesh::StylizedTerrainMesh() { _queue_rebuild(); }
StylizedTerrainMesh::~StylizedTerrainMesh() {}

void StylizedTerrainMesh::_validate_property(PropertyInfo &p_property) const {
	if (p_property.name == StringName("_surfaces")) {
		p_property.usage &= ~PROPERTY_USAGE_STORAGE;
	}
}

void StylizedTerrainMesh::_queue_rebuild() {
	if (!_rebuild_queued) {
		_rebuild_queued = true;
		call_deferred("rebuild");
	}
}

// ---------------------------------------------------------------------------------------------
// Height field
// ---------------------------------------------------------------------------------------------

float StylizedTerrainMesh::_height_raw(
		float p_wx,
		float p_wz
) const {
	const Vector3 p(p_wx * noise_frequency, 0.0f, p_wz * noise_frequency);
	const float n = prop::fbm(p, noise_octaves, noise_lacunarity, noise_gain, (uint32_t)seed, ridged);
	float hn = CLAMP(n * 0.5f + 0.5f, 0.0f, 1.0f); // 0..1

	if (island_falloff > 0.0f) {
		const float rx = p_wx / (0.5f * size.x);
		const float rz = p_wz / (0.5f * size.y);
		const float d = CLAMP(Math::sqrt(rx * rx + rz * rz), 0.0f, 1.0f); // 0 centre .. 1 corner
		const float mask = 1.0f - Math::smoothstep(0.55f, 1.0f, d) * island_falloff;
		hn *= mask;
	}
	return base_height + hn * height_scale;
}

Color StylizedTerrainMesh::_surface_color(
		float p_h_norm,
		float p_cx,
		float p_cz
) const {
	Color c = low_color.lerp(high_color, CLAMP(p_h_norm, 0.0f, 1.0f));
	const float rim = Math::smoothstep(0.18f, 0.0f, p_h_norm); // near the low end -> dry rim
	c = c.lerp(edge_color, rim * 0.6f);
	if (color_jitter > 0.0f) {
		const float j = prop::value_noise(Vector3(p_cx * 0.35f, 0.0f, p_cz * 0.35f), (uint32_t)seed + 101u);
		c = c.lerp(j > 0.0f ? high_color : low_color, Math::abs(j) * color_jitter);
	}
	return c;
}

float StylizedTerrainMesh::sample_height(const Vector2 &p_world) const {
	const float h = _height_raw(p_world.x, p_world.y);
	if (!terrace) {
		return h;
	}
	const float rel = h - base_height;
	return base_height + Math::floor(rel / step_height) * step_height;
}

// ---------------------------------------------------------------------------------------------
// Meshing helpers
// ---------------------------------------------------------------------------------------------

namespace {

// A plan-position with its continuous (pre-terrace) height, carried through contour clipping.
struct PV {
	Vector2 p;
	float h;
};

/// One flat-shaded triangle, wound so the front face points at `p_want`, shaded with that normal.
void push_tri(
		Vector3 a,
		Vector3 b,
		Vector3 c,
		const Vector3 &p_want,
		const Color &p_color,
		PackedVector3Array &r_v,
		PackedVector3Array &r_n,
		PackedVector2Array &r_uv,
		PackedColorArray &r_c
) {
	// Godot front faces are clockwise from the visible side, so wind the geometry against p_want.
	if (((b - a).cross(c - a)).dot(p_want) > 0.0f) {
		const Vector3 t = b;
		b = c;
		c = t;
	}
	const Vector3 n = p_want.normalized();
	const Vector3 tri[3] = { a, b, c };
	for (int i = 0; i < 3; ++i) {
		r_v.push_back(tri[i]);
		r_n.push_back(n);
		r_uv.push_back(Vector2(tri[i].x, tri[i].z) * 0.05f);
		r_c.push_back(p_color);
	}
}

void push_quad(
		const Vector3 &a,
		const Vector3 &b,
		const Vector3 &c,
		const Vector3 &d,
		const Vector3 &p_want,
		const Color &p_color,
		PackedVector3Array &r_v,
		PackedVector3Array &r_n,
		PackedVector2Array &r_uv,
		PackedColorArray &r_c
) {
	push_tri(a, b, c, p_want, p_color, r_v, r_n, r_uv, r_c);
	push_tri(a, c, d, p_want, p_color, r_v, r_n, r_uv, r_c);
}

/// Split a convex plan-polygon by the iso-line h = level into below (h <= level) and above; the up-to-two
/// new vertices are the marching-squares contour crossing (interpolated along the edges it cuts).
void clip_h(
		const std::vector<PV> &p_poly,
		float p_level,
		std::vector<PV> &r_below,
		std::vector<PV> &r_above,
		PV r_cut[2],
		int &r_ncut
) {
	r_below.clear();
	r_above.clear();
	r_ncut = 0;
	const int n = (int)p_poly.size();
	for (int i = 0; i < n; ++i) {
		const PV &a = p_poly[i];
		const PV &b = p_poly[(i + 1) % n];
		const bool a_in = a.h <= p_level;
		const bool b_in = b.h <= p_level;
		if (a_in) {
			r_below.push_back(a);
		} else {
			r_above.push_back(a);
		}
		if (a_in != b_in) {
			const float t = (p_level - a.h) / (b.h - a.h);
			const PV m{ a.p.lerp(b.p, t), p_level };
			r_below.push_back(m);
			r_above.push_back(m);
			if (r_ncut < 2) {
				r_cut[r_ncut] = m;
			}
			++r_ncut;
		}
	}
}

Vector2 centroid2(const std::vector<PV> &p_poly) {
	Vector2 s;
	for (const PV &v : p_poly) {
		s += v.p;
	}
	return p_poly.empty() ? s : s / (float)p_poly.size();
}

} // namespace

// Terrace one triangle by slicing it at every band boundary it crosses (marching squares within the
// triangle): each slice is a flat plate at its band floor; the interpolated cut lines are the smooth
// contours, and a vertical riser follows each. Border edges drop a skirt to `p_bottom`.
void StylizedTerrainMesh::_emit_triangle(
		Vector2 p_a,
		float p_ha,
		Vector2 p_b,
		float p_hb,
		Vector2 p_c,
		float p_hc,
		float p_bottom,
		PackedVector3Array &r_v,
		PackedVector3Array &r_n,
		PackedVector2Array &r_uv,
		PackedColorArray &r_c,
		std::vector<LevSeg> &r_segs
) const {
	const float span = MAX(_max_h - _min_h, 0.001f);
	const float hx = 0.5f * size.x;
	const float hz = 0.5f * size.y;

	auto emit_flat = [&](const std::vector<PV> &poly, int band) {
		if (poly.size() < 3) {
			return;
		}
		const float flat = base_height + (float)band * step_height;
		const float hn = (flat + step_height * 0.5f - _min_h) / span;
		const Vector2 cc = centroid2(poly);
		const Color col = _surface_color(hn, cc.x * 0.5f, cc.y * 0.5f);
		for (size_t i = 1; i + 1 < poly.size(); ++i) {
			push_tri(
					Vector3(poly[0].p.x, flat, poly[0].p.y), Vector3(poly[i].p.x, flat, poly[i].p.y),
					Vector3(poly[i + 1].p.x, flat, poly[i + 1].p.y), Vector3(0, 1, 0), col, r_v, r_n, r_uv, r_c
			);
		}
		// Skirt any polygon edge that lies on the patch border straight down to p_bottom.
		const int n = (int)poly.size();
		for (int i = 0; i < n; ++i) {
			const Vector2 &q0 = poly[i].p;
			const Vector2 &q1 = poly[(i + 1) % n].p;
			Vector3 want;
			if (Math::abs(q0.x - hx) < 0.02f && Math::abs(q1.x - hx) < 0.02f) {
				want = Vector3(1, 0, 0);
			} else if (Math::abs(q0.x + hx) < 0.02f && Math::abs(q1.x + hx) < 0.02f) {
				want = Vector3(-1, 0, 0);
			} else if (Math::abs(q0.y - hz) < 0.02f && Math::abs(q1.y - hz) < 0.02f) {
				want = Vector3(0, 0, 1);
			} else if (Math::abs(q0.y + hz) < 0.02f && Math::abs(q1.y + hz) < 0.02f) {
				want = Vector3(0, 0, -1);
			} else {
				continue;
			}
			push_quad(
					Vector3(q0.x, p_bottom, q0.y), Vector3(q0.x, flat, q0.y), Vector3(q1.x, flat, q1.y),
					Vector3(q1.x, p_bottom, q1.y), want, col.darkened(riser_darken), r_v, r_n, r_uv, r_c
			);
		}
	};

	std::vector<PV> remaining = { { p_a, p_ha }, { p_b, p_hb }, { p_c, p_hc } };
	std::vector<PV> below, above;
	PV cut[2];
	int nc = 0;
	const float mn = MIN(p_ha, MIN(p_hb, p_hc));
	const float mx = MAX(p_ha, MAX(p_hb, p_hc));
	const int kmin = (int)Math::floor((mn - base_height) / step_height);
	const int kmax = (int)Math::floor((mx - base_height) / step_height);
	for (int k = kmin + 1; k <= kmax; ++k) {
		const float y = base_height + (float)k * step_height;
		clip_h(remaining, y, below, above, cut, nc);
		emit_flat(below, k - 1);
		if (nc == 2) {
			r_segs.push_back({ k, cut[0].p, cut[1].p }); // walls extruded later from the stitched loops
		}
		remaining.swap(above);
		if (remaining.size() < 3) {
			return;
		}
	}
	emit_flat(remaining, kmax);
}

void StylizedTerrainMesh::_build_terraced(
		int p_nx,
		int p_nz,
		PackedVector3Array &r_v,
		PackedVector3Array &r_n,
		PackedVector2Array &r_uv,
		PackedColorArray &r_c
) {
	const float cx = size.x / (float)p_nx;
	const float cz = size.y / (float)p_nz;
	const float ox = -0.5f * size.x;
	const float oz = -0.5f * size.y;
	const float bottom = _min_h - 4.0f;

	// Heights at shared grid corners, so contour crossings match across cells (seamless terraces).
	std::vector<float> h((size_t)(p_nx + 1) * (p_nz + 1));
	for (int j = 0; j <= p_nz; ++j) {
		for (int i = 0; i <= p_nx; ++i) {
			h[(size_t)j * (p_nx + 1) + i] = _height_raw(ox + i * cx, oz + j * cz);
		}
	}
	auto P = [&](int i, int j) { return Vector2(ox + i * cx, oz + j * cz); };
	auto H = [&](int i, int j) { return h[(size_t)j * (p_nx + 1) + i]; };

	// Caps + border skirts per triangle; every band-boundary crossing is collected as a contour segment.
	std::vector<LevSeg> segs;
	for (int j = 0; j < p_nz; ++j) {
		for (int i = 0; i < p_nx; ++i) {
			_emit_triangle(
					P(i, j), H(i, j), P(i + 1, j), H(i + 1, j), P(i + 1, j + 1), H(i + 1, j + 1), bottom, r_v, r_n,
					r_uv, r_c, segs
			);
			_emit_triangle(
					P(i, j), H(i, j), P(i + 1, j + 1), H(i + 1, j + 1), P(i, j + 1), H(i, j + 1), bottom, r_v, r_n,
					r_uv, r_c, segs
			);
		}
	}

	// Stitch the segments into loops, extrude them into the terrace walls, and keep the loops as data.
	_build_walls(segs, r_v, r_n, r_uv, r_c);
}

namespace {

// Stitch undirected contour segments into polylines. Endpoints that coincide (they match exactly across
// shared triangle edges) are keyed by quantized position; each segment is walked once, so a closed
// contour returns to its start and a border-open contour ends at a degree-1 node.
void stitch_segments(
		const std::vector<std::pair<
				Vector2,
				Vector2>> &p_segs,
		std::vector<std::vector<Vector2>> &r_loops
) {
	auto key = [](const Vector2 &p) -> int64_t {
		const int64_t xi = (int64_t)std::llround(p.x * 1000.0f);
		const int64_t zi = (int64_t)std::llround(p.y * 1000.0f);
		return (xi << 21) ^ (zi & 0x1fffff);
	};
	struct Edge {
		int64_t a;
		int64_t b;
		bool used;
	};
	std::vector<Edge> edges;
	std::unordered_map<int64_t, Vector2> pos;
	std::unordered_map<int64_t, std::vector<int>> inc;
	edges.reserve(p_segs.size());
	for (const auto &s : p_segs) {
		const int64_t ka = key(s.first);
		const int64_t kb = key(s.second);
		if (ka == kb) {
			continue;
		}
		pos[ka] = s.first;
		pos[kb] = s.second;
		const int idx = (int)edges.size();
		edges.push_back({ ka, kb, false });
		inc[ka].push_back(idx);
		inc[kb].push_back(idx);
	}
	for (int start = 0; start < (int)edges.size(); ++start) {
		if (edges[start].used) {
			continue;
		}
		std::vector<Vector2> loop;
		int ei = start;
		int64_t cur = edges[start].a;
		loop.push_back(pos[cur]);
		while (ei >= 0 && !edges[ei].used) {
			edges[ei].used = true;
			const int64_t nxt = (edges[ei].a == cur) ? edges[ei].b : edges[ei].a;
			loop.push_back(pos[nxt]);
			cur = nxt;
			ei = -1;
			for (int cand : inc[cur]) {
				if (!edges[cand].used) {
					ei = cand;
					break;
				}
			}
		}
		if (loop.size() >= 2) {
			r_loops.push_back(loop);
		}
	}
}

} // namespace

void StylizedTerrainMesh::_build_walls(
		const std::vector<LevSeg> &p_segs,
		PackedVector3Array &r_v,
		PackedVector3Array &r_n,
		PackedVector2Array &r_uv,
		PackedColorArray &r_c
) {
	const float span = MAX(_max_h - _min_h, 0.001f);
	const float eps = MAX(cell_size * 0.4f, 0.05f);

	// Group segments by band boundary k, then stitch each group into polylines by matching endpoints.
	std::map<int, std::vector<LevSeg>> by_level;
	for (const LevSeg &s : p_segs) {
		by_level[s.k].push_back(s);
	}

	for (const auto &kv : by_level) {
		const int k = kv.first;
		const float y_hi = base_height + (float)k * step_height;
		const float y_lo = y_hi - step_height;

		std::vector<std::pair<Vector2, Vector2>> pairs;
		pairs.reserve(kv.second.size());
		for (const LevSeg &s : kv.second) {
			pairs.push_back({ s.a, s.b });
		}
		std::vector<std::vector<Vector2>> loops;
		stitch_segments(pairs, loops);

		for (const std::vector<Vector2> &loop : loops) {
			PackedVector3Array loop3;
			for (const Vector2 &p : loop) {
				loop3.push_back(Vector3(p.x, y_hi, p.y));
			}
			_contour_loops.push_back(loop3);

			// Extrude each loop edge into a vertical wall, facing the lower side of the contour.
			const int n = (int)loop.size();
			for (int i = 0; i + 1 < n; ++i) {
				const Vector2 &p0 = loop[i];
				const Vector2 &p1 = loop[i + 1];
				const Vector2 mid = (p0 + p1) * 0.5f;
				Vector2 dir = (p1 - p0);
				if (dir.length_squared() < 1e-9f) {
					continue;
				}
				dir.normalize();
				const Vector2 perp(-dir.y, dir.x);
				const float h_pos = _height_raw(mid.x + perp.x * eps, mid.y + perp.y * eps);
				const float h_neg = _height_raw(mid.x - perp.x * eps, mid.y - perp.y * eps);
				const Vector2 low = (h_pos < h_neg) ? perp : -perp; // wall faces the lower plateau
				const Color col =
						_surface_color((y_hi - _min_h) / span, mid.x * 0.5f, mid.y * 0.5f).darkened(riser_darken);
				push_quad(
						Vector3(p0.x, y_lo, p0.y), Vector3(p0.x, y_hi, p0.y), Vector3(p1.x, y_hi, p1.y),
						Vector3(p1.x, y_lo, p1.y), Vector3(low.x, 0, low.y), col, r_v, r_n, r_uv, r_c
				);
			}
		}
	}
}

Array StylizedTerrainMesh::get_contours() const {
	Array out;
	for (const PackedVector3Array &loop : _contour_loops) {
		out.push_back(loop);
	}
	return out;
}

void StylizedTerrainMesh::_build_faceted(
		int p_nx,
		int p_nz,
		PackedVector3Array &r_v,
		PackedVector3Array &r_n,
		PackedVector2Array &r_uv,
		PackedColorArray &r_c
) const {
	const float cx = size.x / (float)p_nx;
	const float cz = size.y / (float)p_nz;
	const float ox = -0.5f * size.x;
	const float oz = -0.5f * size.y;
	const float span = MAX(_max_h - _min_h, 0.001f);

	auto corner = [&](int i, int j) {
		const float wx = ox + i * cx;
		const float wz = oz + j * cz;
		return Vector3(wx, _height_raw(wx, wz), wz);
	};
	for (int j = 0; j < p_nz; ++j) {
		for (int i = 0; i < p_nx; ++i) {
			const Vector3 a = corner(i, j);
			const Vector3 b = corner(i + 1, j);
			const Vector3 c = corner(i + 1, j + 1);
			const Vector3 d = corner(i, j + 1);
			const float hnorm = ((a.y + b.y + c.y + d.y) * 0.25f - _min_h) / span;
			const Color qc = _surface_color(hnorm, (float)i, (float)j);
			push_quad(a, b, c, d, Vector3(0, 1, 0), qc, r_v, r_n, r_uv, r_c);
		}
	}
}

// ---------------------------------------------------------------------------------------------
// Build
// ---------------------------------------------------------------------------------------------

void StylizedTerrainMesh::rebuild() {
	_rebuild_queued = false;
	clear_surfaces();
	_contour_loops.clear();

	const int nx = MAX(1, (int)Math::round(size.x / cell_size));
	const int nz = MAX(1, (int)Math::round(size.y / cell_size));

	// Height range over the cell grid, for colour normalisation.
	const float cx = size.x / (float)nx;
	const float cz = size.y / (float)nz;
	const float ox = -0.5f * size.x;
	const float oz = -0.5f * size.y;
	_min_h = 1e9f;
	_max_h = -1e9f;
	for (int j = 0; j <= nz; ++j) {
		for (int i = 0; i <= nx; ++i) {
			const float h = _height_raw(ox + i * cx, oz + j * cz);
			_min_h = MIN(_min_h, h);
			_max_h = MAX(_max_h, h);
		}
	}

	PackedVector3Array vertices, normals;
	PackedVector2Array uvs;
	PackedColorArray colors;
	if (terrace) {
		_build_terraced(nx, nz, vertices, normals, uvs, colors);
	} else {
		_build_faceted(nx, nz, vertices, normals, uvs, colors);
	}

	if (vertices.size() >= 3) {
		Array arrays;
		arrays.resize(Mesh::ARRAY_MAX);
		arrays[Mesh::ARRAY_VERTEX] = vertices;
		arrays[Mesh::ARRAY_NORMAL] = normals;
		arrays[Mesh::ARRAY_TEX_UV] = uvs;
		arrays[Mesh::ARRAY_COLOR] = colors;
		add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, arrays);
	}
	emit_changed();
}

} // namespace godot
