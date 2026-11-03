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
// strips share their end columns: each top point's direction is smoothed along the fall and every
// strip offsets that point by it, so neighbours meet exactly and a strip wraps a corner in one piece;
// vertices are welded by position with averaged normals, so the light has no seams either.
#include "marching_cubes/grid/mc_grid.h"
#include <godot_cpp/templates/hash_map.hpp>
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

// The profile of a fall p_drop high with bevel radius p_radius.
std::vector<Row> profile(
		float p_radius,
		float p_drop
) {
	std::vector<Row> rows;
	float r = std::min(p_radius, p_drop * 0.5f); // a short fall gets a smaller bevel
	auto push = [&](float p_out, float p_down, const Vector2 &p_normal, float p_over) {
		Row row = { p_out, p_down, p_normal.normalized(), 0.0f, p_over };
		if (!rows.empty()) {
			row.along = rows.back().along + Vector2(p_out - rows.back().out, p_down - rows.back().down).length();
		}
		rows.push_back(row);
	};
	// flat, just above the surface
	push(-r - FLAT, -LIFT, Vector2(0, 1), 0.0f);
	// the bevel: a quarter circle centred (-r, r) below the top, from flat to straight down
	for (int i = 0; i <= BEVEL_ROWS; i++) {
		float a = static_cast<float>(i) / static_cast<float>(BEVEL_ROWS) * HALF_PI;
		float lift = LIFT * (1.0f - static_cast<float>(i) / static_cast<float>(BEVEL_ROWS));
		push(-r + r * std::sin(a), r * (1.0f - std::cos(a)) - lift, Vector2(std::sin(a), std::cos(a)), a / HALF_PI);
	}
	// the drop, then the foot
	float below = p_drop - r;
	float foot = std::min(FOOT, below * 0.5f);
	float straight = below - foot;
	int count = std::max(1, static_cast<int>(std::ceil(straight / DROP_ROW)));
	for (int i = 1; i <= count && straight > 1e-4f; i++) {
		push(0.0f, r + straight * static_cast<float>(i) / static_cast<float>(count), Vector2(1, 0), 1.0f);
	}
	for (int i = 1; i <= FOOT_ROWS && foot > 1e-4f; i++) {
		float a = static_cast<float>(i) / static_cast<float>(FOOT_ROWS) * HALF_PI;
		push(foot * (1.0f - std::cos(a)), p_drop - foot + foot * std::sin(a), Vector2(std::cos(a), std::sin(a)), 1.0f);
	}
	return rows;
}
} // namespace

// Metres along each chain of joined falls, per top point (a walk from each chain's end, then from
// any loop left): the strips' UV.x, so the texture keeps its spacing round corners.
HashMap<Vector3i, float> MCGrid::_fall_chain(const std::vector<PlacedFall> &p_falls) const {
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
	HashMap<Vector3i, float> along;
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
						Vector3i other = weld(fall.ta) == key ? weld(fall.tb) : weld(fall.ta);
						if (!along.has(other)) {
							along.insert(other, along[key] + fall.ta.distance_to(fall.tb));
							stack.push_back(other);
						}
					}
				}
			}
		}
	}
	return along;
}

void MCGrid::set_water_fall_lip(float p_lip) {
	water_fall_lip = std::clamp(p_lip, 0.0f, 0.4f);
	if (!chunks.empty() && water_levels.size() == column_heights.size()) {
		_queue_water();
	}
}

void MCGrid::_emit_fall_strips(
		const std::vector<PlacedFall> &p_falls,
		MCWaterArrays &r_out
) const {
	HashMap<Vector3i, float> chain = _fall_chain(p_falls);

	// each top point's direction, smoothed along the fall (the edge from the sample grid jitters)
	HashMap<Vector2i, std::vector<int>> by_cell;
	for (int f = 0; f < static_cast<int>(p_falls.size()); f++) {
		Vector3 mid = (p_falls[f].ta + p_falls[f].tb) * 0.5f;
		Vector2i cell(static_cast<int>(std::floor(mid.x)), static_cast<int>(std::floor(mid.z)));
		if (!by_cell.has(cell)) {
			by_cell.insert(cell, std::vector<int>());
		}
		by_cell[cell].push_back(f);
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

	// the strips' vertices, welded by position only: neighbouring strips share their end columns,
	// and a shared vertex gets the average of their normals, so the light blends across the join
	// instead of jumping (normals summed here, normalised at the end)
	HashMap<Vector3i, int> made;
	int first = static_cast<int>(r_out.vertices.size());
	std::vector<Vector3> normal_sum;
	auto vertex = [&](const Vector3 &p_position, const Vector3 &p_normal, const Vector2 &p_uv, const Vector2 &p_uv2,
						  const Color &p_flow, float p_depth) {
		Vector3i key = weld(p_position);
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
	for (const PlacedFall &fall : p_falls) {
		std::vector<Row> rows = profile(water_fall_lip, fall.drop);
		const Vector3 *top[2] = { &fall.ta, &fall.tb };
		const Color *top_flow[2] = { &fall.flow_a, &fall.flow_b };
		const float top_depth[2] = { fall.depth_a, fall.depth_b };
		Vector3 direction[2];
		float u[2];
		for (int e = 0; e < 2; e++) {
			direction[e] = smoothed(*top[e], fall.out);
			const float *along = chain.getptr(weld(*top[e]));
			u[e] = along ? *along : 0.0f;
		}
		int previous[2] = { -1, -1 };
		for (const Row &row : rows) {
			int current[2];
			for (int e = 0; e < 2; e++) {
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
