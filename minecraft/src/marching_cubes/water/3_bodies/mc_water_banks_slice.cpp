// MCGrid: the water's banks and fall lines from slices of the tile meshes, baked once per mesh
// (the tiles' triangles from mc_water_banks_bake.cpp).
//
// The water surface always sits at the same height inside the tile at its waterline: a held body
// drop below its tile's top tier line, a stream 0.1 m above its ground. The bed's edge (the fall
// line) is always where the tile under the bed ends, just under its grass. So the rock the water
// needs is a horizontal slice of ONE tile mesh, at one of a few fixed heights:
//   1. cut the mesh's triangles (turned, cell-local) with the plane y = height: each triangle that
//      crosses it gives an edge segment
//   2. a signed distance field in the tile's own space, (BAKE + 1)^2 values in metres, >= 0 in the
//      rock: the distance to the nearest segment; rock or air from the tile's surface in 3D (the
//      side of the nearest triangle the point is on: under a top face or behind a wall is rock),
//      which also works where the slice has no edge or its edge leaves the tile. No tile at all
//      is air, a full-rock cell (hash 255) is rock, and the layer below the grid is the floor: rock
//      below its middle (ground 0's grass); above that, the lowest tiles' walls go on straight down
//   3. cached by (mesh + turn, height): baked once, shared by every cell using that tile, never
//      cleared by terrain edits (a placed tile's shape doesn't change)
// Which way triangles face (their winding) is settled once, from the first tile whose slice can be
// checked against its own heightmap.
#include "marching_cubes/grid/mc_grid.h"
#include "marching_cubes/water/3_bodies/mc_water_shape.h"
#include <algorithm>
#include <cmath>

namespace godot {

namespace {
const float FAR = 4.0f; // m: no edge near
const uint64_t AIR = 1; // slice keys of the two fields that need no mesh
const uint64_t ROCK = 2;

struct SliceSegment {
	Vector2 a; // x, z
	Vector2 b;
};

// The closest point to p on triangle a b c (Ericson, Real-Time Collision Detection 5.1.5).
Vector3 closest_on_triangle(
		const Vector3 &p,
		const Vector3 &a,
		const Vector3 &b,
		const Vector3 &c
) {
	Vector3 ab = b - a, ac = c - a, ap = p - a;
	float d1 = ab.dot(ap), d2 = ac.dot(ap);
	if (d1 <= 0 && d2 <= 0) {
		return a;
	}
	Vector3 bp = p - b;
	float d3 = ab.dot(bp), d4 = ac.dot(bp);
	if (d3 >= 0 && d4 <= d3) {
		return b;
	}
	float vc = d1 * d4 - d3 * d2;
	if (vc <= 0 && d1 >= 0 && d3 <= 0) {
		return a + ab * (d1 / (d1 - d3));
	}
	Vector3 cp = p - c;
	float d5 = ab.dot(cp), d6 = ac.dot(cp);
	if (d6 >= 0 && d5 <= d6) {
		return c;
	}
	float vb = d5 * d2 - d1 * d6;
	if (vb <= 0 && d2 >= 0 && d6 <= 0) {
		return a + ac * (d2 / (d2 - d6));
	}
	float va = d3 * d6 - d5 * d4;
	if (va <= 0 && (d4 - d3) >= 0 && (d5 - d6) >= 0) {
		return b + (c - b) * ((d4 - d3) / ((d4 - d3) + (d5 - d6)));
	}
	float denom = 1.0f / (va + vb + vc);
	return a + ab * (vb * denom) + ac * (vc * denom);
}

// Which side of the tile's surface p is on: > 0 on the side its triangles face (air, with winding
// +1). The nearest triangle decides; where several are about as near (an edge or corner of the
// surface), their facings are added, so a ridge or a valley gets the right side.
float facing_side(
		const std::vector<Vector3> &p_triangles,
		const Vector3 &p
) {
	float best = 1e9f;
	Vector3 closest;
	for (size_t i = 0; i + 2 < p_triangles.size(); i += 3) {
		Vector3 q = closest_on_triangle(p, p_triangles[i], p_triangles[i + 1], p_triangles[i + 2]);
		float d = p.distance_squared_to(q);
		if (d < best) {
			best = d;
			closest = q;
		}
	}
	Vector3 normal;
	float tie = std::sqrt(best) + 1e-4f;
	for (size_t i = 0; i + 2 < p_triangles.size(); i += 3) {
		const Vector3 &a = p_triangles[i];
		Vector3 q = closest_on_triangle(p, a, p_triangles[i + 1], p_triangles[i + 2]);
		if (p.distance_to(q) <= tie) {
			Vector3 n = (p_triangles[i + 1] - a).cross(p_triangles[i + 2] - a);
			if (n.length_squared() > 1e-12f) {
				normal += n.normalized();
			}
		}
	}
	return (p - closest).dot(normal);
}
} // namespace

const std::vector<float> &MCGrid::_slice_field(
		const Vector3i &p_cell,
		float p_height,
		uint64_t &r_key
) const {
	const int n = BAKE + 1;
	auto constant = [&](uint64_t p_key, float p_value) -> const std::vector<float> & {
		r_key = p_key;
		auto found = slice_fields.find(p_key);
		if (found == slice_fields.end()) {
			found = slice_fields.emplace(p_key, std::vector<float>(static_cast<size_t>(n * n), p_value)).first;
		}
		return found->second;
	};
	if (p_cell.y < 0) { // below the grid: the floor (ground 0's grass at the layer's middle)
		if (p_height < 0.0f) {
			return constant(ROCK, FAR);
		}
		// above the floor's grass but below every tile: the lowest tiles' walls go on straight down
		return _slice_field(Vector3i(p_cell.x, 0, p_cell.z), -0.49f, r_key);
	}
	uint8_t hash = get_cell_hash(p_cell);
	const MCBakedTile *tile = _baked_tile(p_cell);
	if (!tile) {
		return hash == 255 ? constant(ROCK, FAR) : constant(AIR, -FAR);
	}
	uint64_t tile_key = 0;
	for (const auto &entry : baked_tiles) { // the tile's mesh + turn key
		if (&entry.second == tile) {
			tile_key = entry.first;
			break;
		}
	}
	uint64_t key = (tile_key * 0x9E3779B97F4A7C15ULL) ^ static_cast<uint64_t>(std::lround((p_height + 2.0f) * 1024.0f)) ^ 0x5bd1e995ULL;
	r_key = key;
	auto found = slice_fields.find(key);
	if (found != slice_fields.end()) {
		return found->second;
	}
	// 1. the cut
	std::vector<SliceSegment> segments;
	const std::vector<Vector3> &t = tile->triangles;
	for (size_t i = 0; i + 2 < t.size(); i += 3) {
		const Vector3 p[3] = { t[i], t[i + 1], t[i + 2] };
		Vector2 cuts[2];
		int count = 0;
		for (int m = 0; m < 3 && count < 2; m++) {
			const Vector3 &a = p[m];
			const Vector3 &b = p[(m + 1) % 3];
			if ((a.y - p_height) * (b.y - p_height) < 0.0f) {
				float s = (p_height - a.y) / (b.y - a.y);
				cuts[count++] = Vector2(a.x + (b.x - a.x) * s, a.z + (b.z - a.z) * s);
			}
		}
		if (count < 2) {
			continue;
		}
		segments.push_back({ cuts[0], cuts[1] });
	}
	// 2. the field
	const float step = 1.0f / BAKE;
	std::vector<float> field(static_cast<size_t>(n * n));
	std::vector<int8_t> by_height(static_cast<size_t>(n * n)); // the heightmap's say: 1 rock, -1 air, 0 unknown
	for (int j = 0; j < n; j++) {
		for (int i = 0; i < n; i++) {
			size_t k = static_cast<size_t>(j * n + i);
			float h = tile->heights[k];
			by_height[k] = h <= NO_HIT ? 0 : (h >= p_height ? 1 : -1);
		}
	}
	if (t.empty()) {
		for (size_t k = 0; k < field.size(); k++) {
			field[k] = by_height[k] > 0 ? FAR : -FAR;
		}
		return slice_fields.emplace(key, std::move(field)).first->second;
	}
	int agree = 0;
	int disagree = 0;
	for (int j = 0; j < n; j++) {
		for (int i = 0; i < n; i++) {
			Vector2 p(-0.5f + i * step, -0.5f + j * step);
			float best = FAR; // the distance to the edge: the slice's segments
			for (const SliceSegment &sg : segments) {
				Vector2 ab = sg.b - sg.a;
				float u = ab.length_squared() > 1e-12f ? Math::clamp((p - sg.a).dot(ab) / ab.length_squared(), 0.0f, 1.0f) : 0.0f;
				best = std::min(best, p.distance_to(sg.a + ab * u));
			}
			// the side: the tile's surface in 3D (the slice alone can't tell where its edge leaves the tile)
			bool rock = facing_side(t, Vector3(p.x, p_height, p.y)) < 0.0f;
			size_t k = static_cast<size_t>(j * n + i);
			field[k] = rock ? best : -best;
			if (best > 0.03f && by_height[k] != 0) { // well away from the edge, where the heightmap knows
				(rock == (by_height[k] > 0) ? agree : disagree)++;
			}
		}
	}
	// the winding: settled by the first tile that can be checked, then kept; fields baked before
	// that assumed +1 and are flipped if it turns out -1
	if (slice_winding == 0 && agree + disagree >= 8) {
		slice_winding = agree >= disagree ? 1 : -1;
		if (slice_winding < 0) {
			for (uint64_t earlier : slice_undecided) {
				for (float &v : slice_fields[earlier]) {
					v = -v;
				}
			}
		}
		slice_undecided.clear();
	}
	if (slice_winding < 0) {
		for (float &v : field) {
			v = -v;
		}
	}
	if (slice_winding == 0) {
		slice_undecided.push_back(key);
	}
	return slice_fields.emplace(key, std::move(field)).first->second;
}

} // namespace godot
