// The water's 2D shape inside one terrain cell, shared by the two water mesh builders
// (mc_grid_water_mesh.cpp: per cell on the tiles' sub-grid; mc_grid_water_sampled.cpp: a fine
// sample grid + marching squares). A port of blender/mc_tiles/scripts/cliff_tiles.py's shape
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
const float BEHIND = 0.03f; // a fall checks for its water this far behind its top edge
const float MIN_FALL_RUN = 0.08f; // m: shorter straight runs of a fall are corner stubs, left out
const float WALL_GAP = 0.01f; // square tiles: falls stop this short of a wall (never in its plane)
const float ROUNDING = 0.15f; // blocky: radius of a corner that sticks out (as the tiles)
const float EPS = 1e-3f; // rounded: regions shrink this much (as the tiles)
const float CORNER_X[4] = { -0.5f, 0.5f, 0.5f, -0.5f };
const float CORNER_Y[4] = { -0.5f, -0.5f, 0.5f, 0.5f };
const float SAME = 0.02f;

// One layer of one cell: which corners are solid, and the shape style.
struct Layer {
	bool solid[4] = { false, false, false, false };
	bool blocky = false;
	float rounding = ROUNDING; // blocky: radius of a corner that sticks out (0: square tiles)

	bool all() const { return solid[0] && solid[1] && solid[2] && solid[3]; }
	bool none() const { return !solid[0] && !solid[1] && !solid[2] && !solid[3]; }
	int sub_grid() const { return blocky ? 11 : 9; } // the tiles' sub-grid per cell side
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

// Points into the solid.
inline Vector2 gradient(
		const Layer &p_layer,
		float p_x,
		float p_y
) {
	if (p_layer.blocky) {
		const float e = 1e-4f;
		return Vector2((blend(p_layer, p_x + e, p_y) - blend(p_layer, p_x - e, p_y)) / (2 * e),
				(blend(p_layer, p_x, p_y + e) - blend(p_layer, p_x, p_y - e)) / (2 * e));
	}
	const bool *c = p_layer.solid;
	float u = p_x + 0.5f;
	float v = p_y + 0.5f;
	return Vector2((float(c[1]) - float(c[0])) * (1 - v) + (float(c[2]) - float(c[3])) * v,
			(float(c[3]) - float(c[0])) * (1 - u) + (float(c[2]) - float(c[1])) * u);
}

// Clip a convex polygon to f >= 0 (f given at its points; new points sit on f = 0).
inline void clip(
		std::vector<Vector2> &r_points,
		const std::vector<float> &p_values
) {
	std::vector<Vector2> points;
	size_t count = r_points.size();
	for (size_t i = 0; i < count; i++) {
		size_t j = (i + 1) % count;
		float va = p_values[i];
		float vb = p_values[j];
		if (va >= 0) {
			points.push_back(r_points[i]);
		}
		if ((va >= 0) != (vb >= 0)) {
			points.push_back(r_points[i].lerp(r_points[j], va / (va - vb)));
		}
	}
	r_points = points;
}

// The sub-grid's triangles of a cell (the tiles' grid), as calls to p_each(triangle).
template <typename F>
inline void for_each_triangle(
		int p_n,
		F p_each
) {
	float step = 1.0f / static_cast<float>(p_n);
	for (int j = 0; j < p_n; j++) {
		for (int i = 0; i < p_n; i++) {
			float x0 = -0.5f + static_cast<float>(i) * step;
			float y0 = -0.5f + static_cast<float>(j) * step;
			const Vector2 a(x0, y0), b(x0 + step, y0), c(x0 + step, y0 + step), d(x0, y0 + step);
			p_each(a, b, c);
			p_each(a, c, d);
		}
	}
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

// One water body in one cell: its shape layers.
//   above  the terrain at the water's level and up (banks: the water tucks EDGE under them)
//   bed    the terrain at the bed's tier and up (the water stops EDGE past its edge, where the fall is)
//   wet    the body's own corners, the banks and lower ground: everything but dry ground of the
//          bed's tier, which the water must not stand over
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

	float height(const Vector2 &) const { return top; }
	Vector3 normal(const Vector2 &) const { return Vector3(0, 1, 0); }

	// Where the body's water is: not in a bank, on the bed, not over dry ground (>= 0 inside).
	float water_field(const Vector2 &p) const {
		float f = std::min(tuck() - blend(above, p.x, p.y), blend(bed, p.x, p.y) + lip_at(p));
		return wet.all() ? f : std::min(f, blend(wet, p.x, p.y));
	}

	// The sub-grid: the tiles' one, finer (odd) wherever the body is not a plain quad, the same in
	// every such cell so neighbours share their side vertices.
	int sub_grid() const { return above.sub_grid() * 2 + 1; }

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
