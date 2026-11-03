// MCGrid: waterfalls as one continuous mesh with the water surface (the water tiles builder; Animal
// Crossing style). Seen side-on:
//
//   surface ─────╮      lip    the SURFACE itself, bent: every surface vertex within water_fall_lip
//                │             of a fall line is lowered onto a quarter ellipse, flat where the lip
//                │             starts and straight down at the fall line
//                │      drop   hangs from the surface's own edge vertices (the fall line points are
//                │             surface vertices), straight down
//                ╰──    foot   a short flare out into the water below
//
// So it is one mesh with shared vertices from the flat water over the edge and down: no seam.
//
// UV is continuous too: on the surface it is the world x, z (bent vertices are pushed on along the
// fall's direction by the extra length of the curve, so the texture isn't squeezed over the lip);
// down the fall it carries on "unfolded", as if the fall were laid flat beyond the edge. One flow
// texture runs across the pool, over the lip and down. UV2 (water.gdshader): x how far over the lip
// (0 flat .. 1 straight down), y metres above the landing. COLOR: the flow (the drop: its own
// direction, flowing). The drop's triangles go in MCWaterArrays::first_indices (drawn first).
#include "marching_cubes/grid/mc_grid.h"
#include <godot_cpp/templates/hash_map.hpp>
#include <algorithm>
#include <cmath>
#include <vector>

namespace godot {

namespace {
const int FOOT_ROWS = 2; // rows of the flare at the foot
const float DROP_ROW = 0.25f; // m: the straight drop is cut into rows at most this tall
const float FOOT = 0.12f; // m the foot flares out (and up the fall it starts)
const float HALF_PI = 1.5707963f;
const float SAME_SURFACE = 1e-3f; // m: a vertex is on a fall's surface when its height is this close
const float WELD_SCALE = 4096.0f;
const float SMOOTH = 0.4f; // m: a fall's direction is averaged over its neighbours this far along

Vector3i weld(const Vector3 &p_v) {
	return Vector3i(static_cast<int>(std::lround(p_v.x * WELD_SCALE)), static_cast<int>(std::lround(p_v.y * WELD_SCALE)),
			static_cast<int>(std::lround(p_v.z * WELD_SCALE)));
}

// The lip of a fall: a quarter ellipse, p_across wide, p_down deep.
struct Lip {
	float across = 0.0f;
	float down = 0.0f;
	// the curve's length from its start to angle p_angle (0 flat .. pi/2 straight down)
	float length(float p_angle) const {
		const int steps = 8;
		float total = 0.0f;
		for (int i = 0; i < steps; i++) {
			float t = (static_cast<float>(i) + 0.5f) / static_cast<float>(steps) * p_angle;
			float dx = across * std::cos(t);
			float dy = down * std::sin(t);
			total += std::sqrt(dx * dx + dy * dy);
		}
		return total * p_angle / static_cast<float>(steps);
	}
};

Lip fall_lip(
		float p_radius,
		float p_drop
) {
	Lip lip;
	lip.across = p_radius;
	lip.down = std::min(p_radius, p_drop * 0.5f); // a short fall gets a flatter lip
	return lip;
}

// The flow COLOR of water running in p_direction (the drop).
Color falling_flow(const Vector3 &p_direction) {
	return Color(p_direction.x * 0.5f + 0.5f, p_direction.z * 0.5f + 0.5f, 1.0f, 0.0f);
}

// Squared distance in x, z from p to segment a - b (r_t: where along it, 0 at a .. 1 at b).
float distance_xz_squared(
		const Vector3 &p,
		const Vector3 &a,
		const Vector3 &b,
		float *r_t = nullptr
) {
	Vector2 ab(b.x - a.x, b.z - a.z);
	Vector2 ap(p.x - a.x, p.z - a.z);
	float length_squared = ab.length_squared();
	float t = length_squared > 1e-12f ? std::clamp(ap.dot(ab) / length_squared, 0.0f, 1.0f) : 0.0f;
	if (r_t) {
		*r_t = t;
	}
	return (ap - ab * t).length_squared();
}

// One row of the drop below its first row: out and down from the first row, normal (out, up),
// metres along the strip from the first row.
struct Row {
	float out;
	float down;
	Vector2 normal;
	float along;
};

// The drop's rows for p_below metres from the first row to the landing.
std::vector<Row> drop_rows(float p_below) {
	std::vector<Row> rows;
	float foot = std::min(FOOT, p_below * 0.5f);
	if (foot <= 1e-4f) {
		int count = std::max(1, static_cast<int>(std::ceil(p_below / DROP_ROW)));
		for (int i = 1; i <= count; i++) {
			float down = p_below * static_cast<float>(i) / static_cast<float>(count);
			rows.push_back({ 0.0f, down, Vector2(1, 0), down });
		}
		return rows;
	}
	// straight down in rows of at most DROP_ROW (long thin triangles shear the unfolded texture)
	float straight = p_below - foot;
	int count = std::max(1, static_cast<int>(std::ceil(straight / DROP_ROW)));
	for (int i = 1; i <= count; i++) {
		float down = straight * static_cast<float>(i) / static_cast<float>(count);
		rows.push_back({ 0.0f, down, Vector2(1, 0), down });
	}
	for (int i = 1; i <= FOOT_ROWS; i++) {
		float a = static_cast<float>(i) / static_cast<float>(FOOT_ROWS) * HALF_PI;
		Row row = { foot * (1.0f - std::cos(a)), p_below - foot + foot * std::sin(a), Vector2(std::cos(a), std::sin(a)), 0.0f };
		const Row &last = rows.back();
		row.along = last.along + Vector2(row.out - last.out, row.down - last.down).length();
		rows.push_back(row);
	}
	return rows;
}
} // namespace

// Metres along each chain of joined falls, per top point (a walk from each chain's end, then from
// any loop left): the streaks' across coordinate, so they keep their spacing round corners.
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

// The lip: every surface vertex within water_fall_lip of a fall line (of its own surface) is
// lowered onto the fall's quarter ellipse, its normal turned with the curve, its UV pushed on by
// the curve's extra length, its UV2 set to how far over it is.
void MCGrid::_bend_lips(
		const std::vector<PlacedFall> &p_falls,
		const HashMap<Vector3i, float> &p_chain,
		MCWaterArrays &r_out
) const {
	const float radius = water_fall_lip;
	if (radius <= 0.0f) {
		return;
	}
	// the falls by grid cell (each in every cell its segment comes within the lip of)
	HashMap<Vector2i, std::vector<int>> near;
	for (int f = 0; f < static_cast<int>(p_falls.size()); f++) {
		const PlacedFall &fall = p_falls[f];
		int x0 = static_cast<int>(std::floor(std::min(fall.ta.x, fall.tb.x) - radius));
		int x1 = static_cast<int>(std::floor(std::max(fall.ta.x, fall.tb.x) + radius));
		int z0 = static_cast<int>(std::floor(std::min(fall.ta.z, fall.tb.z) - radius));
		int z1 = static_cast<int>(std::floor(std::max(fall.ta.z, fall.tb.z) + radius));
		for (int z = z0; z <= z1; z++) {
			for (int x = x0; x <= x1; x++) {
				Vector2i cell(x, z);
				if (!near.has(cell)) {
					near.insert(cell, std::vector<int>());
				}
				near[cell].push_back(f);
			}
		}
	}
	for (int i = 0; i < static_cast<int>(r_out.vertices.size()); i++) {
		Vector3 v = r_out.vertices[i];
		const std::vector<int> *list = near.getptr(Vector2i(static_cast<int>(std::floor(v.x)), static_cast<int>(std::floor(v.z))));
		if (!list) {
			continue;
		}
		int nearest = -1;
		float best = radius * radius;
		float best_t = 0.0f;
		for (int f : *list) {
			const PlacedFall &fall = p_falls[f];
			if (std::abs(fall.ta.y - v.y) > SAME_SURFACE) {
				continue; // another body's fall
			}
			float t = 0.0f;
			float d = distance_xz_squared(v, fall.ta, fall.tb, &t);
			if (d < best) {
				best = d;
				best_t = t;
				nearest = f;
			}
		}
		if (nearest < 0) {
			continue;
		}
		const PlacedFall &fall = p_falls[nearest];
		Lip lip = fall_lip(radius, fall.drop);
		// the curve x = across sin(a), y = down (1 - cos(a)) from the lip's start; the vertex is
		// (across - its distance to the fall line) on from the start
		float angle = std::asin(std::clamp(1.0f - std::sqrt(best) / radius, 0.0f, 1.0f));
		float down = lip.down * (1.0f - std::cos(angle));
		Vector3 normal = (fall.out * (lip.down * std::sin(angle)) + Vector3(0, lip.across * std::cos(angle), 0)).normalized();
		float stretch = lip.length(angle) - lip.across * std::sin(angle); // the curve is longer than its run
		r_out.vertices.set(i, v - Vector3(0, down, 0));
		r_out.normals.set(i, normal);
		r_out.uvs.set(i, r_out.uvs[i] + Vector2(fall.out.x, fall.out.z) * stretch);
		r_out.uv2s.set(i, Vector2(angle / HALF_PI, fall.drop - down));
		const float *ua = p_chain.getptr(weld(fall.ta));
		const float *ub = p_chain.getptr(weld(fall.tb));
		if (ua && ub) {
			r_out.fall_u.set(i, Math::lerp(*ua, *ub, best_t));
		}
	}
}

void MCGrid::_emit_fall_strips(
		const std::vector<PlacedFall> &p_falls,
		const HashMap<Vector3i, int> &p_surface_index,
		MCWaterArrays &r_out
) const {
	r_out.fall_u.resize(r_out.vertices.size()); // the surface: 0 (only read where the water falls)
	r_out.fall_u.fill(0.0f);
	HashMap<Vector3i, float> chain = _fall_chain(p_falls);
	_bend_lips(p_falls, chain, r_out);

	// the direction at each top point, smoothed along the fall: the falls of the same surface within
	// SMOOTH of it, weighted by their length (the edge from the sample grid jitters from point to
	// point; unfolding the UV down a tall fall along that jitter would shear it into triangles)
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
	HashMap<Vector3i, std::vector<std::pair<Vector3, int>>> made; // the drop's own vertices: (normal, index)
	auto vertex = [&](const Vector3 &p_position, const Vector3 &p_normal, const Vector2 &p_uv, const Vector2 &p_uv2,
						  const Color &p_flow, float p_u) {
		std::vector<std::pair<Vector3, int>> &list = made[weld(p_position)];
		for (const std::pair<Vector3, int> &e : list) {
			if (e.first.dot(p_normal) > 0.999f) {
				return e.second;
			}
		}
		int index = static_cast<int>(r_out.vertices.size());
		r_out.vertices.push_back(p_position);
		r_out.normals.push_back(p_normal);
		r_out.uvs.push_back(p_uv);
		r_out.uv2s.push_back(p_uv2);
		r_out.colors.push_back(p_flow);
		r_out.fall_u.push_back(p_u);
		list.push_back({ p_normal, index });
		return index;
	};
	for (const PlacedFall &fall : p_falls) {
		Lip lip = fall_lip(water_fall_lip, fall.drop);
		const Vector3 *top[2] = { &fall.ta, &fall.tb };
		const Color *top_flow[2] = { &fall.flow_a, &fall.flow_b };
		int row0[2];
		Vector3 direction[2]; // the smoothed direction at each end (normals, unfolding, flow)
		Vector3 push[2]; // mitred, so every fall at a corner still moves out by its own amount
		float below[2]; // metres from the first row down to the landing
		float u[2]; // metres along the joined falls (the streaks' across coordinate)
		for (int e = 0; e < 2; e++) {
			Vector3i key = weld(*top[e]);
			direction[e] = smoothed(*top[e], fall.out);
			u[e] = chain.has(key) ? chain[key] : 0.0f;
			push[e] = direction[e] / std::max(direction[e].dot(fall.out), 0.5f);
			// the first row is the surface's own edge vertex (bent to the bottom of the lip), so the
			// surface and the fall share it; an end moved by water_expand gets a vertex of its own
			if (const int *index = p_surface_index.getptr(key)) {
				row0[e] = *index;
			} else {
				Vector3 position = *top[e] - Vector3(0, lip.down, 0);
				Vector2 uv = Vector2(position.x, position.z) +
						Vector2(direction[e].x, direction[e].z) * (lip.length(HALF_PI) - lip.across);
				row0[e] = vertex(position, direction[e], uv, Vector2(1.0f, fall.drop - lip.down), *top_flow[e], u[e]);
			}
			below[e] = r_out.vertices[row0[e]].y - (top[e]->y - fall.drop);
		}
		float most = std::max(below[0], below[1]);
		if (most <= 1e-3f) {
			continue;
		}
		int previous[2] = { row0[0], row0[1] };
		for (const Row &row : drop_rows(most)) {
			int current[2];
			for (int e = 0; e < 2; e++) {
				float scale = below[e] / most; // both ends land on the landing
				Vector3 start = r_out.vertices[row0[e]];
				Vector3 position = start + push[e] * row.out - Vector3(0, row.down * scale, 0);
				Vector3 normal = (direction[e] * row.normal.x + Vector3(0, row.normal.y, 0)).normalized();
				Vector2 uv = r_out.uvs[row0[e]] + Vector2(direction[e].x, direction[e].z) * (row.along * scale); // unfolded
				current[e] = vertex(position, normal, uv, Vector2(1.0f, below[e] - row.down * scale),
						falling_flow(direction[e]), u[e]);
			}
			// the same winding as the flat sheets: bottom pair, then the row above
			for (int index : { current[0], current[1], previous[1], current[0], previous[1], previous[0] }) {
				r_out.first_indices.push_back(index);
			}
			previous[0] = current[0];
			previous[1] = current[1];
		}
	}
}

} // namespace godot
