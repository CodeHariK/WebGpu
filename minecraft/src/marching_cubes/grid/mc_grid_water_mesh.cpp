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
#include "marching_cubes/grid/mc_water_shape.h"
#include <godot_cpp/templates/hash_map.hpp>
#include <algorithm>
#include <cmath>
#include <vector>

namespace godot {

namespace {
using namespace mc_water;


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
				values.push_back(pass == 0 ? p_body.tuck() - bv : (pass == 1 ? bv + p_body.lip_at(p) : bv));
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
	auto edge_field = [&](const Vector2 &p) { return blend(p_body.bed, p.x, p.y) + p_body.lip(); }; // the surface's edge
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
		MCWaterArrays::FallPiece piece;
		piece.a = pa;
		piece.b = pb;
		piece.top = (top_a + top_b) * 0.5f; // flat water: the same at both ends
		piece.bottom = p_bottom;
		piece.out = out;
		r_out.fall_pieces.push_back(piece);
	});
}
} // namespace

// Join the fall pieces into whole falls: pieces that share an end and have the same top and bottom
// are one fall, walked from one end to the other; each straight run of it becomes a single quad,
// and UV.x is the distance along the whole fall, so the streaks run on unbroken round corners and
// across cells. UV.y: metres down from the top; UV2.x: metres above the landing.
void MCGrid::_merge_falls(MCWaterArrays &r_out) const {
	std::vector<MCWaterArrays::FallPiece> &pieces = r_out.fall_pieces;
	auto key = [](const Vector3 &p_point, float p_top, float p_bottom) {
		return String::num_int64(static_cast<int64_t>(std::lround(p_point.x * 500.0f))) + "," +
				String::num_int64(static_cast<int64_t>(std::lround(p_point.z * 500.0f))) + "," +
				String::num_int64(static_cast<int64_t>(std::lround(p_top * 100.0f))) + "," +
				String::num_int64(static_cast<int64_t>(std::lround(p_bottom * 100.0f)));
	};
	// which pieces touch each end point
	HashMap<String, std::vector<int>> ends;
	for (int i = 0; i < static_cast<int>(pieces.size()); i++) {
		const MCWaterArrays::FallPiece &p = pieces[static_cast<size_t>(i)];
		ends[key(p.a, p.top, p.bottom)].push_back(i);
		ends[key(p.b, p.top, p.bottom)].push_back(i);
	}
	std::vector<uint8_t> used(pieces.size(), 0);
	auto emit_quad = [&](const Vector3 &p_a, const Vector3 &p_b, float p_ua, float p_ub, float p_top, float p_bottom,
			const Vector3 &p_out) {
		float drop = p_top - p_bottom;
		const Vector3 sheet[4] = { Vector3(p_a.x, p_bottom, p_a.z), Vector3(p_b.x, p_bottom, p_b.z),
			Vector3(p_b.x, p_top, p_b.z), Vector3(p_a.x, p_top, p_a.z) };
		const Vector2 uv[4] = { Vector2(p_ua, drop), Vector2(p_ub, drop), Vector2(p_ub, 0), Vector2(p_ua, 0) };
		const Vector2 uv2[4] = { Vector2(0, 0), Vector2(0, 0), Vector2(drop, 0), Vector2(drop, 0) };
		for (int k : { 0, 1, 2, 0, 2, 3 }) {
			r_out.fall_vertices.push_back(sheet[k]);
			r_out.fall_normals.push_back(p_out);
			r_out.fall_uvs.push_back(uv[k]);
			r_out.fall_uv2s.push_back(uv2[k]);
		}
	};
	// the next unused piece touching point p (with the same top / bottom), and its far end
	auto next_from = [&](const Vector3 &p_point, float p_top, float p_bottom, int &r_index, Vector3 &r_far) {
		const std::vector<int> *list = ends.getptr(key(p_point, p_top, p_bottom));
		if (!list) {
			return false;
		}
		for (int j : *list) {
			if (used[static_cast<size_t>(j)]) {
				continue;
			}
			const MCWaterArrays::FallPiece &q = pieces[static_cast<size_t>(j)];
			r_index = j;
			r_far = key(q.a, p_top, p_bottom) == key(p_point, p_top, p_bottom) ? q.b : q.a;
			return true;
		}
		return false;
	};
	for (int start = 0; start < static_cast<int>(pieces.size()); start++) {
		if (used[static_cast<size_t>(start)]) {
			continue;
		}
		const MCWaterArrays::FallPiece first = pieces[static_cast<size_t>(start)];
		used[static_cast<size_t>(start)] = 1;
		// grow the chain both ways from the first piece
		std::vector<Vector3> points = { first.a, first.b };
		std::vector<Vector3> outs = { first.out };
		int index;
		Vector3 far;
		while (next_from(points.back(), first.top, first.bottom, index, far)) {
			used[static_cast<size_t>(index)] = 1;
			points.push_back(far);
			outs.push_back(pieces[static_cast<size_t>(index)].out);
		}
		while (next_from(points.front(), first.top, first.bottom, index, far)) {
			used[static_cast<size_t>(index)] = 1;
			points.insert(points.begin(), far);
			outs.insert(outs.begin(), pieces[static_cast<size_t>(index)].out);
		}
		// one quad per straight run, UV.x the distance along the fall
		float u = 0.0f;
		size_t run = 0;
		for (size_t k = 1; k < points.size(); k++) {
			bool last = k + 1 == points.size();
			bool turns = !last && (points[k] - points[run]).normalized().dot((points[k + 1] - points[k]).normalized()) < 0.9999f;
			if (last || turns) {
				float length = Vector2(points[k].x - points[run].x, points[k].z - points[run].z).length();
				// a few-cm run is where the edge turns into a wall at the fall's end: not water
				// falling, just a thin strip hanging down the rock corner, so it is left out
				if (length >= MIN_FALL_RUN) {
					emit_quad(points[run], points[k], u, u + length, first.top, first.bottom, outs[run]);
				}
				u += length;
				run = k;
			}
		}
	}
	pieces.clear();
}

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

// The water bodies of cell (p_cx, p_cz): one per water level among its 4 corners, with its shape
// layers, surface height and flow; r_bottoms[k]: where body k's fall lands (below its bed's edge:
// the water below, or the dry ground below; -1e6 when nothing falls).
void MCGrid::_water_bodies(
		int p_cx,
		int p_cz,
		const std::vector<float> &p_surface,
		const std::vector<uint8_t> &p_held,
		const std::vector<Color> &p_flow,
		std::vector<mc_water::Body> &r_bodies,
		std::vector<float> &r_bottoms
) const {
	r_bodies.clear();
	r_bottoms.clear();
	int cell_levels = std::max(_dims().max_corner().y, 1);
	// the 4 corner columns, in hash bit order
	const Vector2i at[4] = { Vector2i(p_cx, p_cz + 1), Vector2i(p_cx + 1, p_cz + 1), Vector2i(p_cx + 1, p_cz),
		Vector2i(p_cx, p_cz) };
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
		return;
	}
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
		// the shape style of the bed's cell (its group)
		char group = _group_at(Vector3i(p_cx, std::clamp(bed - 1, 0, cell_levels - 1), p_cz));
		bool blocky = water_blocky_groups.contains(String::chr(static_cast<char32_t>(group)));
		Body body;
		float rounding = water_square ? 0.0f : ROUNDING;
		for (Layer *layer : { &body.above, &body.bed, &body.wet }) {
			layer->blocky = blocky;
			layer->rounding = rounding;
		}
		for (int k = 0; k < 4; k++) {
			body.above.solid[k] = ground[k] >= body_level;
			body.bed.solid[k] = ground[k] >= bed;
			body.wet.solid[k] = body.above.solid[k] || (wet[k] && level[k] == body_level) || ground[k] < bed;
			body.flow[k] = wet[k] && level[k] == body_level ? p_flow[index[k]] : body_flow;
			body.ground[k] = ground[k];
		}
		body.top = top;
		body.square = water_square;
		body.stream = stream;
		// where its fall lands: the water below, or the dry ground below
		float bottom = -1e6f;
		for (int k = 0; k < 4; k++) {
			if (ground[k] < bed) {
				float y = wet[k] ? p_surface[index[k]] : static_cast<float>(ground[k]) - 0.5f;
				bottom = std::max(bottom, y);
			}
		}
		r_bodies.push_back(body);
		r_bottoms.push_back(bottom);
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
	std::vector<Body> bodies;
	std::vector<float> bottoms;
	for (int cz = 0; cz + 1 < columns.y; cz++) {
		for (int cx = 0; cx + 1 < columns.x; cx++) {
			_water_bodies(cx, cz, p_surface, p_held, p_flow, bodies, bottoms);
			Vector2 centre(static_cast<float>(cx) + 0.5f, static_cast<float>(cz) + 0.5f);
			for (size_t k = 0; k < bodies.size(); k++) {
				emit_body(centre, bodies[k], r_out);
				if (bottoms[k] > -1e5f && bottoms[k] < bodies[k].top - SAME) {
					emit_fall(centre, bodies[k], bottoms[k], r_out);
				}
			}
		}
	}
}

} // namespace godot
