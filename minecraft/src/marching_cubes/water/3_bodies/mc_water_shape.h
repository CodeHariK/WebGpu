// The water's 2D shape inside one terrain cell: the bodies (mc_water_bodies.cpp) carry it, the
// water tiles (mc_water_tiles.cpp) sample it on a fine grid + marching squares. A port of blender/mc_tiles/scripts/cliff_tiles.py's shape
// functions (rounded bilinear blend, blocky quarters), so the water's edges land on the cliffs.
//
// Coordinates: cell-local (x, y) in -0.5 .. 0.5, corners 0 (-x, -y), 1 (+x, -y), 2 (+x, +y),
// 3 (-x, +y) in hash bit order; Godot z = -y.
#ifndef MC_WATER_SHAPE_H
#define MC_WATER_SHAPE_H

#include <godot_cpp/core/math.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/vector3.hpp>
#include <algorithm>
#include <cmath>
#include <vector>

namespace godot {
namespace mc_water {

const float EDGE = 0.05f; // the water runs this far on into the rock (shape units ~ metres)
const float WALL_GAP = 0.01f; // square tiles: falls stop this short of a wall (never in its plane)
const float ROUNDING = 0.15f; // blocky: radius of a corner that sticks out (as the tiles)
const float EPS = 1e-3f; // rounded: regions shrink this much (as the tiles)
const float CORNER_X[4] = { -0.5f, 0.5f, 0.5f, -0.5f };
const float CORNER_Y[4] = { -0.5f, -0.5f, 0.5f, 0.5f };
const int BAKED_SIDE = 16; // baked fields: squares per cell side ((BAKED_SIDE + 1)^2 values, MCGrid::BAKE)

// One layer of one cell: which corners are solid, and the shape style.
struct Layer {
	bool solid[4] = { false, false, false, false };
	bool blocky = false;
	float rounding = ROUNDING; // blocky: radius of a corner that sticks out (0: square tiles)

	bool all() const { return solid[0] && solid[1] && solid[2] && solid[3]; }
	bool none() const { return !solid[0] && !solid[1] && !solid[2] && !solid[3]; }
};

// >= 0 inside the layer's region (cliff_tiles.py: rounded / blocky).
inline float blend(
		const Layer &p_layer,
		float p_x,
		float p_y
) {
	const bool *c = p_layer.solid;
	if (!p_layer.blocky) {
		float u = p_x + 0.5f;
		float v = p_y + 0.5f;
		float value = c[0] * (1 - u) * (1 - v) + c[1] * u * (1 - v) + c[2] * u * v + c[3] * (1 - u) * v;
		return value - 0.5f - EPS;
	}
	float best = -1.0f;
	for (int i = 0; i < 4; i++) {
		if (!c[i]) {
			continue;
		}
		float a = CORNER_X[i] < 0 ? p_x : -p_x; // > 0: across the middle line, out of this quarter
		float b = CORNER_Y[i] < 0 ? p_y : -p_y;
		bool alone = !c[(i + 1) % 4] && !c[(i + 3) % 4];
		float sdf;
		if (alone) {
			float r = p_layer.rounding;
			float qa = std::max(a + r, 0.0f);
			float qb = std::max(b + r, 0.0f);
			sdf = std::sqrt(qa * qa + qb * qb) + std::min(std::max(a + r, b + r), 0.0f) - r;
		} else {
			sdf = std::max(a, b);
		}
		best = std::max(best, -sdf);
	}
	return best;
}

// The flow colour at a point: the 4 corners' flow blended bilinearly, direction mapped to 0..1.
inline Color flow_at(
		const Color p_corner_flow[4],
		float p_x,
		float p_y
) {
	float u = p_x + 0.5f;
	float v = p_y + 0.5f;
	Color c = p_corner_flow[0] * ((1 - u) * (1 - v)) + p_corner_flow[1] * (u * (1 - v)) + p_corner_flow[2] * (u * v) +
			p_corner_flow[3] * ((1 - u) * v);
	return Color(c.r * 0.5f + 0.5f, c.g * 0.5f + 0.5f, c.b, c.a);
}

// One water body in one cell: its corner layers. The banks' and the bed's edges come from the baked
// tile meshes (baked_above, baked_bed); the layers give the corner classes, the style, the saddle
// and the dry gaps.
//   above  the corners at the water's level and up (banks: the water tucks EDGE under them)
//   bed    the corners at the bed's tier and up (the water stops EDGE past its edge, where the fall is)
//   wet    the body's own corners, the banks and lower ground: everything but dry ground of the
//          bed's tier, which the water must not stand over (its shape: the rounded / blocky blend)
struct Body {
	Layer above;
	Layer bed;
	Layer wet;
	float top = 0.0f; // the surface height (flat)
	int ground[4] = { 0, 0, 0, 0 }; // the 4 corner columns' ground heights
	bool stream = false;
	bool square = false; // square tiles: falls stay WALL_GAP off the rock
	// how far the surface runs on into the rock / past the step: EDGE hides the rounded tiles'
	// grooves; square tiles have flat rock, so the water stops exactly at it (nothing pokes out)
	float tuck() const { return square ? 0.0f : EDGE; }
	// how far past the bed's edge the surface reaches: to the falls' line, so a fall's top edge sits
	// exactly on the surface's edge (one continuous sheet, no gap or step between them)
	float lip() const { return square ? WALL_GAP : EDGE; }
	// the surface's reach past the bed's edge at p: lip(), fading out right beside a bank so the
	// surface doesn't run along a wall's foot as a thin sliver (square tiles)
	float lip_at(const Vector2 &p) const {
		if (!square) {
			return EDGE;
		}
		float away = -blend(above, p.x, p.y); // metres from the bank
		return lip() * Math::clamp(away / WALL_GAP - 1.0f, 0.0f, 1.0f);
	}
	Color flow[4];
	// the falls' bevel radius (MCGrid::water_fall_lip): the surface stops this far short of the fall
	// line, and the fall strip's flat part and bevel cover from there (mc_water_fall_strips.cpp)
	float fall_lip = 0.0f;
	// The terrain from the baked tile meshes (mc_water_banks_slice.cpp): (BAKED_SIDE + 1)^2 signed
	// distances in metres over the cell (>= 0 inside the rock; index j * (BAKED_SIDE + 1) + i at
	// x = -0.5 + i / BAKED_SIDE, z = -0.5 + j / BAKED_SIDE, z = -y). Always set (MCGrid::_water_bodies).
	const float *baked_above = nullptr; // the rock above the water surface (the banks)
	const float *baked_bed = nullptr; // the rock of the bed's tier (the fall line is its edge)
	uint64_t baked_key = 0; // per-mesh slices: which two (tiles sharing them share a water tile)

	// A baked field at p, bilinear between its grid values.
	static float baked_at(
			const float *p_field,
			const Vector2 &p
	) {
		const int n = BAKED_SIDE + 1;
		float fx = Math::clamp((p.x + 0.5f) * BAKED_SIDE, 0.0f, static_cast<float>(BAKED_SIDE));
		float fz = Math::clamp((0.5f - p.y) * BAKED_SIDE, 0.0f, static_cast<float>(BAKED_SIDE));
		int i = std::min(static_cast<int>(fx), BAKED_SIDE - 1);
		int j = std::min(static_cast<int>(fz), BAKED_SIDE - 1);
		float u = fx - static_cast<float>(i);
		float v = fz - static_cast<float>(j);
		float a = p_field[j * n + i] * (1 - u) + p_field[j * n + i + 1] * u;
		float b = p_field[(j + 1) * n + i] * (1 - u) + p_field[(j + 1) * n + i + 1] * u;
		return a * (1 - v) + b * v;
	}
	// >= 0 inside a bank (metres from its edge).
	float above_at(const Vector2 &p) const { return baked_at(baked_above, p); }
	// >= 0 on the bed's tier (metres from its edge: the fall line).
	float bed_at(const Vector2 &p) const { return baked_at(baked_bed, p); }

	// Is corner k one of the body's own wet corners (not a bank, on the bed, wet)?
	bool own(int k) const { return wet.solid[k] && !above.solid[k] && bed.solid[k]; }

	// Rounded banks on a diagonal pair of corners make a saddle: two quarters touching at the cell's
	// centre, and the terrain tile closes that point with a thin rock neck. The water must not run
	// through it, or it flows on under the neck to the far side (its falls end up inside the rock):
	// with exactly one of the other two corners the body's own, the water keeps to that corner's side
	// of the line between the banks. That line runs inside the banks everywhere but the centre, so the
	// cut never shows. (>= 0 on the allowed side; a big value when there is no such saddle.)
	float saddle_side(const Vector2 &p) const {
		if (above.blocky) { // (baked banks too: the bake grid is too coarse to see the thin neck)
			return 1.0f; // blocky banks keep a real gap at the centre
		}
		for (int a = 0; a < 2; a++) {
			int b = a + 2; // the bank pair a, b; the other diagonal c, d
			int c = a + 1;
			int d = (a + 3) % 4;
			bool pair = above.solid[a] && above.solid[b] && !above.solid[c] && !above.solid[d];
			if (!pair || own(c) == own(d)) {
				continue;
			}
			int keep = own(c) ? c : d;
			Vector2 along(CORNER_X[b] - CORNER_X[a], CORNER_Y[b] - CORNER_Y[a]);
			Vector2 normal = Vector2(-along.y, along.x).normalized();
			if (normal.dot(Vector2(CORNER_X[keep], CORNER_Y[keep])) < 0.0f) {
				normal = -normal;
			}
			return normal.dot(p); // the line passes through the centre
		}
		return 1.0f;
	}

	// Where the body's water is: not in a bank, on the bed, not over dry ground (>= 0 inside).
	float water_field(const Vector2 &p) const {
		float f = std::min(tuck() - above_at(p), bed_at(p) + lip_at(p) - fall_lip);
		f = std::min(f, saddle_side(p));
		if (wet.all()) {
			return f;
		}
		// a body with none of its own corners (all banks / lower ground) holds no water
		bool any_own = own(0) || own(1) || own(2) || own(3);
		return std::min(f, any_own ? blend(wet, p.x, p.y) : -1e6f);
	}

	// The terrain's ground surface height at p (the tiles' grass at tier t is at y t - 0.5): the
	// highest tier whose layer covers p.
	float ground_at(const Vector2 &p) const {
		int best = 0;
		for (int k = 0; k < 4; k++) {
			Layer layer = above;
			for (int j = 0; j < 4; j++) {
				layer.solid[j] = ground[j] >= ground[k];
			}
			if (ground[k] > best && blend(layer, p.x, p.y) >= 0.0f) {
				best = ground[k];
			}
		}
		return static_cast<float>(best) - 0.5f;
	}
};

} // namespace mc_water
} // namespace godot

#endif // MC_WATER_SHAPE_H
