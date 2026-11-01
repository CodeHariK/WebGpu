// MCGrid: the water mesh from a fine sample grid + marching squares (water_sampled; the other
// builder is mc_grid_water_mesh.cpp).
//
//   1. sample   a regular grid of points, water_sample_rate per cell side, over the whole map. Each
//               point asks the cell it is in which water body covers it (mc_water::Body, the cliff
//               tiles' shape) and takes that body's surface height, or none (dry). The same
//               point always gets the same answer, so neighbouring squares agree.
//   2. surface  marching squares, one pass per water height in a square: corners at that height are
//               in, the others out; where an edge changes, the crossing is found by bisection on
//               the exact shape (not half way), cached per edge, so neighbours share it exactly.
//               A cell that one height covers whole is a single quad.
//   3. walls    each contour segment of a height's region looks just outside itself: lower water
//               there -> a fall down to it; dry ground below the surface -> a fall onto the ground;
//               a bank (ground at or above the surface) or higher water -> nothing. The falls are
//               built from the same contour points as the surface, so the two meet exactly.
// The sample points are kept for the water debug overlay (dots coloured by height).
#include "marching_cubes/grid/mc_grid.h"
#include "marching_cubes/grid/mc_water_shape.h"
#include <godot_cpp/templates/hash_map.hpp>
#include <cmath>
#include <limits>

namespace godot {

using namespace mc_water;

namespace {
const float NONE = std::numeric_limits<float>::quiet_NaN();
const float PROBE = 0.03f; // a wall looks this far outside its contour
const float INSIDE = 1e-4f; // the shape value a point needs to count as water
const int BISECT = 10; // steps when locating a contour crossing on a square's edge

bool same_height(
		float p_a,
		float p_b
) {
	return std::abs(p_a - p_b) < 1e-4f;
}
} // namespace

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

void MCGrid::_emit_water_sampled(
		const std::vector<float> &p_surface,
		const std::vector<uint8_t> &p_held,
		const std::vector<Color> &p_flow,
		MCWaterArrays &r_out
) const {
	Vector2i columns = _column_counts();
	Vector2i cells(columns.x - 1, columns.y - 1);
	if (cells.x <= 0 || cells.y <= 0) {
		return;
	}
	const int rate = water_sample_rate;
	// every cell's bodies, once
	std::vector<std::vector<Body>> bodies(static_cast<size_t>(cells.x * cells.y));
	std::vector<float> bottoms;
	for (int cz = 0; cz < cells.y; cz++) {
		for (int cx = 0; cx < cells.x; cx++) {
			_water_bodies(cx, cz, p_surface, p_held, p_flow, bodies[static_cast<size_t>(cz * cells.x + cx)], bottoms);
		}
	}
	auto cell_of = [&](float p_v, int p_count) { return std::clamp(static_cast<int>(std::floor(p_v)), 0, p_count - 1); };
	// cell-local shape coordinates of world (x, z) in cell (cx, cz); Godot z = -y
	auto local = [](float p_x, float p_z, int p_cx, int p_cz) {
		return Vector2(p_x - static_cast<float>(p_cx) - 0.5f, -(p_z - static_cast<float>(p_cz) - 0.5f));
	};
	// the water height at world (x, z) seen from cell (cx, cz): the highest body covering it
	auto height_in = [&](float p_x, float p_z, int p_cx, int p_cz) {
		float best = NONE;
		Vector2 p = local(p_x, p_z, p_cx, p_cz);
		for (const Body &body : bodies[static_cast<size_t>(p_cz * cells.x + p_cx)]) {
			// strictly inside: a point exactly on a bank's edge (square tiles' quarters meet on the
			// cell's middle lines with the shape value 0 there) is not water
			if (body.water_field(p) > INSIDE && (std::isnan(best) || body.top > best)) {
				best = body.top;
			}
		}
		return best;
	};
	auto height_at = [&](float p_x, float p_z) {
		return height_in(p_x, p_z, cell_of(p_x, cells.x), cell_of(p_z, cells.y));
	};
	// the dry ground's surface height at world (x, z): the tiles' shape of that cell's columns
	auto ground_at = [&](float p_x, float p_z, const Layer &p_style) {
		int cx = cell_of(p_x, cells.x);
		int cz = cell_of(p_z, cells.y);
		Body probe;
		probe.above = p_style;
		const Vector2i at[4] = { Vector2i(cx, cz + 1), Vector2i(cx + 1, cz + 1), Vector2i(cx + 1, cz), Vector2i(cx, cz) };
		for (int k = 0; k < 4; k++) {
			probe.ground[k] = column_heights[static_cast<size_t>(_column_index(at[k].x, at[k].y))];
		}
		return probe.ground_at(local(p_x, p_z, cx, cz));
	};

	// 1. sample
	const int nx = cells.x * rate + 1;
	const int nz = cells.y * rate + 1;
	const float step = 1.0f / static_cast<float>(rate);
	std::vector<float> samples(static_cast<size_t>(nx * nz), NONE);
	for (int j = 0; j < nz; j++) {
		for (int i = 0; i < nx; i++) {
			float h = height_at(static_cast<float>(i) * step, static_cast<float>(j) * step);
			samples[static_cast<size_t>(j * nx + i)] = h;
			if (!std::isnan(h)) {
				r_out.samples.push_back(Vector3(static_cast<float>(i) * step, h, static_cast<float>(j) * step));
			}
		}
	}
	auto sample = [&](int p_i, int p_j) { return samples[static_cast<size_t>(p_j * nx + p_i)]; };

	// 2 + 3. squares
	HashMap<int64_t, Vector3> crossings; // edge + height -> the crossing point
	auto emit_vertex = [&](const Vector3 &p_v, const Body &p_body, int p_cx, int p_cz) {
		r_out.vertices.push_back(p_v);
		r_out.normals.push_back(Vector3(0, 1, 0));
		r_out.uvs.push_back(Vector2(p_v.x, p_v.z));
		r_out.uv2s.push_back(Vector2(p_body.stream ? 1.0f : 0.0f, 0));
		Vector2 p = local(p_v.x, p_v.z, p_cx, p_cz);
		r_out.colors.push_back(flow_at(p_body.flow, p.x, p.y));
	};
	auto emit_wall = [&](const Vector3 &p_a, const Vector3 &p_b, float p_bottom, const Vector3 &p_out) {
		float drop = p_a.y - p_bottom;
		const Vector3 sheet[4] = { Vector3(p_a.x, p_bottom, p_a.z), Vector3(p_b.x, p_bottom, p_b.z), p_b, p_a };
		float ua = p_a.x + p_a.z;
		float ub = p_b.x + p_b.z;
		const Vector2 uv[4] = { Vector2(ua, drop), Vector2(ub, drop), Vector2(ub, 0), Vector2(ua, 0) };
		const Vector2 uv2[4] = { Vector2(0, 0), Vector2(0, 0), Vector2(drop, 0), Vector2(drop, 0) };
		for (int k : { 0, 1, 2, 0, 2, 3 }) {
			r_out.fall_vertices.push_back(sheet[k]);
			r_out.fall_normals.push_back(p_out);
			r_out.fall_uvs.push_back(uv[k]);
			r_out.fall_uv2s.push_back(uv2[k]);
		}
	};
	for (int cz = 0; cz < cells.y; cz++) {
		for (int cx = 0; cx < cells.x; cx++) {
			const std::vector<Body> &cell_bodies = bodies[static_cast<size_t>(cz * cells.x + cx)];
			if (cell_bodies.empty()) {
				continue;
			}
			// a cell one height covers whole: one quad
			int i0 = cx * rate;
			int j0 = cz * rate;
			float first = sample(i0, j0);
			bool whole = !std::isnan(first);
			for (int j = j0; j <= j0 + rate && whole; j++) {
				for (int i = i0; i <= i0 + rate && whole; i++) {
					whole = same_height(sample(i, j), first);
				}
			}
			if (whole) {
				const Body *body = &cell_bodies[0];
				for (const Body &b : cell_bodies) {
					if (same_height(b.top, first)) {
						body = &b;
					}
				}
				float x0 = static_cast<float>(cx);
				float z0 = static_cast<float>(cz);
				const Vector3 q[4] = { Vector3(x0, first, z0), Vector3(x0 + 1, first, z0), Vector3(x0 + 1, first, z0 + 1),
					Vector3(x0, first, z0 + 1) };
				for (int k : { 0, 1, 2, 0, 2, 3 }) {
					emit_vertex(q[k], *body, cx, cz);
				}
				continue;
			}
			for (int j = j0; j < j0 + rate; j++) {
				for (int i = i0; i < i0 + rate; i++) {
					// corners counter-clockwise in x z: (i, j) (i+1, j) (i+1, j+1) (i, j+1)
					const int ci[4] = { i, i + 1, i + 1, i };
					const int cj[4] = { j, j, j + 1, j + 1 };
					float h[4];
					for (int k = 0; k < 4; k++) {
						h[k] = sample(ci[k], cj[k]);
					}
					for (int k = 0; k < 4; k++) {
						if (std::isnan(h[k])) {
							continue;
						}
						bool done = false;
						for (int m = 0; m < k; m++) {
							done = done || same_height(h[m], h[k]);
						}
						if (done) {
							continue;
						}
						float level = h[k];
						const Body *body = nullptr;
						for (const Body &b : cell_bodies) {
							if (same_height(b.top, level)) {
								body = &b;
							}
						}
						if (!body) {
							continue;
						}
						bool in[4];
						for (int m = 0; m < 4; m++) {
							in[m] = same_height(h[m], level);
						}
						auto corner = [&](int m) {
							return Vector3(static_cast<float>(ci[m]) * step, level, static_cast<float>(cj[m]) * step);
						};
						// the crossing on the edge from corner m to m + 1 (bisection from its lower-index
						// end, so both squares sharing the edge find the same point)
						auto crossing = [&](int m) {
							int n = (m + 1) % 4;
							int a = m;
							int b = n;
							if (cj[b] * nx + ci[b] < cj[a] * nx + ci[a]) {
								std::swap(a, b);
							}
							int64_t edge = (static_cast<int64_t>(cj[a]) * nx + ci[a]) * 2 + (cj[a] == cj[b] ? 0 : 1);
							int64_t key = edge * 4096 + static_cast<int64_t>(std::lround(level * 64.0f)) + 2048;
							if (const Vector3 *hit = crossings.getptr(key)) {
								return *hit;
							}
							Vector3 pa = corner(a);
							Vector3 pb = corner(b);
							bool in_a = in[a];
							float lo = 0.0f;
							float hi = 1.0f;
							for (int s = 0; s < BISECT; s++) {
								float t = (lo + hi) * 0.5f;
								Vector3 p = pa.lerp(pb, t);
								bool in_t = same_height(height_in(p.x, p.z, cx, cz), level);
								if (in_t == in_a) {
									lo = t;
								} else {
									hi = t;
								}
							}
							Vector3 p = pa.lerp(pb, (lo + hi) * 0.5f);
							p.y = level;
							crossings.insert(key, p);
							return p;
						};
						// the region's polygon(s), and its contour segments (each with an out corner)
						std::vector<std::vector<Vector3>> polygons;
						struct Segment {
							Vector3 a;
							Vector3 b;
							Vector3 out_corner;
						};
						std::vector<Segment> segments;
						bool saddle = in[0] == in[2] && in[1] == in[3] && in[0] != in[1];
						if (saddle) {
							for (int m = 0; m < 4; m++) {
								if (!in[m]) {
									continue;
								}
								Vector3 e0 = crossing((m + 3) % 4);
								Vector3 e1 = crossing(m);
								polygons.push_back({ corner(m), e1, e0 });
								segments.push_back({ e0, e1, corner((m + 2) % 4) });
							}
						} else {
							std::vector<Vector3> polygon;
							std::vector<Vector3> cut;
							Vector3 out_sum;
							int outs = 0;
							for (int m = 0; m < 4; m++) {
								if (in[m]) {
									polygon.push_back(corner(m));
								} else {
									out_sum += corner(m);
									outs++;
								}
								if (in[m] != in[(m + 1) % 4]) {
									Vector3 e = crossing(m);
									polygon.push_back(e);
									cut.push_back(e);
								}
							}
							polygons.push_back(polygon);
							if (cut.size() == 2 && outs > 0) {
								segments.push_back({ cut[0], cut[1], out_sum / static_cast<float>(outs) });
							}
						}
						for (const std::vector<Vector3> &polygon : polygons) {
							for (size_t t = 1; t + 1 < polygon.size(); t++) {
								emit_vertex(polygon[0], *body, cx, cz);
								emit_vertex(polygon[t], *body, cx, cz);
								emit_vertex(polygon[t + 1], *body, cx, cz);
							}
						}
						// walls: what is just outside each contour segment?
						for (const Segment &seg : segments) {
							Vector3 mid = (seg.a + seg.b) * 0.5f;
							Vector3 out = seg.out_corner - mid;
							out.y = 0;
							if (out.length_squared() < 1e-12f) {
								continue;
							}
							out = out.normalized();
							Vector3 q = mid + out * PROBE;
							float other = height_at(q.x, q.z);
							float bottom;
							if (!std::isnan(other)) {
								if (other >= level - 0.02f) {
									continue; // the same or higher water: no wall here
								}
								bottom = other;
							} else {
								bottom = ground_at(q.x, q.z, body->above);
								if (bottom >= level - 0.02f) {
									continue; // a bank
								}
							}
							emit_wall(seg.a, seg.b, bottom, out);
						}
					}
				}
			}
		}
	}
}

} // namespace godot
