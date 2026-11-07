// Water step 4 (tiles): cut the flat water away from under the fall strips' bevels.
//
// A tile's flat water is trimmed back from the fall line by about the lip radius when the tile is
// baked (Body::fall_lip), but that trim is measured on the cell's own shape. After placing, the fall
// lines are joined and relaxed across cells (_join_fall_ends, _relax_fall_chains) and each strip
// leaves along a direction smoothed over its neighbours (_fall_ends). Where the line bends
// hard (a U or a V between falls) or a fall ends at a bank, the flat water can then reach out past
// where the strip's bevel starts, and sticks out through the bevel (or covers it).
//
// Seen side-on, at one strip:
//
//   flat water ════════╗                 the flat water must end left of B
//              strip  ═══════╗           strip's flat part: from lip + FLAT back, LIFT above
//                        B    ╲          B: where the bevel starts, r back from the fall line
//                              │         (r = the end's bevel radius, FallEnd::radius)
//
// Each strip's footprint, from r + TUCK back from the fall line (under the strip's flat part, which
// hides the cut) out to BEYOND past the fall line (flat water there hangs over the drop), between
// its two end columns (the same columns the strip uses, so neighbouring footprints meet), is cut
// out of the flat triangles at the strip's height: a triangle the footprint touches is replaced by
// the convex pieces left outside it. Nothing outside a footprint changes, so no hole opens; the cut
// points are welded so the pieces share them.
//
// Cheap where nothing is cut: a flat triangle lies in one cell, so only the footprints over that
// cell are tested, first by box, then by a separating edge; few triangles get to the cutting.
#include "marching_cubes/terrain/mc_grid.h"
#include <godot_cpp/templates/hash_map.hpp>
#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <vector>

namespace godot {

namespace {
const float TUCK = 0.01f; // m behind the bevel's start the flat water ends
const float BEYOND = 0.15f; // m past the fall line the footprint reaches: flat water there hangs over the drop
const float SAME_HEIGHT = 1e-3f; // m: flat water at a strip's top height
const float MIN_AREA = 1e-8f; // m^2 (twice): smaller pieces are dropped
const float CUT_WELD = 1e4f; // cut points this close (1 / CUT_WELD m) are one vertex
const int MAX_CORNERS = 16; // a piece's corners (a triangle cut by a few footprints has fewer)

// A corner of a flat piece: where (x, z), its flow colour and depth, its vertex (-1: a new cut point).
struct FlatCorner {
	Vector2 p;
	Color color;
	float depth;
	int index;
};

// A convex flat piece, counter-clockwise in x, z (fixed size: no allocation per piece).
struct Piece {
	FlatCorner c[MAX_CORNERS];
	int n = 0;
	void add(const FlatCorner &p_corner) {
		if (n < MAX_CORNERS) {
			c[n++] = p_corner;
		}
	}
	float area() const { // twice the signed area
		float sum = 0.0f;
		for (int i = 0; i < n; i++) {
			sum += c[i].p.cross(c[(i + 1) % n].p);
		}
		return sum;
	}
	Rect2 bounds() const {
		Rect2 box(c[0].p, Vector2());
		for (int i = 1; i < n; i++) {
			box.expand_to(c[i].p);
		}
		return box;
	}
};

// A strip's footprint (or one half of it): a convex polygon, counter-clockwise in x, z, at the
// strip's top height.
struct Cut {
	Vector2 points[4];
	int n = 0;
	float y = 0.0f;
	Rect2 bounds;
};

// Is the polygon convex and counter-clockwise (every corner turns left)?
bool convex(
		const Vector2 *p_points,
		int p_count
) {
	for (int i = 0; i < p_count; i++) {
		const Vector2 &a = p_points[i];
		const Vector2 &b = p_points[(i + 1) % p_count];
		const Vector2 &c = p_points[(i + 2) % p_count];
		if ((b - a).cross(c - b) <= MIN_AREA) {
			return false;
		}
	}
	return true;
}

// Do triangle t (any winding) and a footprint not overlap? True when an edge of either has the
// other wholly on its outer side (a quick test before cutting; touching counts as apart).
bool apart(
		const Vector2 t[3],
		const Cut &p_cut
) {
	for (int e = 0; e < p_cut.n; e++) { // the footprint's edges (counter-clockwise: inside on the left)
		const Vector2 &u = p_cut.points[e];
		Vector2 along = p_cut.points[(e + 1) % p_cut.n] - u;
		if (along.cross(t[0] - u) <= 0.0f && along.cross(t[1] - u) <= 0.0f && along.cross(t[2] - u) <= 0.0f) {
			return true;
		}
	}
	float turn = (t[1] - t[0]).cross(t[2] - t[0]) >= 0.0f ? 1.0f : -1.0f;
	for (int e = 0; e < 3; e++) { // the triangle's edges
		const Vector2 &u = t[e];
		Vector2 along = (t[(e + 1) % 3] - u) * turn;
		bool all_out = true;
		for (int k = 0; k < p_cut.n; k++) {
			all_out = all_out && along.cross(p_cut.points[k] - u) <= 0.0f;
		}
		if (all_out) {
			return true;
		}
	}
	return false;
}

// The part of a convex piece on one side of the line u -> v: left (p_sign 1) or right (-1).
Piece clip(
		const Piece &p_piece,
		const Vector2 &u,
		const Vector2 &v,
		float p_sign
) {
	Piece out;
	for (int i = 0; i < p_piece.n; i++) {
		const FlatCorner &a = p_piece.c[i];
		const FlatCorner &b = p_piece.c[(i + 1) % p_piece.n];
		float sa = p_sign * (v - u).cross(a.p - u);
		float sb = p_sign * (v - u).cross(b.p - u);
		if (sa >= 0.0f) {
			out.add(a);
		}
		if ((sa >= 0.0f) != (sb >= 0.0f)) {
			float t = sa / (sa - sb);
			out.add({ a.p.lerp(b.p, t), a.color.lerp(b.color, t), a.depth + (b.depth - a.depth) * t, -1 });
		}
	}
	return out;
}

// A convex piece minus a footprint: the convex pieces outside it (one per footprint edge it
// crosses), added to r_pieces. False (nothing added) when the footprint doesn't overlap it.
bool subtract(
		const Piece &p_piece,
		const Cut &p_cut,
		std::vector<Piece> &r_pieces
) {
	size_t first = r_pieces.size();
	Piece rest = p_piece;
	for (int e = 0; e < p_cut.n && rest.n >= 3; e++) {
		const Vector2 &u = p_cut.points[e];
		const Vector2 &v = p_cut.points[(e + 1) % p_cut.n];
		Piece out = clip(rest, u, v, -1.0f);
		if (out.n >= 3 && out.area() > MIN_AREA) {
			r_pieces.push_back(out);
		}
		rest = clip(rest, u, v, 1.0f);
	}
	if (rest.n < 3 || rest.area() <= MIN_AREA) {
		r_pieces.resize(first); // no overlap
		return false;
	}
	return true;
}
} // namespace

void MCGrid::_cut_surface_under_falls(
		const std::vector<PlacedFall> &p_falls,
		const std::vector<FallEnds> &p_ends,
		MCWaterArrays &r_out
) const {
	if (p_falls.empty()) {
		return;
	}
	// every strip's footprint (one quad, or two triangles where the quad isn't convex), by cell
	std::vector<Cut> cuts;
	Vector2i columns = _column_counts();
	Vector2i cells(columns.x - 1, columns.y - 1);
	std::vector<std::vector<int>> by_cell(static_cast<size_t>(std::max(cells.x * cells.y, 0)));
	auto cell_list = [&](int x, int z) -> std::vector<int> * {
		if (x < 0 || z < 0 || x >= cells.x || z >= cells.y) {
			return nullptr;
		}
		return &by_cell[static_cast<size_t>(z * cells.x + x)];
	};
	auto add_cut = [&](std::initializer_list<Vector2> p_points, float y) {
		Cut cut;
		for (const Vector2 &p : p_points) {
			cut.points[cut.n++] = p;
		}
		float turn = 0.0f; // twice the area
		for (int i = 0; i < cut.n; i++) {
			turn += cut.points[i].cross(cut.points[(i + 1) % cut.n]);
		}
		if (std::abs(turn) <= MIN_AREA) {
			return;
		}
		if (turn < 0.0f) {
			std::reverse(cut.points, cut.points + cut.n); // counter-clockwise: inside is left of every edge
		}
		cut.y = y;
		cut.bounds = Rect2(cut.points[0], Vector2());
		for (int i = 1; i < cut.n; i++) {
			cut.bounds.expand_to(cut.points[i]);
		}
		int id = static_cast<int>(cuts.size());
		cuts.push_back(cut);
		Vector2 end = cut.bounds.get_end();
		for (int z = static_cast<int>(std::floor(cut.bounds.position.y)); z <= static_cast<int>(std::floor(end.y)); z++) {
			for (int x = static_cast<int>(std::floor(cut.bounds.position.x)); x <= static_cast<int>(std::floor(end.x)); x++) {
				if (std::vector<int> *list = cell_list(x, z)) {
					list->push_back(id);
				}
			}
		}
	};
	for (size_t f = 0; f < p_falls.size(); f++) {
		const PlacedFall &fall = p_falls[f];
		const FallEnds &ends = p_ends[f];
		Vector2 a(fall.ta.x, fall.ta.z);
		Vector2 b(fall.tb.x, fall.tb.z);
		Vector2 da(ends.end[0].direction.x, ends.end[0].direction.z);
		Vector2 db(ends.end[1].direction.x, ends.end[1].direction.z);
		Vector2 a_back = a - da * (ends.end[0].radius + TUCK); // where each end's bevel starts
		Vector2 b_back = b - db * (ends.end[1].radius + TUCK);
		Vector2 a_out = a + da * BEYOND;
		Vector2 b_out = b + db * BEYOND;
		const Vector2 quad[4] = { a_back, b_back, b_out, a_out };
		const Vector2 reversed[4] = { a_out, b_out, b_back, a_back };
		if (convex(quad, 4) || convex(reversed, 4)) {
			add_cut({ a_back, b_back, b_out, a_out }, fall.ta.y);
		} else { // (where the end columns converge, at the tip of a tight U)
			add_cut({ a_back, b_back, b_out }, fall.ta.y);
			add_cut({ a_back, b_out, a_out }, fall.ta.y);
		}
	}

	// every flat triangle a footprint touches: replaced by what is left outside the footprints
	std::vector<int32_t> kept;
	kept.reserve(static_cast<size_t>(r_out.indices.size()) + 256);
	std::vector<int64_t> starts; // the pieces' starts ("pieces" view), moved with their triangles
	size_t next_piece = 0;
	HashMap<Vector3i, int> cut_points;
	auto cut_vertex = [&](const FlatCorner &c, float y, const Vector2 &uv2) {
		if (c.index >= 0) {
			return c.index;
		}
		Vector3 v(c.p.x, y, c.p.y);
		Vector3i key(static_cast<int>(std::lround(v.x * CUT_WELD)), static_cast<int>(std::lround(v.y * CUT_WELD)),
				static_cast<int>(std::lround(v.z * CUT_WELD)));
		if (const int *hit = cut_points.getptr(key)) {
			return *hit;
		}
		int index = static_cast<int>(r_out.vertices.size());
		r_out.vertices.push_back(v);
		r_out.normals.push_back(Vector3(0, 1, 0));
		r_out.uvs.push_back(c.p); // the surface's UV is world x, z
		r_out.uv2s.push_back(uv2);
		r_out.colors.push_back(c.color);
		r_out.depths.push_back(c.depth);
		cut_points.insert(key, index);
		return index;
	};
	int64_t count = r_out.indices.size();
	const int32_t *indices = r_out.indices.ptr();
	std::vector<int> near;
	std::vector<Piece> pieces;
	std::vector<Piece> next;
	for (int64_t t = 0; t + 2 < count; t += 3) {
		while (next_piece < r_out.pieces.size() && r_out.pieces[next_piece] <= t) {
			starts.push_back(static_cast<int64_t>(kept.size()));
			next_piece++;
		}
		int32_t ix[3] = { indices[t], indices[t + 1], indices[t + 2] };
		const Vector3 *vertices = r_out.vertices.ptr(); // (again each time: cut_vertex adds vertices)
		float y = vertices[ix[0]].y;
		// the footprints over its cell (it lies in the one its middle is in) at its height
		Vector3 middle = (vertices[ix[0]] + vertices[ix[1]] + vertices[ix[2]]) / 3.0f;
		const std::vector<int> *list = cell_list(static_cast<int>(std::floor(middle.x)), static_cast<int>(std::floor(middle.z)));
		near.clear();
		if (list && !list->empty()) {
			const Vector2 corners[3] = { Vector2(vertices[ix[0]].x, vertices[ix[0]].z),
				Vector2(vertices[ix[1]].x, vertices[ix[1]].z), Vector2(vertices[ix[2]].x, vertices[ix[2]].z) };
			Rect2 bounds(corners[0], Vector2());
			bounds.expand_to(corners[1]);
			bounds.expand_to(corners[2]);
			for (int id : *list) {
				const Cut &cut = cuts[static_cast<size_t>(id)];
				if (std::abs(cut.y - y) <= SAME_HEIGHT && cut.bounds.intersects(bounds) && !apart(corners, cut)) {
					near.push_back(id);
				}
			}
		}
		if (near.empty()) { // (most triangles)
			kept.insert(kept.end(), ix, ix + 3);
			continue;
		}
		Piece triangle;
		for (int32_t i : ix) {
			triangle.add({ Vector2(vertices[i].x, vertices[i].z), r_out.colors[i], r_out.depths[i], i });
		}
		if (triangle.area() < 0.0f) {
			std::swap(triangle.c[1], triangle.c[2]); // counter-clockwise (the winding is fixed in _water_mesh)
		}
		pieces.assign(1, triangle);
		bool changed = false;
		for (int id : near) {
			next.clear();
			for (const Piece &piece : pieces) {
				if (!cuts[static_cast<size_t>(id)].bounds.intersects(piece.bounds()) ||
						!subtract(piece, cuts[static_cast<size_t>(id)], next)) {
					next.push_back(piece);
				} else {
					changed = true;
				}
			}
			pieces.swap(next);
		}
		if (!changed) {
			kept.insert(kept.end(), ix, ix + 3);
			continue;
		}
		Vector2 uv2 = r_out.uv2s[ix[0]]; // one value over a body's surface
		for (const Piece &piece : pieces) {
			int first = cut_vertex(piece.c[0], y, uv2);
			for (int k = 1; k + 1 < piece.n; k++) { // a fan: the pieces are convex
				kept.push_back(first);
				kept.push_back(cut_vertex(piece.c[k], y, uv2));
				kept.push_back(cut_vertex(piece.c[k + 1], y, uv2));
			}
		}
	}
	while (next_piece < r_out.pieces.size()) {
		starts.push_back(static_cast<int64_t>(kept.size()));
		next_piece++;
	}
	r_out.indices.resize(static_cast<int64_t>(kept.size()));
	std::copy(kept.begin(), kept.end(), r_out.indices.ptrw());
	r_out.pieces = starts;
}

} // namespace godot
