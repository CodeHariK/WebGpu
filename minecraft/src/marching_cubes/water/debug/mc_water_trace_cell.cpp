// MCGrid: steps 3 - 4 step by step, for one cell (trace_water_cell; the Debug tab's water steps
// walk, a left click on the map picks the cell; Prev / Next go through it, the text is in the
// Debug tab).
//
// Steps 1 - 2 are run the way _rebuild_water runs them, then the builder's own functions make
// steps 3 - 4 for the whole map into scratch arrays: nothing the game shows is changed. Each trace
// step keeps what it draws (MCTraceStep::draw):
//
//   cell        the 4 corner columns: ground, water level, wet or dry
//   3 classes   per body: what each corner is to it (bank orange, wet blue, lower purple, dry yellow)
//   3 shape     per body: the three lines that bound it (bank edge orange, bed edge purple, dry
//               edge yellow) and the water's edge where they meet (blue)
//   3 banks     per body with a bank: where its bank's edge comes from - the waterline tile cut by
//               a plane at the water's height (red), the edge it gives (orange)
//   4 samples   per body: the sample grid, wet blue / dry grey
//   4 edge      per body: marching squares' edge pieces (shore green, fall orange)
//   4 tile      the tile's triangles, and how many bodies on the map share it
//   4 placed    the tiles round the cell, welded, with the fall strips (the last step)
// A yellow post stands on the cell in every step, so it is easy to find.
#include "marching_cubes/grid/mc_debug_mesh.h"
#include "marching_cubes/grid/mc_grid.h"
#include "marching_cubes/water/4_tiles/mc_water_tile.h"
#include <godot_cpp/classes/time.hpp>
#include <algorithm>
#include <cmath>
#include <memory>

namespace godot {

using namespace mc_water;

namespace {
const float LIFT = 0.08f; // m above the water
const char *CLASS_NAME[4] = { "bank", "wet", "lower", "dry" };
const Color CLASS_COLOR[4] = {
	Color(1.0f, 0.5f, 0.15f), // bank
	Color(0.2f, 0.55f, 1.0f), // wet
	Color(0.7f, 0.3f, 1.0f), // lower
	Color(1.0f, 0.95f, 0.25f), // dry
};
const Color CELL(1.0f, 1.0f, 1.0f, 1.0f);
const Color POST(1.0f, 0.85f, 0.1f, 1.0f);
const Color WET_COLUMN(0.25f, 0.55f, 1.0f, 0.9f);
const Color DRY_COLUMN(0.55f, 0.4f, 0.25f, 0.9f);
const Color BANK_LINE(1.0f, 0.5f, 0.15f);
const Color BED_LINE(0.7f, 0.3f, 1.0f);
const Color DRY_LINE(1.0f, 0.95f, 0.25f);
const Color WATER_LINE(0.3f, 0.8f, 1.0f);
const Color SAMPLE_WET(0.25f, 0.6f, 1.0f, 1.0f);
const Color SAMPLE_DRY(0.5f, 0.5f, 0.5f, 0.8f);
const Color SHORE(0.2f, 1.0f, 0.3f);
const Color FALL(1.0f, 0.55f, 0.1f);
const Color TILE_FILL(0.3f, 0.7f, 1.0f, 0.3f);
const Color TILE_WIRE(1.0f, 1.0f, 1.0f, 0.9f);
const Color TILE_WIRE_DIM(1.0f, 1.0f, 1.0f, 0.35f);
const Color GREY(0.55f, 0.55f, 0.55f, 0.8f);
const Color STRIP(1.0f, 0.55f, 0.1f, 0.8f);
const Color PER_MESH_LINE(1.0f, 0.2f, 0.15f); // banks: per mesh (the slice)
const Color MESH_WIRE(0.75f, 0.6f, 0.45f, 0.45f); // the tile mesh the slice cuts
const int AROUND = 1; // cells round the traced one in the map-wide steps

const float RIBBON = 0.016f; // m: how wide the step's own lines are drawn (flat ribbons; GPU lines are 1 px)

// A line drawn as a flat ribbon RIBBON wide (horizontal; a vertical segment falls back to a 1 px line).
void ribbon(
		MCDebugMesh &r_debug,
		const Vector3 &p_a,
		const Vector3 &p_b,
		const Color &p_color,
		float p_width = RIBBON
) {
	Vector3 along(p_b.x - p_a.x, 0, p_b.z - p_a.z);
	if (along.length_squared() < 1e-10f) {
		r_debug.line(p_a, p_b, p_color);
		return;
	}
	Vector3 side = Vector3(-along.z, 0, along.x).normalized() * (p_width * 0.5f);
	r_debug.triangle(p_a - side, p_b - side, p_b + side, p_color);
	r_debug.triangle(p_a - side, p_b + side, p_a + side, p_color);
}

// A body-local point (shape coordinates, -0.5 .. 0.5; Godot z = -y) of cell (cx, cz) in grid space.
Vector3 at(
		const Vector2i &p_cell,
		const Vector2 &p,
		float p_y
) {
	return Vector3(static_cast<float>(p_cell.x) + 0.5f + p.x, p_y, static_cast<float>(p_cell.y) + 0.5f - p.y);
}

// The line where f = 0 over the cell (marching squares on an n x n grid, crossings interpolated).
template <typename F>
void contour(
		MCDebugMesh &r_debug,
		const Vector2i &p_cell,
		float p_y,
		F p_f,
		const Color &p_color,
		int p_n = 32
) {
	float step = 1.0f / static_cast<float>(p_n);
	auto point = [&](int i, int j) { return Vector2(-0.5f + static_cast<float>(i) * step, -0.5f + static_cast<float>(j) * step); };
	std::vector<float> v(static_cast<size_t>((p_n + 1) * (p_n + 1)));
	for (int j = 0; j <= p_n; j++) {
		for (int i = 0; i <= p_n; i++) {
			v[static_cast<size_t>(j * (p_n + 1) + i)] = p_f(point(i, j));
		}
	}
	for (int j = 0; j < p_n; j++) {
		for (int i = 0; i < p_n; i++) {
			const int ci[4] = { i, i + 1, i + 1, i };
			const int cj[4] = { j, j, j + 1, j + 1 };
			std::vector<Vector2> cuts;
			for (int m = 0; m < 4; m++) {
				int k = (m + 1) % 4;
				float a = v[static_cast<size_t>(cj[m] * (p_n + 1) + ci[m])];
				float b = v[static_cast<size_t>(cj[k] * (p_n + 1) + ci[k])];
				if ((a >= 0.0f) != (b >= 0.0f)) {
					cuts.push_back(point(ci[m], cj[m]).lerp(point(ci[k], cj[k]), a / (a - b)));
				}
			}
			for (size_t m = 0; m + 1 < cuts.size(); m += 2) {
				ribbon(r_debug, at(p_cell, cuts[m], p_y), at(p_cell, cuts[m + 1], p_y), p_color);
			}
		}
	}
}

void square_outline(
		MCDebugMesh &r_debug,
		const Vector2i &p_cell,
		float p_y,
		const Color &p_color
) {
	Vector3 c00(static_cast<float>(p_cell.x), p_y, static_cast<float>(p_cell.y));
	const Vector3 c[4] = { c00, c00 + Vector3(1, 0, 0), c00 + Vector3(1, 0, 1), c00 + Vector3(0, 0, 1) };
	for (int k = 0; k < 4; k++) {
		r_debug.line(c[k], c[(k + 1) % 4], p_color);
	}
}

void wire(
		MCDebugMesh &r_debug,
		const Vector3 &p_a,
		const Vector3 &p_b,
		const Vector3 &p_c,
		const Color &p_color
) {
	r_debug.line(p_a, p_b, p_color);
	r_debug.line(p_b, p_c, p_color);
	r_debug.line(p_c, p_a, p_color);
}

// Where triangle a b c crosses the plane y = p_y: a segment (false when it doesn't).
bool slice_triangle(
		const Vector3 &p_a,
		const Vector3 &p_b,
		const Vector3 &p_c,
		float p_y,
		Vector3 &r_from,
		Vector3 &r_to
) {
	const Vector3 v[3] = { p_a, p_b, p_c };
	Vector3 hits[3];
	int count = 0;
	for (int e = 0; e < 3 && count < 2; e++) {
		const Vector3 &a = v[e];
		const Vector3 &b = v[(e + 1) % 3];
		if ((a.y >= p_y) != (b.y >= p_y)) {
			hits[count++] = a.lerp(b, (p_y - a.y) / (b.y - a.y));
		}
	}
	if (count < 2) {
		return false;
	}
	r_from = hits[0];
	r_to = hits[1];
	return true;
}

bool in_rect(
		const Vector3 &p,
		const Rect2 &p_rect
) {
	return p.x >= p_rect.position.x && p.z >= p_rect.position.y && p.x <= p_rect.position.x + p_rect.size.x &&
			p.z <= p_rect.position.y + p_rect.size.y;
}

// The triangles of p_indices (into p_mesh's vertices) whose middle is in p_rect, drawn as wire
// (and filled when p_fill's alpha is above 0); returns how many have their middle in p_cell.
int add_triangles(
		MCDebugMesh &r_debug,
		const MCWaterArrays &p_mesh,
		const PackedInt32Array &p_indices,
		const Rect2 &p_rect,
		const Rect2 &p_cell,
		float p_lift,
		const Color &p_fill,
		const Color &p_wire,
		int *r_in_rect = nullptr
) {
	int in_cell = 0;
	int in_window = 0;
	Vector3 lift(0, p_lift, 0);
	for (int64_t t = 0; t + 2 < p_indices.size(); t += 3) {
		Vector3 a = p_mesh.vertices[p_indices[t]] + lift;
		Vector3 b = p_mesh.vertices[p_indices[t + 1]] + lift;
		Vector3 c = p_mesh.vertices[p_indices[t + 2]] + lift;
		Vector3 middle = (a + b + c) / 3.0f;
		if (!in_rect(middle, p_rect)) {
			continue;
		}
		in_window++;
		bool own = in_rect(middle, p_cell);
		in_cell += own ? 1 : 0;
		if (p_fill.a > 0.0f && own) {
			r_debug.triangle(a, b, c, p_fill);
		}
		wire(r_debug, a, b, c, p_wire);
	}
	if (r_in_rect) {
		*r_in_rect = in_window;
	}
	return in_cell;
}

String why(
		int p_class,
		int p_ground,
		float p_top
) {
	switch (p_class) {
		case BANK:
			return vformat("ground %d reaches above the surface (%.2f): rock, the water ends under it", p_ground, p_top);
		case WET:
			return vformat("ground %d, one of this body's own wet columns", p_ground);
		case LOWER:
			return vformat("ground %d is below this body's bed: the water falls toward it", p_ground);
		default:
			return vformat("ground %d is the bed's tier but dry there: the water stops short of it", p_ground);
	}
}
} // namespace

int MCGrid::trace_water_cell(const Vector2i &p_cell) {
	water_trace.clear();
	water_trace_kind = 5;
	pour_step = -1;
	water_trace_cell = p_cell;
	Vector2i columns = _column_counts();
	if (!heightmap_mode || water_levels.size() != column_heights.size() || p_cell.x < 0 || p_cell.y < 0 ||
			p_cell.x >= columns.x - 1 || p_cell.y >= columns.y - 1) {
		return 0;
	}
	// steps 1 - 2 as _rebuild_water runs them (the spills swapped in until the end)
	std::vector<uint8_t> stored = _spilled_levels();
	water_levels.swap(stored);
	std::vector<float> surface;
	std::vector<uint8_t> held;
	_water_surfaces(surface, held);
	std::vector<Color> flow = _water_flow(surface);
	// step 3 for this cell, and for every cell (which bodies share a tile)
	std::vector<Body> bodies;
	std::vector<float> bottoms;
	_water_bodies(p_cell.x, p_cell.y, surface, held, flow, bodies, bottoms);
	std::vector<uint64_t> keys(bodies.size());
	std::vector<int> sharing(bodies.size(), 0);
	for (size_t k = 0; k < bodies.size(); k++) {
		keys[k] = tile_key(bodies[k], water_sample_rate);
	}
	{
		std::vector<Body> other;
		std::vector<float> other_bottoms;
		for (int cz = 0; cz + 1 < columns.y; cz++) {
			for (int cx = 0; cx + 1 < columns.x; cx++) {
				_water_bodies(cx, cz, surface, held, flow, other, other_bottoms);
				for (const Body &body : other) {
					uint64_t key = tile_key(body, water_sample_rate);
					for (size_t k = 0; k < keys.size(); k++) {
						sharing[k] += key == keys[k] ? 1 : 0;
					}
				}
			}
		}
	}
	// step 4 for the whole map, into scratch arrays (timed)
	Time *time = Time::get_singleton();
	MCWaterArrays tiles_mesh;
	uint64_t t0 = time->get_ticks_usec();
	_emit_water_tiles(surface, held, flow, tiles_mesh);
	uint64_t t1 = time->get_ticks_usec();

	// where to draw
	float y = -1e6f;
	for (const Body &body : bodies) {
		y = std::max(y, body.top);
	}
	if (bodies.empty()) {
		for (int k = 0; k < 4; k++) {
			Vector2i c = p_cell + Vector2i(k & 1, k >> 1);
			y = std::max(y, static_cast<float>(column_heights[static_cast<size_t>(_column_index(c.x, c.y))]) - 0.5f);
		}
	}
	y += LIFT;
	Rect2 cell_rect(Vector2(p_cell), Vector2(1, 1));
	Rect2 window(Vector2(p_cell - Vector2i(AROUND, AROUND)), Vector2(1 + 2 * AROUND, 1 + 2 * AROUND));
	String where = vformat("cell (%d, %d)", p_cell.x, p_cell.y);
	// a new step: the post and the cell's square, then what the step adds
	auto add = [&](const String &p_text, float p_y = 1e9f) {
		float square_y = p_y < 1e8f ? p_y : y; // a body's steps: at that body's surface
		MCTraceStep &step = water_trace.emplace_back();
		step.text = p_text;
		step.draw = std::make_shared<MCDebugMesh>();
		Vector3 centre(static_cast<float>(p_cell.x) + 0.5f, y, static_cast<float>(p_cell.y) + 0.5f);
		step.draw->line(centre, centre + Vector3(0, 1.5f, 0), POST);
		step.draw->square(centre + Vector3(0, 1.5f, 0), 0.06f, POST);
		square_outline(*step.draw, p_cell, square_y, CELL);
		return step.draw;
	};

	// the cell: its 4 corner columns
	{
		String text = vformat("%s: its 4 corner columns (each a ground height and a water level, step 1).", where);
		text += vformat("\nStep 3 gives the cell one body (a flat sheet of water) per water level among them: %d here.", static_cast<int>(bodies.size()));
		std::shared_ptr<MCDebugMesh> draw = add(text);
		String lines;
		for (int k = 0; k < 4; k++) {
			Vector2i c = p_cell + Vector2i(k & 1, k >> 1);
			size_t i = static_cast<size_t>(_column_index(c.x, c.y));
			bool wet = _is_wet(static_cast<int>(i));
			float cy = wet ? surface[i] : static_cast<float>(column_heights[i]) - 0.5f;
			draw->square(Vector3(static_cast<float>(c.x), cy + LIFT, static_cast<float>(c.y)), 0.08f, wet ? WET_COLUMN : DRY_COLUMN);
			lines += vformat("\n(%d, %d): ground %d, %s", c.x, c.y, column_heights[i],
					wet ? vformat("water level %d (%s, surface %.2f)", water_levels[i], held[i] ? "held" : "stream", surface[i]) : String("dry"));
		}
		water_trace.back().text += lines;
	}
	for (size_t k = 0; k < bodies.size(); k++) {
		const Body &body = bodies[k];
		String name = vformat("%s, body %d / %d", where, static_cast<int>(k) + 1, static_cast<int>(bodies.size()));
		float by = body.top + LIFT;
		// 3: the corner classes
		{
			String text = vformat("Step 3 (bodies), %s: surface at %.2f, %s.\nWhat each corner is to this body:", name, body.top,
					body.stream ? "a stream (a thin sheet that keeps running)" : "held (banks keep it in, one layer deep)");
			std::shared_ptr<MCDebugMesh> draw = add(text, by);
			for (int m = 0; m < 4; m++) {
				int cls = corner_class(body, m);
				Vector2 corner(m == 0 || m == 3 ? -0.5f : 0.5f, m < 2 ? -0.5f : 0.5f);
				draw->square(at(p_cell, corner * 0.8f, by), 0.07f, CLASS_COLOR[cls]);
				Vector2i c = p_cell + Vector2i(corner.x > 0 ? 1 : 0, corner.y > 0 ? 0 : 1);
				water_trace.back().text += vformat("\n(%d, %d) %s: %s", c.x, c.y, CLASS_NAME[cls], why(cls, body.ground[m], body.top));
			}
			if (bottoms[k] > -1e5f) {
				water_trace.back().text += vformat("\nIts falls land at %.2f.", bottoms[k]);
			}
		}
		// 3: the shape
		{
			String text = vformat("Step 3 (bodies), %s: its shape. Three lines bound the water, all from the 4 corner classes:"
								  "\norange: the bank's edge (rock above the water)"
								  "\npurple: the bed's edge (where the ground drops to lower corners)"
								  "\nyellow: the edge toward dry corners"
								  "\nblue: the water's edge, where all three agree (the water is inside all of them)",
					name);
			std::shared_ptr<MCDebugMesh> draw = add(text, by);
			contour(*draw, p_cell, by, [&](const Vector2 &p) { return body.above_at(p); }, BANK_LINE);
			contour(*draw, p_cell, by, [&](const Vector2 &p) { return body.bed_at(p); }, BED_LINE);
			if (!body.wet.all()) {
				contour(*draw, p_cell, by, [&](const Vector2 &p) { return blend(body.wet, p.x, p.y); }, DRY_LINE);
			}
			contour(*draw, p_cell, by + 0.01f, [&](const Vector2 &p) { return body.water_field(p); }, WATER_LINE);
		}
		// 3: where the bank's edge comes from: the waterline tile, cut by a plane
		if (!body.above.none()) {
			std::shared_ptr<MCDebugMesh> draw = add(String(), by);
			int layer = static_cast<int>(std::floor(body.top));
			float local = body.top - (static_cast<float>(layer) + 0.5f);
			const MCBakedTile *tile = layer >= 0 ? _baked_tile(Vector3i(p_cell.x, layer, p_cell.y)) : nullptr;
			int cuts = 0;
			if (tile) {
				Vector3 origin(static_cast<float>(p_cell.x) + 0.5f, static_cast<float>(layer) + 0.5f, static_cast<float>(p_cell.y) + 0.5f);
				for (size_t t = 0; t + 2 < tile->triangles.size(); t += 3) {
					const Vector3 &a = tile->triangles[t];
					const Vector3 &b = tile->triangles[t + 1];
					const Vector3 &c = tile->triangles[t + 2];
					wire(*draw, origin + a, origin + b, origin + c, MESH_WIRE);
					Vector3 from;
					Vector3 to;
					if (slice_triangle(a, b, c, local, from, to)) {
						ribbon(*draw, origin + from + Vector3(0, LIFT, 0), origin + to + Vector3(0, LIFT, 0), PER_MESH_LINE);
						cuts++;
					}
				}
			}
			contour(*draw, p_cell, by + 0.01f, [&](const Vector2 &p) { return body.above_at(p); }, BANK_LINE);
			water_trace.back().text = vformat(
					"Step 3 (bodies), %s: where the bank's edge comes from. Take the one tile the water's surface passes"
					" through (layer %d, its triangles in brown) and cut it with a flat plane at the water's height (%.2f m into"
					" the tile): %d triangles cross it (red). Which side is rock comes from the nearest triangle's facing; the"
					" bank's edge (orange) is where that cut is. It depends only on the mesh, its turn and the height, so it is"
					" baked once and shared by every cell that uses that mesh. The fall line is cut the same way, from the tile"
					" under the bed, just below its grass.",
					name, layer, local, cuts);
			if (!tile) {
				water_trace.back().text += "\n(No tile in that layer here: below the grid's first layer, or an empty cell.)";
			}
		}
		// 4: the samples
		int rate = water_sample_rate;
		float step = 1.0f / static_cast<float>(rate);
		{
			std::shared_ptr<MCDebugMesh> draw = add(String(), by);
			int wet = 0;
			for (int j = 0; j <= rate; j++) {
				for (int i = 0; i <= rate; i++) {
					Vector2 p(-0.5f + static_cast<float>(i) * step, -0.5f + static_cast<float>(j) * step);
					bool in = body.water_field(p) > TILE_INSIDE;
					wet += in ? 1 : 0;
					draw->square(at(p_cell, p, by), 0.018f, in ? SAMPLE_WET : SAMPLE_DRY);
				}
			}
			contour(*draw, p_cell, by, [&](const Vector2 &p) { return body.water_field(p); }, WATER_LINE);
			water_trace.back().text = vformat("Step 4 (tiles), %s: sample the shape on a %d x %d grid (sample rate %d):"
											  " %d of %d points are water (blue), the rest dry (grey).",
					name, rate + 1, rate + 1, rate, wet, (rate + 1) * (rate + 1));
		}
		// 4: the tile (the builder's own, from the cache)
		const WaterTile *cached = cached_tile(keys[k]);
		WaterTile baked;
		if (!cached) {
			baked = bake_tile(body, rate);
		}
		const WaterTile &tile = cached ? *cached : baked;
		{
			String text = vformat("Step 4 (tiles), %s: marching squares. Each grid square with water at some corners and not"
								  " others gets an edge piece (crossings found on the exact shape). Each piece looks just outside"
								  " itself: rock -> a shore (green, %d), anything lower -> a fall (orange, %d).",
					name, static_cast<int>(tile.shores.size()), static_cast<int>(tile.falls.size()));
			if (tile.whole) {
				text = vformat("Step 4 (tiles), %s: every sample is water, so the tile is one whole quad (no edge pieces).", name);
			}
			std::shared_ptr<MCDebugMesh> draw = add(text, by);
			for (const Vector2 &p : tile.wet_samples) {
				draw->square(at(p_cell, p, by), 0.01f, GREY);
			}
			for (const WaterTile::Shore &shore : tile.shores) {
				ribbon(*draw, at(p_cell, shore.a, by), at(p_cell, shore.b, by), SHORE);
			}
			for (const WaterTile::Fall &fall : tile.falls) {
				Vector3 a = at(p_cell, fall.a, by);
				Vector3 b = at(p_cell, fall.b, by);
				ribbon(*draw, a, b, FALL);
				Vector3 out(fall.out.x, 0, -fall.out.y);
				ribbon(*draw, (a + b) * 0.5f, (a + b) * 0.5f + out * 0.08f, FALL, RIBBON * 0.6f); // which way it faces
			}
		}
		{
			int triangles = tile.whole ? 2 : static_cast<int>(tile.surface.size() / 3);
			String text = vformat("Step 4 (tiles), %s: the tile, %d triangles. It is baked once and cached by its key"
								  " (corner classes, style, rate, lip, spill, banks): %d bod%s on this map use%s the very same tile.",
					name, triangles, sharing[k], sharing[k] == 1 ? "y" : "ies", sharing[k] == 1 ? "s" : "");
			std::shared_ptr<MCDebugMesh> draw = add(text, by);
			if (tile.whole) {
				const Vector2 q[4] = { Vector2(-0.5f, -0.5f), Vector2(0.5f, -0.5f), Vector2(0.5f, 0.5f), Vector2(-0.5f, 0.5f) };
				draw->triangle(at(p_cell, q[0], by), at(p_cell, q[1], by), at(p_cell, q[2], by), TILE_FILL);
				draw->triangle(at(p_cell, q[0], by), at(p_cell, q[2], by), at(p_cell, q[3], by), TILE_FILL);
				wire(*draw, at(p_cell, q[0], by), at(p_cell, q[1], by), at(p_cell, q[2], by), TILE_WIRE);
				wire(*draw, at(p_cell, q[0], by), at(p_cell, q[2], by), at(p_cell, q[3], by), TILE_WIRE);
			}
			for (size_t t = 0; t + 2 < tile.surface.size(); t += 3) {
				Vector3 a = at(p_cell, tile.surface[t], by);
				Vector3 b = at(p_cell, tile.surface[t + 1], by);
				Vector3 c = at(p_cell, tile.surface[t + 2], by);
				draw->triangle(a, b, c, TILE_FILL);
				wire(*draw, a, b, c, TILE_WIRE);
			}
		}
	}
	if (bodies.empty()) {
		water_trace.back().text += "\nNo water in this cell: nothing for steps 3 - 4.";
		water_levels.swap(stored);
		return static_cast<int>(water_trace.size());
	}
	// 4: placed (the last step)
	{
		int tiles_cell = 0;
		int tiles_window = 0;
		std::shared_ptr<MCDebugMesh> draw = add(String());
		tiles_cell = add_triangles(*draw, tiles_mesh, tiles_mesh.indices, window, cell_rect, LIFT, TILE_FILL, TILE_WIRE, &tiles_window);
		int strips = 0;
		add_triangles(*draw, tiles_mesh, tiles_mesh.first_indices, window, cell_rect, LIFT, Color(0, 0, 0, 0), STRIP, &strips);
		water_trace.back().text = vformat("Step 4 (tiles), placed - the last step. Every body of every cell stamps its"
										  " tile at its surface height; neighbours put the same points on their shared side, so the"
										  " vertices are welded into one mesh. Round %s: %d triangles (%d in the cell), and %d fall-strip"
										  " triangles down the cliffs (orange). Whole map: %.2f ms.",
				where, tiles_window, tiles_cell, strips, static_cast<double>(t1 - t0) / 1000.0);
	}
	water_levels.swap(stored);
	return static_cast<int>(water_trace.size());
}

} // namespace godot
