// MCGrid: waterfalls as bevelled L-shaped strips (the water tiles builder; Animal Crossing style).
// Seen side-on, each fall is its own strip:
//
//   surface ══════╗          flat   FLAT metres lying just above the surface (LIFT), covering where
//            ─────╲                 the surface stops: the surface is trimmed back by the bevel
//                  ╲         bevel  a quarter circle, water_fall_lip radius (less on a short fall),
//                   │               ending straight down on the fall line (the cliff)
//                   │        drop   straight down in rows of at most DROP_ROW
//                   ╰──      foot   a short flare out into the water below
//
// The strip has its OWN UV, so it is smooth however the fall curves:
//   UV.x  metres along the joined falls (a walk along each chain of falls)
//   UV.y  metres along the profile from the strip's start
// The water is opaque, so the strip simply covers the surface where it lies on it; its flat part is
// shaded from the world position like the surface (water.gdshader), so where it starts doesn't show.
// UV2: x how far over the bevel (0 flat .. 1 straight down), y metres above the landing. COLOR:
// the surface's flow where the strip starts (flat and bevel), falling (drop). Where falls meet, the
// strips share their end columns: each top point's direction (smoothed along the fall) and bevel
// radius (the smallest of the falls there) are the same for every strip ending at it (_fall_ends),
// so neighbours meet exactly and a strip wraps a corner in one piece; vertices are welded by
// position and UV.x with averaged normals, so the light has no seams either.
#include "marching_cubes/terrain/mc_grid.h"
#include "marching_cubes/water/4_tiles/mc_water_tile.h"
#include <godot_cpp/templates/hash_map.hpp>
#include <godot_cpp/variant/vector4i.hpp>
#include <algorithm>
#include <cmath>
#include <vector>

namespace godot {

namespace {
const float FLAT = 0.1f; // m of flat strip lying on the surface before the bevel
const float LIFT = 0.004f; // m the flat part lies above the surface (no z-fighting)
const int BEVEL_ROWS = 6; // rows round the bevel
const int FOOT_ROWS = 2; // rows of the flare at the foot
const float FOOT = 0.12f; // m the foot flares out (and up the fall it starts)
const float DROP_ROW = 0.25f; // m: the straight drop is cut into rows at most this tall
const float HALF_PI = 1.5707963f;
const float SAME_SURFACE = 1e-3f; // m: falls of the same surface are this close in height
const float SMOOTH = 0.4f; // m: a fall's direction is averaged over its neighbours this far along
const float WELD_SCALE = 4096.0f;
const float JOIN = 0.03f; // m: two falls' ends on the same cell side this close are one point
const int RELAX_PASSES = 3; // fall lines: smoothing passes across cells
const float RELAX = 0.5f; // how far a point moves toward its two neighbours' middle per pass

Vector3i weld(const Vector3 &p_v) {
	return Vector3i(static_cast<int>(std::lround(p_v.x * WELD_SCALE)), static_cast<int>(std::lround(p_v.y * WELD_SCALE)),
			static_cast<int>(std::lround(p_v.z * WELD_SCALE)));
}

// Squared distance in x, z from p to segment a - b.
float distance_xz_squared(
		const Vector3 &p,
		const Vector3 &a,
		const Vector3 &b
) {
	Vector2 ab(b.x - a.x, b.z - a.z);
	Vector2 ap(p.x - a.x, p.z - a.z);
	float length_squared = ab.length_squared();
	float t = length_squared > 1e-12f ? std::clamp(ap.dot(ab) / length_squared, 0.0f, 1.0f) : 0.0f;
	return (ap - ab * t).length_squared();
}

// The flow COLOR of water running in p_direction (the drop).
Color falling_flow(const Vector3 &p_direction) {
	return Color(p_direction.x * 0.5f + 0.5f, p_direction.z * 0.5f + 0.5f, 1.0f, 0.0f);
}

// One row of a strip's profile, from the fall's top point: out (negative: back over the surface)
// and down, the normal (out, up), metres along the profile, how far over the bevel.
struct Row {
	float out;
	float down;
	Vector2 normal;
	float along;
	float over;
};

// How many straight rows a fall p_drop high with bevel radius p_radius gets.
int drop_rows(
		float p_radius,
		float p_drop
) {
	float below = p_drop - p_radius;
	float straight = below - std::min(FOOT, below * 0.5f);
	return std::max(1, static_cast<int>(std::ceil(straight / DROP_ROW)));
}

// The profile of one end of a fall p_drop high: the lip p_lip (the flat part starts FLAT behind it),
// bevel radius p_radius (FallEnd::radius), p_rows straight rows. Always the same number of rows
// (both ends of a strip are paired row by row).
std::vector<Row> profile(
		float p_lip,
		float p_radius,
		float p_drop,
		int p_rows
) {
	std::vector<Row> rows;
	float r = p_radius;
	auto push = [&](float p_out, float p_down, const Vector2 &p_normal, float p_over) {
		Row row = { p_out, p_down, p_normal.normalized(), 0.0f, p_over };
		if (!rows.empty()) {
			row.along = rows.back().along + Vector2(p_out - rows.back().out, p_down - rows.back().down).length();
		}
		rows.push_back(row);
	};
	// flat, just above the surface, from FLAT behind where the tile's surface stops: the tile is
	// trimmed by the full lip (p_lip), so on a short fall (a smaller bevel) the flat part
	// still reaches back past that edge and no ground shows between them
	push(-p_lip - FLAT, -LIFT, Vector2(0, 1), 0.0f);
	// the bevel: a quarter circle centred (-r, r) below the top, from flat to straight down
	for (int i = 0; i <= BEVEL_ROWS; i++) {
		float a = static_cast<float>(i) / static_cast<float>(BEVEL_ROWS) * HALF_PI;
		float lift = LIFT * (1.0f - static_cast<float>(i) / static_cast<float>(BEVEL_ROWS));
		push(-r + r * std::sin(a), r * (1.0f - std::cos(a)) - lift, Vector2(std::sin(a), std::cos(a)), a / HALF_PI);
	}
	// the drop, then the foot
	float below = std::max(p_drop - r, 0.0f);
	float foot = std::min(FOOT, below * 0.5f);
	float straight = below - foot;
	for (int i = 1; i <= p_rows; i++) {
		push(0.0f, r + straight * static_cast<float>(i) / static_cast<float>(p_rows), Vector2(1, 0), 1.0f);
	}
	for (int i = 1; i <= FOOT_ROWS; i++) {
		float a = static_cast<float>(i) / static_cast<float>(FOOT_ROWS) * HALF_PI;
		push(foot * (1.0f - std::cos(a)), p_drop - foot + foot * std::sin(a), Vector2(std::cos(a), std::sin(a)), 1.0f);
	}
	return rows;
}
} // namespace

// Metres along each chain of joined falls at every fall's two ends (ta, tb): the strips' UV.x, so
// the texture keeps its spacing round corners. A walk from each chain's end, then from any loop
// left; each fall gets the walk's distance where it was entered and that plus its length where it
// leaves, so a loop's last fall runs on past the loop's length (its far end doesn't drop back to 0;
// the strips there don't share the vertex: they are welded by position and UV.x).
std::vector<std::pair<float, float>> MCGrid::_fall_chain(const std::vector<PlacedFall> &p_falls) const {
	HashMap<Vector3i, std::vector<int>> at;
	for (int f = 0; f < static_cast<int>(p_falls.size()); f++) {
		for (const Vector3 &v : { p_falls[f].ta, p_falls[f].tb }) {
			Vector3i key = weld(v);
			if (!at.has(key)) {
				at.insert(key, std::vector<int>());
			}
			at[key].push_back(f);
		}
	}
	std::vector<std::pair<float, float>> ends(p_falls.size());
	for (size_t f = 0; f < p_falls.size(); f++) {
		ends[f] = { 0.0f, p_falls[f].ta.distance_to(p_falls[f].tb) }; // (a fall the walks miss)
	}
	HashMap<Vector3i, float> along; // the walk's distance at each point it reached
	std::vector<uint8_t> walked(p_falls.size(), 0);
	for (int pass = 0; pass < 2; pass++) { // chain ends first (one fall there), then loops
		for (int f = 0; f < static_cast<int>(p_falls.size()); f++) {
			for (const Vector3 &v : { p_falls[f].ta, p_falls[f].tb }) {
				Vector3i start = weld(v);
				if (walked[f] || along.has(start) || (pass == 0 && at[start].size() != 1)) {
					continue;
				}
				along.insert(start, 0.0f);
				std::vector<Vector3i> stack = { start };
				while (!stack.empty()) {
					Vector3i key = stack.back();
					stack.pop_back();
					for (int g : at[key]) {
						if (walked[g]) {
							continue;
						}
						walked[g] = 1;
						const PlacedFall &fall = p_falls[g];
						bool from_a = weld(fall.ta) == key;
						Vector3i other = from_a ? weld(fall.tb) : weld(fall.ta);
						float u0 = along[key];
						float u1 = u0 + fall.ta.distance_to(fall.tb);
						ends[static_cast<size_t>(g)] = from_a ? std::make_pair(u0, u1) : std::make_pair(u1, u0);
						if (!along.has(other)) {
							along.insert(other, u1);
							stack.push_back(other);
						}
					}
				}
			}
		}
	}
	return ends;
}

// Each cell's tile places its falls from its own tile mesh's cut, so where a fall runs on into the
// next cell the two cells can put the shared end a little apart on their common side (up to ~2 cm):
// the strips then don't share their end column and the background shows through a crack. Ends on a
// cell side with no partner at the same spot are joined to the nearest such end on the same side at
// the same height (within JOIN): both move to their middle, top and foot together.
void MCGrid::_join_fall_ends(std::vector<PlacedFall> &r_falls) {
	struct End {
		size_t fall;
		bool b; // tb / bb (else ta / ba)
		Vector3 at;
	};
	auto on_side = [](const Vector3 &p) {
		return std::abs(p.x - std::round(p.x)) < 1e-3f || std::abs(p.z - std::round(p.z)) < 1e-3f;
	};
	HashMap<Vector3i, int> count; // how many falls end at each point
	for (const PlacedFall &fall : r_falls) {
		for (const Vector3 &v : { fall.ta, fall.tb }) {
			int *n = count.getptr(weld(v));
			if (n) {
				(*n)++;
			} else {
				count.insert(weld(v), 1);
			}
		}
	}
	std::vector<End> lonely;
	for (size_t f = 0; f < r_falls.size(); f++) {
		for (bool b : { false, true }) {
			const Vector3 &v = b ? r_falls[f].tb : r_falls[f].ta;
			if (on_side(v) && count[weld(v)] == 1) {
				lonely.push_back({ f, b, v });
			}
		}
	}
	std::vector<uint8_t> joined(lonely.size(), 0);
	for (size_t i = 0; i < lonely.size(); i++) {
		size_t best = lonely.size();
		float best_distance = JOIN;
		for (size_t k = 0; k < lonely.size(); k++) {
			const Vector3 &a = lonely[i].at;
			const Vector3 &c = lonely[k].at;
			if (k == i || joined[k] || joined[i] || lonely[k].fall == lonely[i].fall || std::abs(a.y - c.y) > 1e-3f) {
				continue;
			}
			bool same_side = (std::abs(a.x - std::round(a.x)) < 1e-3f && std::abs(c.x - a.x) < 1e-3f) ||
					(std::abs(a.z - std::round(a.z)) < 1e-3f && std::abs(c.z - a.z) < 1e-3f);
			float d = Vector2(c.x - a.x, c.z - a.z).length();
			if (same_side && d < best_distance) {
				best = k;
				best_distance = d;
			}
		}
		if (best == lonely.size()) {
			continue;
		}
		Vector3 middle = (lonely[i].at + lonely[best].at) * 0.5f;
		for (size_t e : { i, best }) {
			PlacedFall &fall = r_falls[lonely[e].fall];
			Vector3 &top = lonely[e].b ? fall.tb : fall.ta;
			Vector3 &foot = lonely[e].b ? fall.bb : fall.ba;
			Vector3 shift(middle.x - top.x, 0, middle.z - top.z);
			top += shift;
			foot += shift;
			joined[e] = 1;
		}
	}
}

// Each cell's falls follow its own edge, which meets the next cell's at an angle on their shared
// side: a fall line with a kink (a V) at every cell border. Here the joined falls are smoothed
// across cells: every point where exactly two falls meet (same height) moves RELAX of the way
// toward the middle of its two neighbours, RELAX_PASSES times; a chain's ends (a bank, dry ground)
// stay put. Top and foot move together. The flat water is then cut back under the strips
// (mc_water_surface_cut.cpp), so the moves this makes leave no overlap and no gap.
void MCGrid::_relax_fall_chains(std::vector<PlacedFall> &r_falls) {
	HashMap<Vector3i, int> index_of; // a top point -> its slot
	std::vector<Vector3> point;
	std::vector<std::vector<int>> next; // each point's neighbours (the far ends of its falls)
	std::vector<int> end_a(r_falls.size());
	std::vector<int> end_b(r_falls.size());
	auto slot = [&](const Vector3 &p) {
		Vector3i key = weld(p);
		if (const int *hit = index_of.getptr(key)) {
			return *hit;
		}
		index_of.insert(key, static_cast<int>(point.size()));
		point.push_back(p);
		next.push_back({});
		return static_cast<int>(point.size() - 1);
	};
	for (size_t f = 0; f < r_falls.size(); f++) {
		end_a[f] = slot(r_falls[f].ta);
		end_b[f] = slot(r_falls[f].tb);
		if (end_a[f] != end_b[f]) {
			next[static_cast<size_t>(end_a[f])].push_back(end_b[f]);
			next[static_cast<size_t>(end_b[f])].push_back(end_a[f]);
		}
	}
	std::vector<Vector3> moved = point;
	for (int pass = 0; pass < RELAX_PASSES; pass++) {
		std::vector<Vector3> was = moved;
		for (size_t i = 0; i < point.size(); i++) {
			const std::vector<int> &n = next[i];
			if (n.size() != 2) {
				continue; // a chain's end, or where more than two falls meet
			}
			const Vector3 &a = was[static_cast<size_t>(n[0])];
			const Vector3 &b = was[static_cast<size_t>(n[1])];
			if (std::abs(a.y - was[i].y) > SAME_SURFACE || std::abs(b.y - was[i].y) > SAME_SURFACE) {
				continue;
			}
			Vector3 middle = (a + b) * 0.5f;
			moved[i] = was[i] + Vector3(middle.x - was[i].x, 0, middle.z - was[i].z) * RELAX;
		}
	}
	for (size_t f = 0; f < r_falls.size(); f++) {
		PlacedFall &fall = r_falls[f];
		Vector3 da = moved[static_cast<size_t>(end_a[f])] - point[static_cast<size_t>(end_a[f])];
		Vector3 db = moved[static_cast<size_t>(end_b[f])] - point[static_cast<size_t>(end_b[f])];
		fall.ta += da;
		fall.ba += da;
		fall.tb += db;
		fall.bb += db;
	}
}

void MCGrid::set_water_fall_lip(float p_lip) {
	water_fall_lip = std::clamp(p_lip, 0.0f, 0.4f);
	mc_water::clear_tile_cache(); // tiles of the old lip aren't used again
	if (!chunks.empty() && water_levels.size() == column_heights.size()) {
		_queue_water();
	}
}

// Each fall's two ends (ta, tb) as its strip is drawn. The direction: smoothed along the falls (the
// edge from the sample grid jitters). The bevel radius: the lip, less on a short fall (half its
// drop), the smallest of every fall ending at that point. Both are the same for every strip ending
// at a point, so neighbouring strips put the very same column there (no crack where a fall's drop
// changes). The strips (_emit_fall_strips) and the surface cut (_cut_surface_under_falls) use these.
std::vector<FallEnds> MCGrid::_fall_ends(const std::vector<PlacedFall> &p_falls) const {
	HashMap<Vector2i, std::vector<int>> by_cell;
	HashMap<Vector3i, float> radius_at; // the smallest bevel radius of the falls ending at a point
	for (int f = 0; f < static_cast<int>(p_falls.size()); f++) {
		Vector3 mid = (p_falls[f].ta + p_falls[f].tb) * 0.5f;
		Vector2i cell(static_cast<int>(std::floor(mid.x)), static_cast<int>(std::floor(mid.z)));
		if (!by_cell.has(cell)) {
			by_cell.insert(cell, std::vector<int>());
		}
		by_cell[cell].push_back(f);
		float r = std::min(water_fall_lip, p_falls[f].drop * 0.5f); // a short fall gets a smaller bevel
		for (const Vector3 &v : { p_falls[f].ta, p_falls[f].tb }) {
			float *known = radius_at.getptr(weld(v));
			if (known) {
				*known = std::min(*known, r);
			} else {
				radius_at.insert(weld(v), r);
			}
		}
	}
	auto smoothed = [&](const Vector3 &p_top, const Vector3 &p_fallback) {
		Vector3 sum;
		int cx = static_cast<int>(std::floor(p_top.x));
		int cz = static_cast<int>(std::floor(p_top.z));
		for (int z = cz - 1; z <= cz + 1; z++) {
			for (int x = cx - 1; x <= cx + 1; x++) {
				const std::vector<int> *list = by_cell.getptr(Vector2i(x, z));
				if (!list) {
					continue;
				}
				for (int f : *list) {
					const PlacedFall &other = p_falls[f];
					if (std::abs(other.ta.y - p_top.y) > SAME_SURFACE) {
						continue;
					}
					float d = std::sqrt(distance_xz_squared(p_top, other.ta, other.tb));
					if (d < SMOOTH) {
						sum += other.out * (other.ta.distance_to(other.tb) * (1.0f - d / SMOOTH));
					}
				}
			}
		}
		return sum.length_squared() > 1e-10f ? sum.normalized() : p_fallback;
	};
	std::vector<FallEnds> ends(p_falls.size());
	for (size_t f = 0; f < p_falls.size(); f++) {
		const PlacedFall &fall = p_falls[f];
		const Vector3 *top[2] = { &fall.ta, &fall.tb };
		for (int e = 0; e < 2; e++) {
			ends[f].end[e].direction = smoothed(*top[e], fall.out);
			ends[f].end[e].radius = radius_at[weld(*top[e])];
		}
	}
	return ends;
}

void MCGrid::_emit_fall_strips(
		const std::vector<PlacedFall> &p_falls,
		const std::vector<FallEnds> &p_ends,
		MCWaterArrays &r_out
) const {
	std::vector<std::pair<float, float>> chain = _fall_chain(p_falls);

	// the strips' vertices, welded by position and UV.x: neighbouring strips share their end columns,
	// and a shared vertex gets the average of their normals, so the light blends across the join
	// instead of jumping (normals summed here, normalised at the end); where a loop of falls closes,
	// UV.x differs and the two strips keep their own vertices
	HashMap<Vector4i, int> made;
	int first = static_cast<int>(r_out.vertices.size());
	std::vector<Vector3> normal_sum;
	auto vertex = [&](const Vector3 &p_position, const Vector3 &p_normal, const Vector2 &p_uv, const Vector2 &p_uv2,
						  const Color &p_flow, float p_depth) {
		Vector3i at = weld(p_position);
		Vector4i key(at.x, at.y, at.z, static_cast<int>(std::lround(p_uv.x * 256.0f)));
		if (const int *hit = made.getptr(key)) {
			normal_sum[static_cast<size_t>(*hit - first)] += p_normal;
			return *hit;
		}
		int index = static_cast<int>(r_out.vertices.size());
		r_out.vertices.push_back(p_position);
		r_out.normals.push_back(p_normal);
		r_out.uvs.push_back(p_uv);
		r_out.uv2s.push_back(p_uv2);
		r_out.colors.push_back(p_flow);
		r_out.depths.push_back(p_depth);
		normal_sum.push_back(p_normal);
		made.insert(key, index);
		return index;
	};
	for (size_t f = 0; f < p_falls.size(); f++) {
		const PlacedFall &fall = p_falls[f];
		r_out.begin_first_piece(); // one strip: one piece
		const FallEnds &ends = p_ends[f];
		// each end's profile with its own bevel radius, the same number of rows (paired row by row)
		int rows_down = drop_rows(std::min(ends.end[0].radius, ends.end[1].radius), fall.drop);
		std::vector<Row> rows[2] = { profile(water_fall_lip, ends.end[0].radius, fall.drop, rows_down),
			profile(water_fall_lip, ends.end[1].radius, fall.drop, rows_down) };
		const Vector3 *top[2] = { &fall.ta, &fall.tb };
		const Color *top_flow[2] = { &fall.flow_a, &fall.flow_b };
		const float top_depth[2] = { fall.depth_a, fall.depth_b };
		Vector3 direction[2] = { ends.end[0].direction, ends.end[1].direction };
		float u[2] = { chain[f].first, chain[f].second };
		int previous[2] = { -1, -1 };
		for (size_t r = 0; r < rows[0].size(); r++) {
			int current[2];
			for (int e = 0; e < 2; e++) {
				const Row &row = rows[e][r];
				// offset by the point's own (smoothed) direction only: every strip ending at a point
				// puts the same column there, so neighbouring strips meet without cracks
				Vector3 position = *top[e] + direction[e] * row.out - Vector3(0, row.down, 0);
				Vector3 normal = (direction[e] * row.normal.x + Vector3(0, row.normal.y, 0)).normalized();
				Color flow = row.over < 1.0f ? *top_flow[e] : falling_flow(direction[e]);
				current[e] = vertex(position, normal, Vector2(u[e], row.along), Vector2(row.over, fall.drop - row.down), flow,
						top_depth[e]);
			}
			if (previous[0] >= 0) {
				// the same winding as the flat sheets had: bottom pair, then the row above
				for (int index : { current[0], current[1], previous[1], current[0], previous[1], previous[0] }) {
					r_out.first_indices.push_back(index);
				}
			}
			previous[0] = current[0];
			previous[1] = current[1];
		}
	}
	for (size_t i = 0; i < normal_sum.size(); i++) {
		if (normal_sum[i].length_squared() > 1e-10f) {
			r_out.normals.set(first + static_cast<int64_t>(i), normal_sum[i].normalized());
		}
	}
}

} // namespace godot
