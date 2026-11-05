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
#include "marching_cubes/grid/mc_grid.h"
#include "marching_cubes/grid/mc_water_shape.h"
#include <godot_cpp/templates/hash_map.hpp>
#include <cmath>
#include <algorithm>
#include <unordered_map>

namespace godot {

using namespace mc_water;

namespace {
const float INSIDE = 1e-4f; // the shape value a point needs to count as water
const float PROBE = 0.03f; // a fall looks this far outside its contour to see what is there
const int BISECT = 12; // steps when locating a contour crossing on a square's edge
const float WELD = 4096.0f; // vertices closer than 1 / WELD metres are welded
const float SHARP = 0.95f; // edge normals turning more than this (cos) inside a square: a sharp corner

enum CornerClass : uint32_t {
	BANK = 0,
	WET = 1,
	LOWER = 2,
	DRY = 3,
};

// One baked tile: the surface triangles and the falls, in cell-local shape coordinates.
struct WaterTile {
	bool whole = false; // the body covers the cell: one quad
	std::vector<Vector2> surface; // triangles, 3 points each
	struct Fall {
		Vector2 a;
		Vector2 b;
		Vector2 out; // faces down the step (shape coordinates)
		bool to_dry = false; // lands on dry ground of the bed's tier (else: below the bed)
	};
	std::vector<Fall> falls;
	struct Shore { // where the surface meets a bank
		Vector2 a;
		Vector2 b;
		Vector2 out; // towards the bank
	};
	std::vector<Shore> shores;
	std::vector<Vector2> wet_samples; // the debug overlay's dots
};

std::unordered_map<uint64_t, WaterTile> tile_cache; // shared by every grid: tiles depend on the key only

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

// style (rounded / blocky / square), sample rate, grow, fall lip, spill radius and the 4 corner classes
uint64_t tile_key(
		const Body &p_body,
		int p_rate
) {
	uint32_t style = !p_body.above.blocky ? 0 : (p_body.above.rounding > 0.0f ? 1 : 2);
	uint32_t grow = static_cast<uint32_t>(std::lround(p_body.grow * 200.0f)); // 0 .. 60
	uint32_t lip = static_cast<uint32_t>(std::lround(p_body.fall_lip * 100.0f)); // 0 .. 40
	uint64_t spill = p_body.spill_mask ? static_cast<uint64_t>(std::lround(p_body.spill * 100.0f)) : 0; // 0 .. 100
	uint64_t key = (static_cast<uint64_t>(p_body.spill_mask) << 40) | (spill << 32) | (lip << 24) | (grow << 18) | (style << 16) | (static_cast<uint32_t>(p_rate) << 8);
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
			bool wet = p_body.water_field(point(i, j)) > INSIDE;
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
			if (p_body.water_field(p_in.lerp(p_out, t)) > INSIDE) {
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
		if (blend(p_body.above, q.x, q.y) >= 0.0f) {
			tile.shores.push_back({ p_a, p_b, out }); // a bank: no fall, a shore (foam particles)
			return;
		}
		WaterTile::Fall fall;
		fall.a = p_a;
		fall.b = p_b;
		fall.out = out;
		fall.to_dry = blend(p_body.bed, q.x, q.y) >= 0.0f;
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

// A tile: the falls (and shores) on the water's real edge, the fall line at the cliff; the surface
// from the shape trimmed back by the bevel (Body::fall_lip): the fall strip's flat part and bevel
// (mc_grid_water_falls.cpp) cover from there out to the fall line.
WaterTile bake_tile(
		const Body &p_body,
		int p_rate
) {
	Body edge = p_body;
	edge.fall_lip = 0.0f;
	WaterTile tile = bake_contour(edge, p_rate);
	if (p_body.fall_lip <= 0.0f) {
		return tile;
	}
	WaterTile trimmed = bake_contour(p_body, p_rate);
	trimmed.falls = tile.falls;
	trimmed.shores = tile.shores;
	return trimmed;
}

} // namespace

// Points every MCWaterArrays::PARTICLE_SPACING along a -> b (at least one, the middle), each with
// p_direction: particle emission points.
void MCGrid::_sample_line(
		const Vector3 &p_a,
		const Vector3 &p_b,
		const Vector3 &p_direction,
		std::vector<Vector3> &r_points,
		std::vector<Vector3> &r_directions
) {
	float length = p_a.distance_to(p_b);
	int count = std::max(1, static_cast<int>(std::ceil(length / MCWaterArrays::PARTICLE_SPACING)));
	for (int i = 0; i < count; i++) {
		float t = (static_cast<float>(i) + 0.5f) / static_cast<float>(count);
		r_points.push_back(p_a.lerp(p_b, t));
		r_directions.push_back(p_direction);
	}
}

// water_expand along the falls: a fall's free end (one no other fall continues from, where it meets a
// bank or dry ground) reaches on by p_grow along the fall, top and foot together.
void MCGrid::_extend_fall_ends(
		std::vector<PlacedFall> &r_falls,
		float p_grow
) {
	if (p_grow <= 0.0f) {
		return;
	}
	HashMap<Vector3i, int> ends; // top end -> how many falls end there
	for (const PlacedFall &f : r_falls) {
		for (const Vector3 &v : { f.ta, f.tb }) {
			int *n = ends.getptr(weld_key(v));
			if (n) {
				(*n)++;
			} else {
				ends.insert(weld_key(v), 1);
			}
		}
	}
	for (PlacedFall &f : r_falls) {
		Vector3 along = f.tb - f.ta;
		if (along.length_squared() < 1e-10f) {
			continue;
		}
		along = along.normalized() * p_grow;
		if (ends[weld_key(f.ta)] == 1) {
			f.ta -= along;
			f.ba -= along;
		}
		if (ends[weld_key(f.tb)] == 1) {
			f.tb += along;
			f.bb += along;
		}
	}
}

namespace {
Vector3i weld_key(const Vector3 &p_v) {
	return Vector3i(static_cast<int>(std::lround(p_v.x * WELD)), static_cast<int>(std::lround(p_v.y * WELD)),
			static_cast<int>(std::lround(p_v.z * WELD)));
}
} // namespace

void MCGrid::set_water_expand(float p_expand) {
	water_expand = std::clamp(p_expand, 0.0f, 0.3f);
	if (!chunks.empty() && water_levels.size() == column_heights.size()) {
		_queue_water();
	}
}

void MCGrid::set_water_sampled(bool p_on) {
	water_sampled = p_on;
	if (!chunks.empty() && water_levels.size() == column_heights.size()) {
		_queue_water();
	}
}

void MCGrid::set_water_sample_rate(int p_rate) {
	water_sample_rate = std::clamp(p_rate, 2, 32);
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
	for (int cz = 0; cz < cells.y; cz++) {
		for (int cx = 0; cx < cells.x; cx++) {
			_water_bodies(cx, cz, p_surface, p_held, p_flow, bodies, bottoms);
			Vector2 centre(static_cast<float>(cx) + 0.5f, static_cast<float>(cz) + 0.5f);
			auto world = [&](const Vector2 &p, float y) { return Vector3(centre.x + p.x, y, centre.y - p.y); }; // z = -y
			for (size_t k = 0; k < bodies.size(); k++) {
				const Body &body = bodies[k];
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
					r_out.uv2s.push_back(Vector2(0.0f, body.stream ? -1.0f : 0.0f)); // water.gdshader's UV2
					r_out.colors.push_back(flow_at(body.flow, p.x, p.y));
					r_out.depths.push_back(std::max(body.top - body.ground_at(p), 0.0f));
					surface_index.insert(weld, index);
					r_out.indices.push_back(index);
				};
				if (water_outline_fill) {
					// the flat water comes from the outlines instead (mc_grid_water_fill.cpp)
				} else if (tile.whole) {
					const Vector2 q[4] = { Vector2(-0.5f, -0.5f), Vector2(0.5f, -0.5f), Vector2(0.5f, 0.5f), Vector2(-0.5f, 0.5f) };
					for (int m : { 0, 1, 2, 0, 2, 3 }) {
						surface_vertex(q[m]);
					}
				} else {
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
				// the edges for the outlines, oriented with the water on the left seen from above
				// (out points away from the water: a -> b turned left is the water side)
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
				for (const WaterTile::Shore &shore : tile.shores) {
					add_edge(shore.a, shore.b, shore.out, WaterEdge::BANK);
				}
				for (const WaterTile::Fall &fall : tile.falls) {
					add_edge(fall.a, fall.b, fall.out, WaterEdge::FALL);
				}
				// shore foam: points along where the surface meets a bank, drifting away from it
				for (const WaterTile::Shore &shore : tile.shores) {
					Vector3 away(-shore.out.x, 0, shore.out.y);
					_sample_line(world(shore.a, body.top + 0.02f), world(shore.b, body.top + 0.02f), away,
							r_out.shore_points, r_out.shore_directions);
				}
				if (debug_water) {
					for (const Vector2 &p : tile.wet_samples) {
						r_out.samples.push_back(world(p, body.top));
					}
				}
			}
		}
	}
	_extend_fall_ends(placed_falls, water_expand);
	// fall splashes: points along each fall's foot, thrown up and out
	for (const PlacedFall &fall : placed_falls) {
		if (fall.drop < 0.1f) {
			continue;
		}
		Vector3 lift(0, 0.05f, 0);
		_sample_line(fall.ba + lift, fall.bb + lift, (fall.out * 0.5f + Vector3(0, 1, 0)).normalized(),
				r_out.splash_points, r_out.splash_directions);
	}
	r_out.final_uv2 = true;
	r_out.placed_falls = placed_falls;
	if (!water_outline_fill) { // (outline: swept along the outlines instead, mc_grid_water_sweep.cpp)
		_emit_fall_strips(placed_falls, r_out); // bevelled L strips (mc_grid_water_falls.cpp)
	}
}

} // namespace godot
