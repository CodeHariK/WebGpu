// MCGrid: the water surface and falls, built cell by cell with the same shape as the cliff tiles.
//
// The terrain is drawn on the dual grid: a cell sits between 4 corner columns and its tile's edges
// come from a 2D shape function of those 4 corners (blender/mc_tiles/scripts/cliff_tiles.py: the
// rounded bilinear blend, or the blocky quarters). Water used to be a square per column, so its
// edges and falls were straight where the cliffs are curved, and bits of ground poked through.
// Here the water uses the very same shape function, on the same sub-grid, so its edges land on the
// cliff edges exactly:
//
//   layer t      the corners whose ground is at least t (solid), shaped by the blend: inside it the
//                terrain stands at tier t or higher
//   a water body at level L in a cell (its wet corners), over its bed tier B (the lowest ground of
//   those corners; one tier below L for poured water):
//     surface    where the terrain is below L and at least B: outside layer L, inside layer B —
//                clipped per sub-grid triangle like the tiles' grass, at the body's surface height;
//                and only over the body's own corners (never over dry ground of the bed's tier)
//     fall       along the edge of layer B (the step down at the bed's edge), where it borders the
//                water: a sheet from the surface down to the lower water (or dry ground) beside it
//
// Both edges run EDGE on into the rock, so the grooves cut in the cliff face (up to 4.5 cm) never
// leave a gap, and the upper and lower water overlap under the fall. A cell one body covers whole is
// a single quad (the open sea, the middle of a pond); only shore cells use the sub-grid. The shape
// per cell follows its group: groups listed in water_blocky_groups are blocky.
//
// Coordinates: the shape works in the tiles' Blender plane (x, y), cell-local -0.5 .. 0.5, corners
// 0 (-x, -y), 1 (+x, -y), 2 (+x, +y), 3 (-x, +y) in hash bit order; Godot z = -y, so corner 0 is the
// column (x, z + 1), 1 (x + 1, z + 1), 2 (x + 1, z), 3 (x, z).
#include "marching_cubes/grid/mc_grid.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace godot {

namespace {
const float EDGE = 0.05f; // the water runs this far on into the rock (shape units ~ metres)
const float BEHIND = 0.03f; // a fall checks for its water this far behind its top edge
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
float blend(
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
Vector2 gradient(
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
void clip(
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
void for_each_triangle(
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
Color flow_at(
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
	bool stream = false;
	bool square = false; // square tiles: falls stay WALL_GAP off the rock
	// how far the surface runs on into the rock / past the step: EDGE hides the rounded tiles'
	// grooves; square tiles have flat rock, so the water stops exactly at it (nothing pokes out)
	float tuck() const { return square ? 0.0f : EDGE; }
	Color flow[4];

	float height(const Vector2 &) const { return top; }
	Vector3 normal(const Vector2 &) const { return Vector3(0, 1, 0); }

	// Where the body's water is: not in a bank, on the bed, not over dry ground (>= 0 inside).
	float water_field(const Vector2 &p) const {
		float f = std::min(tuck() - blend(above, p.x, p.y), blend(bed, p.x, p.y) + tuck());
		return wet.all() ? f : std::min(f, blend(wet, p.x, p.y));
	}

	// The sub-grid: the tiles' one, finer (odd) wherever the body is not a plain quad, the same in
	// every such cell so neighbours share their side vertices.
	int sub_grid() const { return above.sub_grid() * 2 + 1; }
};

// The body's surface in one cell. A cell it covers whole is one quad; otherwise the sub-grid,
// clipped like the tiles' grass.
void emit_body(
		const Vector2 &p_centre,
		const Body &p_body,
		MCWaterArrays &r_out
) {
	auto emit_polygon = [&](const std::vector<Vector2> &p_points) {
		for (size_t k = 1; k + 1 < p_points.size(); k++) {
			for (const Vector2 &p : { p_points[0], p_points[k], p_points[k + 1] }) {
				Vector3 v(p_centre.x + p.x, p_body.height(p), p_centre.y - p.y);
				r_out.vertices.push_back(v);
				r_out.normals.push_back(p_body.normal(p));
				r_out.uvs.push_back(Vector2(v.x, v.z));
				r_out.uv2s.push_back(Vector2(p_body.stream ? 1.0f : 0.0f, 0));
				r_out.colors.push_back(flow_at(p_body.flow, p.x, p.y));
			}
		}
	};
	if (p_body.above.none() && p_body.bed.all() && p_body.wet.all()) {
		emit_polygon({ Vector2(-0.5f, -0.5f), Vector2(0.5f, -0.5f), Vector2(0.5f, 0.5f), Vector2(-0.5f, 0.5f) });
		return;
	}
	for_each_triangle(p_body.sub_grid(), [&](const Vector2 &a, const Vector2 &b, const Vector2 &c) {
		std::vector<Vector2> points = { a, b, c };
		const Layer *layers[3] = { &p_body.above, &p_body.bed, &p_body.wet };
		for (int pass = 0; pass < 3 && points.size() >= 3; pass++) {
			if (pass == 2 && p_body.wet.all()) {
				break;
			}
			std::vector<float> values;
			for (const Vector2 &p : points) {
				float bv = blend(*layers[pass], p.x, p.y);
				values.push_back(pass == 0 ? p_body.tuck() - bv : (pass == 1 ? bv + p_body.tuck() : bv));
			}
			clip(points, values);
		}
		if (points.size() >= 3) {
			emit_polygon(points);
		}
	});
}

// The fall at the edge of the bed's layer (EDGE past it) wherever the body's water reaches it: a
// vertical sheet per contour segment from the surface down to p_bottom, facing down the step.
void emit_fall(
		const Vector2 &p_centre,
		const Body &p_body,
		float p_bottom,
		MCWaterArrays &r_out
) {
	if (p_body.bed.all() || p_body.bed.none()) {
		return; // no edge in this cell
	}
	// square tiles: WALL_GAP off the step's rock face (never in its plane), else EDGE past it
	auto edge_field = [&](const Vector2 &p) { return blend(p_body.bed, p.x, p.y) + (p_body.square ? WALL_GAP : EDGE); };
	// the part of the water other than the bed's edge: not in a bank, not over dry ground
	auto reach = [&](const Vector2 &p) {
		float f = (p_body.square ? -WALL_GAP : EDGE) - blend(p_body.above, p.x, p.y);
		return p_body.wet.all() ? f : std::min(f, blend(p_body.wet, p.x, p.y));
	};
	for_each_triangle(p_body.sub_grid(), [&](const Vector2 &t0, const Vector2 &t1, const Vector2 &t2) {
		const Vector2 tri[3] = { t0, t1, t2 };
		Vector2 cross[2];
		int found = 0;
		for (int e = 0; e < 3 && found < 2; e++) {
			Vector2 a = tri[e];
			Vector2 b = tri[(e + 1) % 3];
			float va = edge_field(a);
			float vb = edge_field(b);
			if ((va >= 0) != (vb >= 0)) {
				cross[found++] = a.lerp(b, va / (va - vb));
			}
		}
		if (found != 2) {
			return;
		}
		Vector2 a = cross[0];
		Vector2 b = cross[1];
		// cut the segment to where the water is
		float wa = reach(a);
		float wb = reach(b);
		if (wa < 0 && wb < 0) {
			return;
		}
		if (p_body.square && wa < WALL_GAP && wb < WALL_GAP) {
			return; // along a bank: that is the wall, not a fall
		}
		if (wa < 0) {
			a = b.lerp(a, wb / (wb - wa));
		} else if (wb < 0) {
			b = a.lerp(b, wa / (wa - wb));
		}
		float top_a = p_body.height(a);
		float top_b = p_body.height(b);
		top_a = std::max(top_a, p_bottom);
		top_b = std::max(top_b, p_bottom);
		// facing down the step: away from the bed (the gradient points into it); Godot z = -y
		Vector2 g = gradient(p_body.bed, (a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f);
		// only where this body's surface is right behind the edge: a step beside dry ground of the bed
		// tier (no water up there) has no fall, it is just the terrain's cliff
		if (g.length_squared() > 1e-12f) {
			Vector2 behind = (a + b) * 0.5f + g.normalized() * BEHIND;
			if (p_body.water_field(behind) < 0.0f) {
				return;
			}
		}
		Vector3 out(-g.x, 0, g.y);
		out = out.length_squared() > 1e-12f ? out.normalized() : Vector3(1, 0, 0);
		Vector3 pa(p_centre.x + a.x, 0, p_centre.y - a.y);
		Vector3 pb(p_centre.x + b.x, 0, p_centre.y - b.y);
		const Vector3 sheet[4] = { pa + Vector3(0, p_bottom, 0), pb + Vector3(0, p_bottom, 0), pb + Vector3(0, top_b, 0),
			pa + Vector3(0, top_a, 0) };
		// UV: x along the ground (metres, continuous between segments), y metres down from the top;
		// UV2.x: metres above the landing
		float ua = pa.x + pa.z;
		float ub = pb.x + pb.z;
		float drop_a = top_a - p_bottom;
		float drop_b = top_b - p_bottom;
		const Vector2 uv[4] = { Vector2(ua, drop_a), Vector2(ub, drop_b), Vector2(ub, 0), Vector2(ua, 0) };
		const Vector2 uv2[4] = { Vector2(0, 0), Vector2(0, 0), Vector2(drop_b, 0), Vector2(drop_a, 0) };
		for (int k : { 0, 1, 2, 0, 2, 3 }) {
			r_out.fall_vertices.push_back(sheet[k]);
			r_out.fall_normals.push_back(out);
			r_out.fall_uvs.push_back(uv[k]);
			r_out.fall_uv2s.push_back(uv2[k]);
		}
	});
}
} // namespace

void MCGrid::set_water_square(bool p_on) {
	water_square = p_on;
	if (!chunks.empty() && water_levels.size() == column_heights.size()) {
		_queue_water();
	}
}

void MCGrid::set_water_blocky_groups(const String &p_groups) {
	water_blocky_groups = p_groups;
	if (!chunks.empty() && water_levels.size() == column_heights.size()) {
		_queue_water(); // not while the scene is still loading (the grid isn't built yet)
	}
}

// Build the water of every cell into r_out (see the top of this file).
void MCGrid::_emit_water_cells(
		const std::vector<float> &p_surface,
		const std::vector<uint8_t> &p_held,
		const std::vector<Color> &p_flow,
		MCWaterArrays &r_out
) const {
	Vector2i columns = _column_counts();
	int cell_levels = std::max(_dims().max_corner().y, 1);
	for (int cz = 0; cz + 1 < columns.y; cz++) {
		for (int cx = 0; cx + 1 < columns.x; cx++) {
			// the 4 corner columns, in hash bit order
			const Vector2i at[4] = { Vector2i(cx, cz + 1), Vector2i(cx + 1, cz + 1), Vector2i(cx + 1, cz), Vector2i(cx, cz) };
			size_t index[4];
			int ground[4];
			int level[4];
			bool wet[4];
			bool any_wet = false;
			for (int i = 0; i < 4; i++) {
				index[i] = static_cast<size_t>(_column_index(at[i].x, at[i].y));
				ground[i] = column_heights[index[i]];
				wet[i] = _is_wet(static_cast<int>(index[i]));
				level[i] = wet[i] ? water_levels[index[i]] : 0;
				any_wet = any_wet || wet[i];
			}
			if (!any_wet) {
				continue;
			}
			Vector2 centre(static_cast<float>(cx) + 0.5f, static_cast<float>(cz) + 0.5f);
			// each water level in the cell is a body (done from its first corner)
			for (int i = 0; i < 4; i++) {
				bool first = wet[i];
				for (int k = 0; k < i && first; k++) {
					first = !(wet[k] && level[k] == level[i]);
				}
				if (!first) {
					continue;
				}
				int body_level = level[i];
				int bed = 255;
				float top = -1e6f;
				bool stream = false;
				Color body_flow(0, 0, 0, 0);
				int members = 0;
				for (int k = 0; k < 4; k++) {
					if (wet[k] && level[k] == body_level) {
						bed = std::min(bed, ground[k]);
						top = std::max(top, p_surface[index[k]]);
						stream = stream || !p_held[index[k]];
						body_flow += p_flow[index[k]];
						members++;
					}
				}
				body_flow = body_flow * (1.0f / static_cast<float>(members));
				Color corner_flow[4];
				for (int k = 0; k < 4; k++) {
					corner_flow[k] = wet[k] && level[k] == body_level ? p_flow[index[k]] : body_flow;
				}
				// the shape style of the bed's cell (its group)
				char group = _group_at(Vector3i(cx, std::clamp(bed - 1, 0, cell_levels - 1), cz));
				bool blocky = water_blocky_groups.contains(String::chr(static_cast<char32_t>(group)));
				Body body;
				body.above.blocky = blocky;
				body.bed.blocky = blocky;
				body.wet.blocky = blocky;
				float rounding = water_square ? 0.0f : ROUNDING;
				body.above.rounding = rounding;
				body.bed.rounding = rounding;
				body.wet.rounding = rounding;
				for (int k = 0; k < 4; k++) {
					body.above.solid[k] = ground[k] >= body_level;
					body.bed.solid[k] = ground[k] >= bed;
					body.wet.solid[k] = body.above.solid[k] || (wet[k] && level[k] == body_level) || ground[k] < bed;
					body.flow[k] = corner_flow[k];
				}
				body.top = top;
				// just under the dry ground of the level's bed tier (one below the level): the same in
				// every cell of this level, so the skirt matches across cell sides
				body.square = water_square;
				body.stream = stream;
				emit_body(centre, body, r_out);
				// the fall at the bed's edge: down to the water below, or to the dry ground below
				float bottom = -1e6f;
				for (int k = 0; k < 4; k++) {
					if (ground[k] < bed) {
						float y = wet[k] ? p_surface[index[k]] : static_cast<float>(ground[k]) - 0.5f;
						bottom = std::max(bottom, y);
					}
				}
				if (bottom > -1e5f && bottom < top - SAME) {
					emit_fall(centre, body, bottom, r_out);
				}
			}
		}
	}
}

} // namespace godot
