// MCGrid: the flat water from the outlines (steps 3-4 of the outline-first water mesh; on with
// water_outline_fill, the Water tab's "Surface: tiles / outline").
//
//   3. inset    every outline (mc_grid_water_outline.cpp) resampled to points at most RESAMPLE
//               apart, then its FALL runs pulled in toward the water by water_fall_lip (the falls'
//               bevel covers from there to the fall line). Points between two FALL edges move along
//               the mitre of their normals; a point where a FALL run meets a BANK run walks along
//               the BANK points instead until it is p_inset in from the FALL line (the bank points
//               it passes are dropped), so it stays on the bank line and the corner closes cleanly. BANK runs stay where they are (tucked under the rock).
//   4. fill     per surface height: the inset outline points plus a grid of points inside the water
//               (every GRID metres, at least CLEAR from the outline) and a ring RING in from it, Delaunay-triangulated (water
//               reaching the grid's border, which has only hole loops, is closed by the border); a triangle
//               is kept when its middle and its edges' middles are inside the outlines (even-odd over
//               all the loops of that height, so islands are holes). The outline is sampled densely
//               and the inner points keep clear of it, so the triangles follow it.
// Vertex data as the tile surface's: UV world x, z; COLOR the flow of the nearest column; depth
// (CUSTOM0.x) the surface height above the nearest column's ground.
#include "marching_cubes/grid/mc_grid.h"
#include <godot_cpp/classes/geometry2d.hpp>
#include <godot_cpp/classes/time.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/templates/hash_map.hpp>
#include <algorithm>
#include <cmath>
#include <vector>

namespace godot {

namespace {
const float RESAMPLE = 0.1f; // m: outline points at most this far apart
const float GRID = 0.25f; // m: inner points' spacing
const float CLEAR = 0.17f; // m: grid points keep at least this far from the outline
const float RING = 0.09f; // m: a ring of points this far in from the outline (the triangles then
                          // follow the outline round its inner corners instead of spanning them)
const float SAME_TOP = 1e-3f;

Vector2 xz(const Vector3 &p_v) { return Vector2(p_v.x, p_v.z); }

// The left (water side) normal of a -> b seen from above, in x, z.
Vector2 left_of(
		const Vector2 &p_a,
		const Vector2 &p_b
) {
	Vector2 d = p_b - p_a;
	if (d.length_squared() < 1e-12f) {
		return Vector2();
	}
	d = d.normalized();
	return Vector2(-d.y, d.x);
}

// The loops' edges bucketed by rows of ROW metres in z, for a fast even-odd inside test: a ray
// from p along +x only meets the edges spanning p's row.
struct LoopRows {
	static constexpr float ROW = 0.25f;
	int first_row = 0;
	std::vector<std::vector<std::pair<Vector2, Vector2>>> rows;

	explicit LoopRows(const std::vector<std::vector<Vector2>> &p_loops) {
		float lo = 1e30f;
		float hi = -1e30f;
		for (const std::vector<Vector2> &loop : p_loops) {
			for (const Vector2 &p : loop) {
				lo = std::min(lo, p.y);
				hi = std::max(hi, p.y);
			}
		}
		if (lo > hi) {
			return;
		}
		first_row = static_cast<int>(std::floor(lo / ROW));
		rows.resize(static_cast<size_t>(static_cast<int>(std::floor(hi / ROW)) - first_row + 1));
		for (const std::vector<Vector2> &loop : p_loops) {
			size_t n = loop.size();
			for (size_t i = 0; i < n; i++) {
				const Vector2 &a = loop[i];
				const Vector2 &b = loop[(i + 1) % n];
				int r0 = static_cast<int>(std::floor(std::min(a.y, b.y) / ROW)) - first_row;
				int r1 = static_cast<int>(std::floor(std::max(a.y, b.y) / ROW)) - first_row;
				for (int r = r0; r <= r1; r++) {
					rows[static_cast<size_t>(r)].push_back({ a, b });
				}
			}
		}
	}

	bool inside(const Vector2 &p) const {
		int r = static_cast<int>(std::floor(p.y / ROW)) - first_row;
		if (r < 0 || r >= static_cast<int>(rows.size())) {
			return false;
		}
		bool in = false;
		for (const std::pair<Vector2, Vector2> &e : rows[static_cast<size_t>(r)]) {
			const Vector2 &a = e.first;
			const Vector2 &b = e.second;
			if ((a.y > p.y) != (b.y > p.y) && p.x < (b.x - a.x) * (p.y - a.y) / (b.y - a.y) + a.x) {
				in = !in;
			}
		}
		return in;
	}
};

// Squared distance from p to segment a - b.
float segment_distance_squared(
		const Vector2 &p,
		const Vector2 &a,
		const Vector2 &b
) {
	Vector2 ab = b - a;
	float l = ab.length_squared();
	float t = l > 1e-12f ? std::clamp((p - a).dot(ab) / l, 0.0f, 1.0f) : 0.0f;
	return (p - (a + ab * t)).length_squared();
}
} // namespace

void MCGrid::set_water_outline_fill(bool p_on) {
	water_outline_fill = p_on;
	if (!chunks.empty() && water_levels.size() == column_heights.size()) {
		_queue_water();
	}
}

// Step 3 on one outline: resampled, its FALL runs pulled in by p_inset (see the top of this file).
WaterOutline MCGrid::_inset_outline(
		const WaterOutline &p_outline,
		float p_inset
) {
	WaterOutline resampled;
	resampled.top = p_outline.top;
	resampled.closed = p_outline.closed;
	int count = static_cast<int>(p_outline.points.size());
	int edges = static_cast<int>(p_outline.kinds.size());
	for (int i = 0; i < edges; i++) {
		const Vector3 &a = p_outline.points[static_cast<size_t>(i)];
		const Vector3 &b = p_outline.points[static_cast<size_t>((i + 1) % count)];
		int pieces = std::max(1, static_cast<int>(std::ceil(a.distance_to(b) / RESAMPLE)));
		for (int k = 0; k < pieces; k++) {
			resampled.points.push_back(a.lerp(b, static_cast<float>(k) / static_cast<float>(pieces)));
			resampled.kinds.push_back(p_outline.kinds[static_cast<size_t>(i)]);
		}
	}
	if (!resampled.closed) {
		resampled.points.push_back(p_outline.points.back());
	}
	resampled.origins = resampled.points;
	if (p_inset <= 0.0f) {
		return resampled;
	}
	WaterOutline inset = resampled;
	count = static_cast<int>(resampled.points.size());
	edges = static_cast<int>(resampled.kinds.size());
	std::vector<uint8_t> removed(static_cast<size_t>(count), 0); // bank points a slide passed
	auto point = [&](int p_i) { return xz(resampled.points[static_cast<size_t>((p_i + count) % count)]); };
	for (int i = 0; i < count; i++) {
		bool has_in = resampled.closed || i > 0;
		bool has_out = resampled.closed || i < edges;
		bool fall_in = has_in && resampled.kinds[static_cast<size_t>((i - 1 + edges) % edges)] == WaterEdge::FALL;
		bool fall_out = has_out && resampled.kinds[static_cast<size_t>(i % edges)] == WaterEdge::FALL;
		if (!fall_in && !fall_out) {
			continue; // BANK: stays under the rock
		}
		Vector2 p = point(i);
		Vector2 moved = p;
		if (fall_in && fall_out) { // the mitre of the two normals
			Vector2 n1 = left_of(point(i - 1), p);
			Vector2 n2 = left_of(p, point(i + 1));
			Vector2 n = n1 + n2;
			if (n.length_squared() > 1e-8f) {
				n = n.normalized();
				moved = p + n * (p_inset / std::max(n.dot(n1), 0.3f));
			}
		} else { // FALL meets BANK: slide along the BANK edge, staying on the bank line
			Vector2 fall_normal = fall_in ? left_of(point(i - 1), p) : left_of(p, point(i + 1));
			bool bank_ahead = fall_in; // the BANK edge leaves this point (else it arrives)
			bool has_bank = bank_ahead ? has_out : has_in;
			if (has_bank) {
				// walk along the BANK points (they are RESAMPLE apart) until the walk has come
				// p_inset in from the FALL line; the bank points passed are dropped, so the
				// outline doesn't fold back over itself
				int step = bank_ahead ? 1 : -1;
				Vector2 at = p;
				int j = i;
				bool reached = false;
				float walked = 0.0f;
				std::vector<int> passed;
				for (int guard = 0; guard < count && walked < p_inset * 3.0f; guard++) {
					int k = j + step;
					if (!resampled.closed && (k < 0 || k >= count)) {
						break;
					}
					Vector2 next = point(k);
					float gained = (next - p).dot(fall_normal); // how far in from the FALL line
					if (gained >= p_inset) {
						float before = (at - p).dot(fall_normal);
						float t = gained - before > 1e-6f ? (p_inset - before) / (gained - before) : 1.0f;
						at = at.lerp(next, std::clamp(t, 0.0f, 1.0f));
						reached = true;
						break;
					}
					int kind_at = bank_ahead ? ((k - 1 + edges) % edges) : ((k + edges) % edges);
					if (resampled.kinds[static_cast<size_t>(kind_at)] != WaterEdge::BANK) {
						break; // the bank run ended
					}
					walked += next.distance_to(at);
					at = next;
					j = k;
					passed.push_back((k + count) % count);
				}
				if (reached) {
					for (int k : passed) {
						removed[static_cast<size_t>(k)] = 1;
					}
					moved = at;
				}
				// not reached (the bank carries on along the fall line): the point stays
			} else {
				moved = p + fall_normal * p_inset; // an open run's end
			}
		}
		Vector3 &out = inset.points[static_cast<size_t>(i)];
		out = Vector3(moved.x, out.y, moved.y);
	}
	// drop the passed bank points (their edges merge into the slid point's)
	WaterOutline kept;
	kept.top = inset.top;
	kept.closed = inset.closed;
	for (int i = 0; i < count; i++) {
		if (removed[static_cast<size_t>(i)]) {
			continue;
		}
		kept.points.push_back(inset.points[static_cast<size_t>(i)]);
		kept.origins.push_back(resampled.points[static_cast<size_t>(i)]);
		if (i < edges) {
			kept.kinds.push_back(inset.kinds[static_cast<size_t>(i)]);
		}
	}
	_cut_inset_loops(kept);
	return kept;
}

// Where the outline turns sharply, pulling both sides in makes them cross: a little loop (a bow tie)
// that would show as a hole in the flat water and overlapping falls. Each such loop (segment i
// crossing a segment a few points further on) is closed: the points in between all move to the
// crossing point, keeping their own origins (the fall line), so the fall fans round the corner.
void MCGrid::_cut_inset_loops(WaterOutline &r_outline) {
	const int reach = 60; // points ahead to look for a crossing (a loop is at most this long)
	bool cut = true;
	while (cut) {
		cut = false;
		int n = static_cast<int>(r_outline.points.size());
		int segments = r_outline.closed ? n : n - 1;
		for (int i = 0; i < segments && !cut; i++) {
			Vector2 a = xz(r_outline.points[static_cast<size_t>(i)]);
			Vector2 b = xz(r_outline.points[static_cast<size_t>((i + 1) % n)]);
			for (int k = 2; k < std::min(reach, segments - 1) && !cut; k++) {
				int j = i + k;
				if (!r_outline.closed && j >= segments) {
					break;
				}
				Vector2 c = xz(r_outline.points[static_cast<size_t>(j % n)]);
				Vector2 d = xz(r_outline.points[static_cast<size_t>((j + 1) % n)]);
				Vector2 r = b - a;
				Vector2 q = d - c;
				float den = r.cross(q);
				if (std::abs(den) < 1e-9f) {
					continue;
				}
				float t = (c - a).cross(q) / den;
				float u = (c - a).cross(r) / den;
				if (t <= 0.0f || t >= 1.0f || u <= 0.0f || u >= 1.0f) {
					continue;
				}
				// points i + 1 .. j (mod n) all move to the crossing; they keep their own origins
				// (the fall line round the corner), so the fall fans round it from that one point
				Vector2 x = a + r * t;
				bool moved = false;
				for (int m = i + 1; m <= j; m++) {
					Vector3 &p = r_outline.points[static_cast<size_t>(m % n)];
					if (Vector2(p.x - x.x, p.z - x.y).length_squared() > 1e-10f) {
						p = Vector3(x.x, p.y, x.y);
						moved = true;
					}
				}
				cut = moved;
			}
		}
	}
}

// Step 4: the flat water from the outlines (see the top of this file).
void MCGrid::_fill_water_surface(
		const std::vector<Color> &p_flow,
		MCWaterArrays &r_out
) const {
	uint64_t prof[5] = { 0, 0, 0, 0, 0 }; // debug: inset, points, delaunay, triangles, point count
	// the closed outlines, inset, grouped by height
	std::vector<std::pair<float, std::vector<std::vector<Vector2>>>> groups;
	for (const WaterOutline &outline : water_outlines) {
		if (!outline.closed || outline.points.size() < 3) {
			continue;
		}
		uint64_t ti = Time::get_singleton()->get_ticks_usec();
		WaterOutline inset = _inset_outline(outline, water_fall_lip);
		prof[0] += Time::get_singleton()->get_ticks_usec() - ti;
		std::vector<Vector2> loop;
		for (const Vector3 &p : inset.points) {
			Vector2 q = xz(p);
			if (loop.empty() || loop.back().distance_squared_to(q) > 1e-8f) { // (a cut loop's points sit on one spot)
				loop.push_back(q);
			}
		}
		if (loop.size() > 1 && loop.front().distance_squared_to(loop.back()) <= 1e-8f) {
			loop.pop_back();
		}
		if (loop.size() < 3) {
			continue;
		}
		size_t g = 0;
		while (g < groups.size() && std::abs(groups[g].first - outline.top) > SAME_TOP) {
			g++;
		}
		if (g == groups.size()) {
			groups.push_back({ outline.top, {} });
		}
		groups[g].second.push_back(loop);
	}
	Vector2i columns = _column_counts();
	// water that reaches the grid's border (the sea round an island) has only hole loops (water on
	// their outside: negative area, the water side being the left); the border closes it
	for (auto &group : groups) {
		float area = 0.0f;
		for (const std::vector<Vector2> &loop : group.second) {
			for (size_t i = 0; i < loop.size(); i++) {
				const Vector2 &a = loop[i];
				const Vector2 &b = loop[(i + 1) % loop.size()];
				area += a.x * b.y - b.x * a.y;
			}
		}
		if (area < 0.0f) {
			float x1 = static_cast<float>(columns.x - 1);
			float z1 = static_cast<float>(columns.y - 1);
			std::vector<Vector2> border;
			const int steps = std::max(columns.x, columns.y) * 2; // points along it, for the triangles
			for (int i = 0; i < steps; i++) {
				border.push_back(Vector2(x1 * static_cast<float>(i) / static_cast<float>(steps), 0.0f));
			}
			for (int i = 0; i < steps; i++) {
				border.push_back(Vector2(x1, z1 * static_cast<float>(i) / static_cast<float>(steps)));
			}
			for (int i = 0; i < steps; i++) {
				border.push_back(Vector2(x1 * (1.0f - static_cast<float>(i) / static_cast<float>(steps)), z1));
			}
			for (int i = 0; i < steps; i++) {
				border.push_back(Vector2(0.0f, z1 * (1.0f - static_cast<float>(i) / static_cast<float>(steps))));
			}
			group.second.push_back(border);
		}
	}
	auto column_at = [&](const Vector2 &p) {
		int x = std::clamp(static_cast<int>(std::lround(p.x)), 0, columns.x - 1);
		int z = std::clamp(static_cast<int>(std::lround(p.y)), 0, columns.y - 1);
		return static_cast<size_t>(_column_index(x, z));
	};
	for (const auto &group : groups) {
		float top = group.first;
		const std::vector<std::vector<Vector2>> &loops = group.second;
		PackedVector2Array points;
		Rect2 bounds;
		bool first = true;
		for (const std::vector<Vector2> &loop : loops) {
			for (const Vector2 &p : loop) {
				points.push_back(p);
				if (first) {
					bounds = Rect2(p, Vector2());
					first = false;
				} else {
					bounds.expand_to(p);
				}
			}
		}
		if (points.size() < 3) {
			continue;
		}
		uint64_t tp = Time::get_singleton()->get_ticks_usec();
		LoopRows rows(loops); // the inside test, bucketed
		// the outline's segments by grid cell, for the inner points' clearance test
		HashMap<Vector2i, std::vector<std::pair<Vector2, Vector2>>> segments;
		for (const std::vector<Vector2> &loop : loops) {
			for (size_t i = 0; i < loop.size(); i++) {
				const Vector2 &a = loop[i];
				const Vector2 &b = loop[(i + 1) % loop.size()];
				Vector2i cell(static_cast<int>(std::floor(a.x)), static_cast<int>(std::floor(a.y)));
				if (!segments.has(cell)) {
					segments.insert(cell, {});
				}
				segments[cell].push_back({ a, b });
			}
		}
		auto clear_of_outline = [&](const Vector2 &p) {
			int cx = static_cast<int>(std::floor(p.x));
			int cz = static_cast<int>(std::floor(p.y));
			for (int z = cz - 1; z <= cz + 1; z++) {
				for (int x = cx - 1; x <= cx + 1; x++) {
					if (const auto *list = segments.getptr(Vector2i(x, z))) {
						for (const auto &s : *list) {
							if (segment_distance_squared(p, s.first, s.second) < CLEAR * CLEAR) {
								return false;
							}
						}
					}
				}
			}
			return true;
		};
		// the ring just inside the outline (each outline point moved in along its normals' average;
		// kept only where it is inside and clear of the outline)
		for (const std::vector<Vector2> &loop : loops) {
			size_t n = loop.size();
			for (size_t i = 0; i < n; i++) {
				const Vector2 &prev = loop[(i + n - 1) % n];
				const Vector2 &p = loop[i];
				const Vector2 &next = loop[(i + 1) % n];
				Vector2 normal = left_of(prev, p) + left_of(p, next);
				if (normal.length_squared() < 1e-8f) {
					continue;
				}
				Vector2 q = p + normal.normalized() * RING;
				bool clear = true;
				int cx = static_cast<int>(std::floor(q.x));
				int cz = static_cast<int>(std::floor(q.y));
				for (int z = cz - 1; z <= cz + 1 && clear; z++) {
					for (int x = cx - 1; x <= cx + 1 && clear; x++) {
						if (const auto *list = segments.getptr(Vector2i(x, z))) {
							for (const auto &sg : *list) {
								if (segment_distance_squared(q, sg.first, sg.second) < RING * RING * 0.5f) {
									clear = false;
									break;
								}
							}
						}
					}
				}
				if (clear && rows.inside(q)) {
					points.push_back(q);
				}
			}
		}
		for (float z = std::ceil(bounds.position.y / GRID) * GRID; z < bounds.get_end().y; z += GRID) {
			for (float x = std::ceil(bounds.position.x / GRID) * GRID; x < bounds.get_end().x; x += GRID) {
				Vector2 p(x, z);
				if (rows.inside(p) && clear_of_outline(p)) {
					points.push_back(p);
				}
			}
		}
		uint64_t td = Time::get_singleton()->get_ticks_usec();
		prof[1] += td - tp;
		PackedInt32Array triangles = Geometry2D::get_singleton()->triangulate_delaunay(points);
		uint64_t tf = Time::get_singleton()->get_ticks_usec();
		prof[2] += tf - td;
		prof[4] += static_cast<uint64_t>(points.size());
		int base = static_cast<int>(r_out.vertices.size());
		for (int i = 0; i < points.size(); i++) {
			const Vector2 &p = points[i];
			size_t column = column_at(p);
			r_out.vertices.push_back(Vector3(p.x, top, p.y));
			r_out.normals.push_back(Vector3(0, 1, 0));
			r_out.uvs.push_back(p);
			r_out.uv2s.push_back(Vector2(0.0f, 0.0f));
			Color c = column < p_flow.size() ? p_flow[column] : Color(0, 0, 0, 0);
			r_out.colors.push_back(Color(c.r * 0.5f + 0.5f, c.g * 0.5f + 0.5f, c.b, c.a));
			float ground = column < column_heights.size() ? static_cast<float>(column_heights[column]) - 0.5f : top;
			r_out.depths.push_back(std::max(top - ground, 0.0f));
		}
		for (int t = 0; t + 2 < triangles.size(); t += 3) {
			const Vector2 &a = points[triangles[t]];
			const Vector2 &b = points[triangles[t + 1]];
			const Vector2 &c = points[triangles[t + 2]];
			// the middle and each edge's middle nudged toward it (an outline edge's own middle lies
			// on the outline, where inside / outside is a coin toss)
			Vector2 middle = (a + b + c) / 3.0f;
			auto nudged = [&](const Vector2 &p_u, const Vector2 &p_v) { return ((p_u + p_v) * 0.5f).lerp(middle, 0.1f); };
			bool in = rows.inside(middle) && rows.inside(nudged(a, b)) && rows.inside(nudged(b, c)) && rows.inside(nudged(c, a));
			if (!in) {
				continue;
			}
			for (int k = 0; k < 3; k++) {
				r_out.indices.push_back(base + triangles[t + k]); // winding fixed in _water_mesh
			}
		}
	}
	if (water_sweep_debug) {
		UtilityFunctions::print("fill: inset ", prof[0], " us, points ", prof[1], " us, delaunay ", prof[2], " us, ", prof[4], " points");
	}
}

} // namespace godot
