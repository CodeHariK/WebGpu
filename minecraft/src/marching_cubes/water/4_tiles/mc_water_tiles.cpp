// MCGrid: the water mesh from baked water tiles, assembled like the terrain's chunks.
//
// A cell's water shape depends only on how its 4 corners relate to the water body (and the style
// and sample rate), never on the actual heights:
//   BANK   ground at or above the water level          (the water ends under it)
//   WET    one of the body's own corners
//   LOWER  ground below the body's bed                 (the water falls down there)
//   DRY    dry ground of the bed's tier                (a gap: the water spills onto it)
// 4 classes x 4 corners = 256 tiles per style and rate, each baked once, the first time it is needed
// (bake_tile: the fine sample grid + marching squares on the cliff tiles' shape, in cell-local
// coordinates, at height 0). Then every rebuild only places tiles: each body of each cell looks
// up its tile, lifts the surface to its height and stretches the falls down to where they land.
// Vertices are welded by position while assembling (the water's edge on a cell side depends only on
// that side's two corners, so neighbouring tiles put the same points there): one indexed mesh with
// shared vertices, no seams.
#include <chrono>
#define CDT_DISABLE_EXCEPTIONS // godot-cpp builds without exceptions
#include "utils/cdt/CDT.h"
#include "marching_cubes/terrain/mc_grid.h"
#include "marching_cubes/water/4_tiles/mc_water_tile.h"
#include <godot_cpp/templates/hash_map.hpp>
#include <cmath>
#include <algorithm>
#include <unordered_map>

namespace godot {

using namespace mc_water;

namespace {
const float PROBE = 0.03f; // a fall looks this far outside its contour to see what is there
const int BISECT = 12; // steps when locating a contour crossing on a square's edge
const float WELD = 4096.0f; // vertices closer than 1 / WELD metres are welded
const float SHARP = 0.95f; // edge normals turning more than this (cos) inside a square: a sharp corner

std::unordered_map<uint64_t, WaterTile> tile_cache; // shared by every grid: tiles depend on the key only


// The tile's shape: style (rounded / blocky / square), sample rate, fall lip and the 4 corner classes
uint64_t shape_key(
		const Body &p_body,
		int p_rate
) {
	uint32_t style = !p_body.above.blocky ? 0 : (p_body.above.rounding > 0.0f ? 1 : 2);
	uint32_t lip = static_cast<uint32_t>(std::lround(p_body.fall_lip * 100.0f)); // 0 .. 40
	uint64_t key = (static_cast<uint64_t>(lip) << 24) | (style << 16) | (static_cast<uint32_t>(p_rate) << 8);
	for (int k = 0; k < 4; k++) {
		key |= static_cast<uint64_t>(corner_class(p_body, k)) << (2 * k);
	}
	return key;
}

// The water shape's outward normal at p (the field grows into the water).
Vector2 outward(
		const Body &p_body,
		const Vector2 &p
) {
	const float e = 1e-3f;
	Vector2 g((p_body.water_field(p + Vector2(e, 0)) - p_body.water_field(p - Vector2(e, 0))) / (2 * e),
			(p_body.water_field(p + Vector2(0, e)) - p_body.water_field(p - Vector2(0, e))) / (2 * e));
	return g.length_squared() < 1e-12f ? Vector2() : -g.normalized();
}

// A sharp corner of the water's edge inside the square [p_lo, p_hi] (extended marching squares):
// the edge crosses the square at p_a and p_b with normals turning sharply, so the true corner is
// where the two tangent lines meet. Plain marching squares would cut it off diagonally (up to one
// sample), showing the rock's corner as a thin post beside the falls. False if the edge is smooth.
bool sharp_corner(
		const Body &p_body,
		const Vector2 &p_a,
		const Vector2 &p_b,
		const Vector2 &p_lo,
		const Vector2 &p_hi,
		Vector2 &r_corner
) {
	Vector2 na = outward(p_body, p_a);
	Vector2 nb = outward(p_body, p_b);
	if (na == Vector2() || nb == Vector2() || na.dot(nb) > SHARP) {
		return false;
	}
	// p_a + t * ta = p_b + u * tb, with the tangents ta = perp(na), tb = perp(nb)
	Vector2 ta(-na.y, na.x);
	Vector2 tb(-nb.y, nb.x);
	float det = ta.cross(tb);
	if (std::abs(det) < 1e-6f) {
		return false;
	}
	float t = (p_b - p_a).cross(tb) / det;
	Vector2 x = p_a + ta * t;
	const float slack = 1e-4f;
	if (x.x < p_lo.x - slack || x.y < p_lo.y - slack || x.x > p_hi.x + slack || x.y > p_hi.y + slack) {
		return false;
	}
	r_corner = x.clamp(p_lo, p_hi);
	return true;
}

// Bake a tile: sample the body's water shape on a (rate + 1)^2 grid over the cell, run marching
// squares (crossings bisected on the exact shape), and find the falls: each contour segment looks
// just outside itself — a bank: nothing; dry ground of the bed's tier: a fall onto it; below the
// bed: a fall down to where the body's water lands.
WaterTile bake_contour(
		const Body &p_body,
		int p_rate
) {
	WaterTile tile;
	const int n = p_rate + 1;
	const float step = 1.0f / static_cast<float>(p_rate);
	auto point = [&](int i, int j) { return Vector2(-0.5f + static_cast<float>(i) * step, -0.5f + static_cast<float>(j) * step); };
	std::vector<uint8_t> in(static_cast<size_t>(n * n));
	bool all = true;
	for (int j = 0; j < n; j++) {
		for (int i = 0; i < n; i++) {
			bool wet = p_body.water_field(point(i, j)) > TILE_INSIDE;
			in[static_cast<size_t>(j * n + i)] = wet ? 1 : 0;
			all = all && wet;
			if (wet) {
				tile.wet_samples.push_back(point(i, j));
			}
		}
	}
	if (all) {
		tile.whole = true;
		return tile;
	}
	auto crossing = [&](const Vector2 &p_in, const Vector2 &p_out) {
		float lo = 0.0f; // in
		float hi = 1.0f; // out
		for (int s = 0; s < BISECT; s++) {
			float t = (lo + hi) * 0.5f;
			if (p_body.water_field(p_in.lerp(p_out, t)) > TILE_INSIDE) {
				lo = t;
			} else {
				hi = t;
			}
		}
		return p_in.lerp(p_out, (lo + hi) * 0.5f);
	};
	// the crossing on a square's edge, always bisected in the same direction (lower grid index
	// first) so the two squares sharing the edge get the very same point
	auto edge_point = [&](int ia, int ja, int ib, int jb) {
		bool swap = jb * n + ib < ja * n + ia;
		if (swap) {
			std::swap(ia, ib);
			std::swap(ja, jb);
		}
		bool a_in = in[static_cast<size_t>(ja * n + ia)] != 0;
		return a_in ? crossing(point(ia, ja), point(ib, jb)) : crossing(point(ib, jb), point(ia, ja));
	};
	// a fall along a -> b facing p_out (unit)
	auto add_fall_facing = [&](const Vector2 &p_a, const Vector2 &p_b, const Vector2 &p_out) {
		if (p_a.distance_squared_to(p_b) < 1e-10f || p_out == Vector2()) {
			return;
		}
		Vector2 mid = (p_a + p_b) * 0.5f;
		Vector2 out = p_out;
		Vector2 q = (mid + out * PROBE).clamp(Vector2(-0.5f, -0.5f), Vector2(0.5f, 0.5f));
		if (p_body.above_at(q) >= 0.0f) {
			tile.shores.push_back({ p_a, p_b, out }); // a bank: no fall, a shore
			return;
		}
		WaterTile::Fall fall;
		fall.a = p_a;
		fall.b = p_b;
		fall.out = out;
		fall.to_dry = p_body.bed_at(q) >= 0.0f;
		tile.falls.push_back(fall);
	};
	// a fall along a -> b facing away from the square's corner(s) outside the water
	auto add_fall = [&](const Vector2 &p_a, const Vector2 &p_b, const Vector2 &p_out_corner) {
		Vector2 out = p_out_corner - (p_a + p_b) * 0.5f;
		if (out.length_squared() >= 1e-12f) {
			add_fall_facing(p_a, p_b, out.normalized());
		}
	};
	for (int j = 0; j < p_rate; j++) {
		for (int i = 0; i < p_rate; i++) {
			// corners counter-clockwise: (i, j) (i+1, j) (i+1, j+1) (i, j+1)
			const int ci[4] = { i, i + 1, i + 1, i };
			const int cj[4] = { j, j, j + 1, j + 1 };
			bool c[4];
			int count = 0;
			for (int m = 0; m < 4; m++) {
				c[m] = in[static_cast<size_t>(cj[m] * n + ci[m])] != 0;
				count += c[m] ? 1 : 0;
			}
			if (count == 0) {
				continue;
			}
			auto corner = [&](int m) { return point(ci[m], cj[m]); };
			auto cut = [&](int m) {
				int k = (m + 1) % 4;
				return edge_point(ci[m], cj[m], ci[k], cj[k]);
			};
			if (count == 4) {
				for (int m : { 0, 1, 2, 0, 2, 3 }) {
					tile.surface.push_back(corner(m));
				}
				continue;
			}
			bool saddle = c[0] == c[2] && c[1] == c[3] && c[0] != c[1];
			if (saddle) {
				for (int m = 0; m < 4; m++) {
					if (!c[m]) {
						continue;
					}
					Vector2 e0 = cut((m + 3) % 4);
					Vector2 e1 = cut(m);
					tile.surface.push_back(corner(m));
					tile.surface.push_back(e1);
					tile.surface.push_back(e0);
					tile.rim.push_back(e0);
					tile.rim.push_back(e1);
					add_fall(e0, e1, corner((m + 2) % 4));
				}
				continue;
			}
			std::vector<Vector2> polygon;
			std::vector<Vector2> edge;
			Vector2 out_sum;
			int outs = 0;
			for (int m = 0; m < 4; m++) {
				if (c[m]) {
					polygon.push_back(corner(m));
				} else {
					out_sum += corner(m);
					outs++;
				}
				if (c[m] != c[(m + 1) % 4]) {
					Vector2 e = cut(m);
					polygon.push_back(e);
					edge.push_back(e);
				}
			}
			// a sharp corner between the two crossings: put it in the outline (between them, where the
			// outside part is) and bend the fall around it
			Vector2 sharp;
			bool is_sharp = edge.size() == 2 && sharp_corner(p_body, edge[0], edge[1], corner(0), corner(2), sharp);
			if (is_sharp) {
				std::vector<Vector2> bent = polygon;
				for (size_t t = 0; t < polygon.size(); t++) {
					const Vector2 &u = polygon[t];
					const Vector2 &w = polygon[(t + 1) % polygon.size()];
					bool pair = (u == edge[0] && w == edge[1]) || (u == edge[1] && w == edge[0]);
					if (pair) {
						bent.insert(bent.begin() + static_cast<long>(t + 1), sharp);
						break;
					}
				}
				// keep it only if the fan stays one way round (no folded triangles)
				float sign = 0.0f;
				bool folded = bent.size() == polygon.size();
				for (size_t t = 1; !folded && t + 1 < bent.size(); t++) {
					float area = (bent[t] - bent[0]).cross(bent[t + 1] - bent[0]);
					if (std::abs(area) < 1e-9f) {
						continue;
					}
					if (sign == 0.0f) {
						sign = area;
					} else if ((area > 0) != (sign > 0)) {
						folded = true;
					}
				}
				if (folded) {
					is_sharp = false;
				} else {
					polygon = bent;
				}
			}
			for (size_t t = 1; t + 1 < polygon.size(); t++) {
				tile.surface.push_back(polygon[0]);
				tile.surface.push_back(polygon[t]);
				tile.surface.push_back(polygon[t + 1]);
			}
			if (edge.size() == 2) { // the rim through this square (bent round a sharp corner)
				if (is_sharp) {
					tile.rim.insert(tile.rim.end(), { edge[0], sharp, sharp, edge[1] });
				} else {
					tile.rim.insert(tile.rim.end(), { edge[0], edge[1] });
				}
			}
			if (edge.size() == 2 && outs > 0) {
				if (is_sharp) {
					add_fall_facing(edge[0], sharp, outward(p_body, edge[0]));
					add_fall_facing(sharp, edge[1], outward(p_body, edge[1]));
				} else {
					add_fall(edge[0], edge[1], out_sum / static_cast<float>(outs));
				}
			}
		}
	}
	return tile;
}

Vector3i weld_key(const Vector3 &p_v);

const float INNER = 0.25f; // m: the inner points' spacing, and the side points' (the sample grid only finds the edge)
const float INNER_CLEAR = 0.06f; // m: inner points keep this far from the rim (no slivers)
const float SIMPLIFY = 0.008f; // m: rim points closer than this to the line through their neighbours are dropped

// The rim's segments joined into polylines (by their shared end points: neighbouring squares'
// crossings are the very same points), each simplified: a point is dropped while the polyline
// stays within SIMPLIFY of it (Douglas-Peucker); the ends (on the cell's sides) always stay.
std::vector<std::vector<Vector2>> simplified_rim(const std::vector<Vector2> &p_rim) {
	std::vector<std::vector<Vector2>> lines;
	std::vector<uint8_t> used(p_rim.size() / 2, 0);
	auto take_from = [&](const Vector2 &p_end, Vector2 &r_next) {
		for (size_t k = 0; k < used.size(); k++) {
			if (used[k]) {
				continue;
			}
			if (p_rim[2 * k].distance_squared_to(p_end) < 1e-12f) {
				used[k] = 1;
				r_next = p_rim[2 * k + 1];
				return true;
			}
			if (p_rim[2 * k + 1].distance_squared_to(p_end) < 1e-12f) {
				used[k] = 1;
				r_next = p_rim[2 * k];
				return true;
			}
		}
		return false;
	};
	for (size_t k = 0; k < used.size(); k++) {
		if (used[k] || p_rim[2 * k].distance_squared_to(p_rim[2 * k + 1]) < 1e-12f) {
			continue;
		}
		used[k] = 1;
		std::vector<Vector2> line = { p_rim[2 * k], p_rim[2 * k + 1] };
		Vector2 next;
		while (take_from(line.back(), next)) {
			line.push_back(next);
		}
		while (take_from(line.front(), next)) {
			line.insert(line.begin(), next);
		}
		// Douglas-Peucker
		std::vector<uint8_t> keep(line.size(), 0);
		keep.front() = keep.back() = 1;
		std::vector<std::pair<size_t, size_t>> spans = { { 0, line.size() - 1 } };
		while (!spans.empty()) {
			auto [a, b] = spans.back();
			spans.pop_back();
			float worst = 0.0f;
			size_t at = a;
			Vector2 ab = line[b] - line[a];
			for (size_t i = a + 1; i < b; i++) {
				float u = ab.length_squared() > 1e-12f ? Math::clamp((line[i] - line[a]).dot(ab) / ab.length_squared(), 0.0f, 1.0f) : 0.0f;
				float d = line[i].distance_to(line[a] + ab * u);
				if (d > worst) {
					worst = d;
					at = i;
				}
			}
			if (worst > SIMPLIFY) {
				keep[at] = 1;
				spans.push_back({ a, at });
				spans.push_back({ at, b });
			}
		}
		std::vector<Vector2> kept;
		for (size_t i = 0; i < line.size(); i++) {
			if (keep[i]) {
				kept.push_back(line[i]);
			}
		}
		lines.push_back(kept);
	}
	return lines;
}

// The tile's surface again, as a constrained Delaunay triangulation (CDT) of: its rim (the edge
// pieces joined and simplified, kept as constraint edges so the triangles follow it), the cell
// corners and side points every INNER (the same in every tile, so neighbours still weld on their
// shared sides), and a coarse grid INNER apart inside the water. A triangle is kept when its middle is water. Far
// fewer, larger triangles than one or two per sample square; the edge is the same.
void triangulate(
		const Body &p_body,
		int p_rate,
		WaterTile &r_tile
) {
	if (r_tile.whole || r_tile.surface.empty()) {
		return;
	}
	std::vector<CDT::V2d<double>> points;
	std::vector<CDT::Edge> edges;
	auto add_point = [&](const Vector2 &p) {
		points.push_back(CDT::V2d<double>(p.x, p.y));
		return static_cast<CDT::VertInd>(points.size() - 1);
	};
	// the rim, simplified, as constraint edges
	std::vector<Vector2> rim; // the kept rim segments (the inner points keep clear of them)
	for (const std::vector<Vector2> &line : simplified_rim(r_tile.rim)) {
		for (size_t i = 0; i + 1 < line.size(); i++) {
			CDT::VertInd a = add_point(line[i]);
			CDT::VertInd b = add_point(line[i + 1]);
			edges.push_back(CDT::Edge(a, b));
			rim.insert(rim.end(), { line[i], line[i + 1] });
		}
	}
	// the surface's points on the cell's sides: its corners, the rim's crossings and every
	// INNER-th sample point there (neighbouring tiles keep the same ones, so the vertices weld)
	int stride = std::max(1, static_cast<int>(std::lround(INNER * static_cast<float>(p_rate))));
	float step = 1.0f / static_cast<float>(p_rate);
	auto on_grid = [&](float p_coordinate) { // a sample point every stride along a side
		float index = (p_coordinate + 0.5f) * static_cast<float>(p_rate);
		int nearest = static_cast<int>(std::lround(index));
		return std::abs(index - static_cast<float>(nearest)) < 1e-4f ? nearest : -1;
	};
	// which sides the rim crosses: only those get the side points between the corners. A side the
	// water covers whole keeps just its corners, as a whole tile's quad does (its neighbour across
	// that side sees the same whole side), so the two meet vertex to vertex (no T-junction)
	auto side_of = [](const Vector2 &p) { // 0 x = -0.5, 1 x = +0.5, 2 y = -0.5, 3 y = +0.5; -1: none
		if (std::abs(std::abs(p.x) - 0.5f) < 1e-6f) {
			return p.x < 0.0f ? 0 : 1;
		}
		if (std::abs(std::abs(p.y) - 0.5f) < 1e-6f) {
			return p.y < 0.0f ? 2 : 3;
		}
		return -1;
	};
	bool crossed[4] = { false, false, false, false };
	for (const Vector2 &p : r_tile.surface) {
		bool side_x = std::abs(std::abs(p.x) - 0.5f) < 1e-6f;
		bool side_y = std::abs(std::abs(p.y) - 0.5f) < 1e-6f;
		if (side_x != side_y && on_grid(side_x ? p.y : p.x) < 0) { // on one side, not a sample point
			crossed[side_of(p)] = true;
		}
	}
	for (const Vector2 &p : r_tile.surface) {
		bool side_x = std::abs(std::abs(p.x) - 0.5f) < 1e-6f; // on a side where x is fixed: along y
		bool side_y = std::abs(std::abs(p.y) - 0.5f) < 1e-6f;
		if (!side_x && !side_y) {
			continue;
		}
		int along = on_grid(side_x ? p.y : p.x);
		bool corner = side_x && side_y;
		bool crossing = along < 0; // not a sample point: where the rim meets the side
		if (corner || crossing || (along % stride == 0 && crossed[side_of(p)])) {
			add_point(p);
		}
	}
	// a coarse grid inside, clear of the rim
	for (int j = stride; j < p_rate; j += stride) {
		for (int i = stride; i < p_rate; i += stride) {
			Vector2 p(-0.5f + static_cast<float>(i) * step, -0.5f + static_cast<float>(j) * step);
			if (p_body.water_field(p) <= TILE_INSIDE) {
				continue;
			}
			bool clear = true;
			for (size_t k = 0; k + 1 < rim.size() && clear; k += 2) {
				const Vector2 &a = rim[k];
				Vector2 ab = rim[k + 1] - a;
				float u = ab.length_squared() > 1e-12f ? Math::clamp((p - a).dot(ab) / ab.length_squared(), 0.0f, 1.0f) : 0.0f;
				clear = p.distance_squared_to(a + ab * u) >= INNER_CLEAR * INNER_CLEAR;
			}
			if (clear) {
				add_point(p);
			}
		}
	}
	CDT::RemoveDuplicatesAndRemapEdges(points, edges);
	CDT::Triangulation<double> cdt(CDT::VertexInsertionOrder::Auto, CDT::IntersectingConstraintEdges::TryResolve, 1e-7);
	cdt.insertVertices(points);
	cdt.insertEdges(edges);
	cdt.eraseSuperTriangle();
	std::vector<Vector2> surface;
	for (const CDT::Triangle &tri : cdt.triangles) {
		Vector2 v[3];
		for (int k = 0; k < 3; k++) {
			const CDT::V2d<double> &q = cdt.vertices[tri.vertices[static_cast<size_t>(k)]];
			v[k] = Vector2(static_cast<float>(q.x), static_cast<float>(q.y));
		}
		Vector2 middle = (v[0] + v[1] + v[2]) / 3.0f;
		if (p_body.water_field(middle) > TILE_INSIDE) {
			surface.insert(surface.end(), { v[0], v[1], v[2] });
		}
	}
	if (!surface.empty()) {
		r_tile.surface = surface;
	}
}

} // namespace

namespace mc_water {

CornerClass corner_class(
		const Body &p_body,
		int p_corner
) {
	if (p_body.above.solid[p_corner]) {
		return BANK;
	}
	if (!p_body.bed.solid[p_corner]) {
		return LOWER;
	}
	return p_body.wet.solid[p_corner] ? WET : DRY;
}

uint64_t tile_key(
		const Body &p_body,
		int p_rate
) {
	return shape_key(p_body, p_rate) ^ (p_body.baked_key * 0xC2B2AE3D27D4EB4FULL); // + the baked banks
}

// A tile: the falls (and shores) on the water's real edge, the fall line at the cliff; the surface
// from the shape trimmed back by the bevel (Body::fall_lip): the fall strip's flat part and bevel
// (mc_water_fall_strips.cpp) cover from there out to the fall line.
WaterTile bake_tile(
		const Body &p_body,
		int p_rate
) {
	Body edge = p_body;
	edge.fall_lip = 0.0f;
	WaterTile tile = bake_contour(edge, p_rate);
	if (p_body.fall_lip <= 0.0f) {
		triangulate(p_body, p_rate, tile);
		return tile;
	}
	WaterTile trimmed = bake_contour(p_body, p_rate);
	trimmed.falls = tile.falls;
	trimmed.shores = tile.shores;
	triangulate(p_body, p_rate, trimmed);
	return trimmed;
}

const WaterTile *cached_tile(uint64_t p_key) {
	auto found = tile_cache.find(p_key);
	return found == tile_cache.end() ? nullptr : &found->second;
}

void clear_tile_cache() { tile_cache.clear(); }

} // namespace mc_water

namespace {
Vector3i weld_key(const Vector3 &p_v) {
	return Vector3i(static_cast<int>(std::lround(p_v.x * WELD)), static_cast<int>(std::lround(p_v.y * WELD)),
			static_cast<int>(std::lround(p_v.z * WELD)));
}
} // namespace


void MCGrid::set_water_sample_rate(int p_rate) {
	water_sample_rate = std::clamp(p_rate, 2, 32);
	clear_tile_cache(); // tiles of the old rate aren't used again
	if (!chunks.empty() && water_levels.size() == column_heights.size()) {
		_queue_water();
	}
}

// Place every body's tile (baking new ones as needed) into one welded, indexed water mesh.
void MCGrid::_emit_water_tiles(
		const std::vector<float> &p_surface,
		const std::vector<uint8_t> &p_held,
		const std::vector<Color> &p_flow,
		MCWaterArrays &r_out
) const {
	Vector2i columns = _column_counts();
	Vector2i cells(columns.x - 1, columns.y - 1);
	HashMap<Vector3i, int> surface_index;
	std::vector<Body> bodies;
	std::vector<float> bottoms;
	std::vector<PlacedFall> placed_falls; // emitted at the end, once every fall is known
	const bool stage_edges = debug_water && water_debug_stage == 3; // the edge pieces (debug stage 3)
	water_bodies_nsec = 0;
	for (int cz = 0; cz < cells.y; cz++) {
		for (int cx = 0; cx < cells.x; cx++) {
			auto bodies_started = std::chrono::steady_clock::now(); // debug timing (step 3 vs step 4)
			_water_bodies(cx, cz, p_surface, p_held, p_flow, bodies, bottoms);
			water_bodies_nsec += static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
					std::chrono::steady_clock::now() - bodies_started).count());
			Vector2 centre(static_cast<float>(cx) + 0.5f, static_cast<float>(cz) + 0.5f);
			auto world = [&](const Vector2 &p, float y) { return Vector3(centre.x + p.x, y, centre.y - p.y); }; // z = -y
			for (size_t k = 0; k < bodies.size(); k++) {
				const Body &body = bodies[k];
				// the tile, from the cache (per-mesh banks: their slices' key is part of the key)
				uint64_t key = tile_key(body, water_sample_rate);
				auto found = tile_cache.find(key);
				if (found == tile_cache.end()) {
					found = tile_cache.emplace(key, bake_tile(body, water_sample_rate)).first;
				}
				const WaterTile &tile = found->second;
				// the surface, welded
				auto surface_vertex = [&](const Vector2 &p) {
					Vector3 v = world(p, body.top);
					Vector3i weld = weld_key(v);
					if (const int *hit = surface_index.getptr(weld)) {
						r_out.indices.push_back(*hit);
						return;
					}
					int index = static_cast<int>(r_out.vertices.size());
					r_out.vertices.push_back(v);
					r_out.normals.push_back(Vector3(0, 1, 0));
					r_out.uvs.push_back(Vector2(v.x, v.z));
					r_out.uv2s.push_back(Vector2(0.0f, body.stream ? -1.0f : 0.0f)); // the liquid shaders' UV2
					r_out.colors.push_back(flow_at(body.flow, p.x, p.y));
					r_out.depths.push_back(std::max(body.top - body.ground_at(p), 0.0f));
					surface_index.insert(weld, index);
					r_out.indices.push_back(index);
				};
				if (tile.whole) {
					r_out.begin_piece(); // this body's flat tile: one piece ("pieces" view)
					const Vector2 q[4] = { Vector2(-0.5f, -0.5f), Vector2(0.5f, -0.5f), Vector2(0.5f, 0.5f), Vector2(-0.5f, 0.5f) };
					for (int m : { 0, 1, 2, 0, 2, 3 }) {
						surface_vertex(q[m]);
					}
				} else {
					r_out.begin_piece();
					for (const Vector2 &p : tile.surface) {
						surface_vertex(p);
					}
				}
				// the falls: down to where the body lands, or onto the dry ground of its bed tier
				float dry_floor = body.top;
				for (int m = 0; m < 4; m++) {
					if (corner_class(body, m) == DRY) {
						dry_floor = std::min(dry_floor, static_cast<float>(body.ground[m]) - 0.5f);
					}
				}
				for (const WaterTile::Fall &fall : tile.falls) {
					float bottom = fall.to_dry ? dry_floor : bottoms[k];
					if (bottom < -1e5f || bottom > body.top - 0.02f) {
						continue;
					}
					PlacedFall placed;
					placed.ta = world(fall.a, body.top);
					placed.tb = world(fall.b, body.top);
					placed.ba = world(fall.a, bottom);
					placed.bb = world(fall.b, bottom);
					placed.out = Vector3(fall.out.x, 0, -fall.out.y);
					placed.drop = body.top - bottom;
					// the surface's flow where the strip starts (back over the surface), so it matches
					auto start_flow = [&](const Vector2 &p_top) {
						Vector2 q = (p_top - fall.out * (water_fall_lip + 0.1f)).clamp(Vector2(-0.5f, -0.5f), Vector2(0.5f, 0.5f));
						return flow_at(body.flow, q.x, q.y);
					};
					placed.flow_a = start_flow(fall.a);
					placed.flow_b = start_flow(fall.b);
					placed.stream = body.stream ? 1.0f : 0.0f;
					// the surface's depth where the strip starts, so its flat part and bevel match it
					auto start_depth = [&](const Vector2 &p_top) {
						Vector2 q = (p_top - fall.out * (water_fall_lip + 0.1f)).clamp(Vector2(-0.5f, -0.5f), Vector2(0.5f, 0.5f));
						return std::max(body.top - body.ground_at(q), 0.0f);
					};
					placed.depth_a = start_depth(fall.a);
					placed.depth_b = start_depth(fall.b);
					placed_falls.push_back(placed);
				}
				// the edge pieces, for water debug stage 3 only, oriented with the water on the left
				// seen from above (out points away from the water: a -> b turned left is the water side)
				auto add_edge = [&](const Vector2 &p_a, const Vector2 &p_b, const Vector2 &p_out, WaterEdge::Kind p_kind) {
					WaterEdge edge;
					edge.a = world(p_a, body.top);
					edge.b = world(p_b, body.top);
					edge.kind = p_kind;
					Vector3 along = edge.b - edge.a;
					Vector3 out(p_out.x, 0, -p_out.y);
					// right of a -> b seen from above (y up): (-along.z, 0, along.x) is left
					if (Vector3(-along.z, 0, along.x).dot(out) > 0.0f) {
						std::swap(edge.a, edge.b); // the water side must be the left
					}
					r_out.edges.push_back(edge);
				};
				if (stage_edges) {
					for (const WaterTile::Shore &shore : tile.shores) {
						add_edge(shore.a, shore.b, shore.out, WaterEdge::BANK);
					}
					for (const WaterTile::Fall &fall : tile.falls) {
						add_edge(fall.a, fall.b, fall.out, WaterEdge::FALL);
					}
				}
				if (debug_water) {
					for (const Vector2 &p : tile.wet_samples) {
						r_out.samples.push_back(world(p, body.top));
					}
				}
			}
		}
	}
	_join_fall_ends(placed_falls); // neighbouring cells' ends that just miss each other (mc_water_fall_strips.cpp)
	_relax_fall_chains(placed_falls); // the fall lines smoothed across cells (no V at every cell border)
	// each fall's ends: direction and bevel radius (mc_water_fall_strips.cpp): the cut and the strips agree
	std::vector<FallEnds> ends = _fall_ends(placed_falls);
	_cut_surface_under_falls(placed_falls, ends, r_out); // no flat water under the bevels (mc_water_surface_cut.cpp)
	_emit_fall_strips(placed_falls, ends, r_out); // bevelled L strips (mc_water_fall_strips.cpp)
}

} // namespace godot
