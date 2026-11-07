// Water step 2b, held or stream (_water_surfaces): the surface height of every wet column.
//
// Water stands one layer deep (surface water_drop below the next tier) only where banks hold it in:
// every side a bank, the same held water, or a waterfall cut into the land (_fall_framed) onto
// lower water; the sea is held. Elsewhere (it ran out of a channel onto open ground, or a tier's
// open edge is beside it) it is a stream: a thin sheet STREAM above the ground that keeps running
// and pours over the edge. Same-level water joined to a stream drains through it, so it is a stream
// too. Derived when the mesh is built; only the levels are stored.
#include "marching_cubes/grid/mc_grid.h"
#include "marching_cubes/water/mc_water_steps.h"
#include <vector>

namespace godot {

using mc_water::SIDE_NAMES;
using mc_water::STEPS;

namespace {
const float STREAM = 0.1f; // m a stream's surface sits above its ground
} // namespace

// Does water at p_level spill onto dry ground of height p_ground beside it (ground below the surface)?
static bool _spills_onto(
		int p_ground,
		int p_level,
		float p_drop
) {
	return static_cast<float>(p_ground) - 0.5f < static_cast<float>(p_level) - 0.5f - p_drop;
}

// Is the waterfall from column p_from (water at p_level) toward p_step framed? Walk sideways along the
// edge over the columns that fall the same way; at both ends there must be a bank (ground at least
// p_level, or the grid's edge) — a fall cut into the land, like a river's. A fall off the open edge of
// a tier (the run ends in lower ground or more water) is not.
bool MCGrid::_fall_framed(
		const Vector2i &p_from,
		const Vector2i &p_step,
		int p_level
) const {
	Vector2i columns = _column_counts();
	auto inside = [&](const Vector2i &c) { return c.x >= 0 && c.y >= 0 && c.x < columns.x && c.y < columns.y; };
	auto bank = [&](const Vector2i &c) {
		return !inside(c) || column_heights[static_cast<size_t>(_column_index(c.x, c.y))] >= p_level;
	};
	const Vector2i across(p_step.y, p_step.x);
	for (int sign : { 1, -1 }) {
		Vector2i m = p_from;
		for (int guard = 0; guard < columns.x + columns.y; guard++) {
			m += across * sign;
			if (bank(m)) {
				break; // a bank ends the run: framed on this side
			}
			int i = _column_index(m.x, m.y);
			if (!_is_wet(i) || water_levels[static_cast<size_t>(i)] != p_level) {
				return false; // lower ground or other water: open
			}
			Vector2i below = m + p_step;
			if (bank(below)) {
				break; // the same water turns away behind a bank: framed
			}
			int j = _column_index(below.x, below.y);
			if (!_is_wet(j) || water_levels[static_cast<size_t>(j)] >= p_level) {
				return false;
			}
			// m falls the same way: the run goes on
		}
	}
	return true;
}

// Where each column's water surface is (r_surface, world y in grid space; dry columns: -1e6) and
// whether it is held (r_held). Water is held one layer deep (surface_y of its level) where banks hold
// it in: every side is a bank, held water at the same level, or a framed fall onto lower water; the
// sea is always held. Anything else is a stream: a thin sheet STREAM above its ground that runs on
// and pours over edges. Same-level water next to a stream runs out through it, so it is a stream too.
void MCGrid::_water_surfaces(
		std::vector<float> &r_surface,
		std::vector<uint8_t> &r_held,
		std::vector<MCTraceStep> *r_trace
) const {
	size_t count = water_levels.size();
	r_surface.assign(count, -1e6f);
	r_held.assign(count, 0);
	Vector2i columns = _column_counts();
	auto inside = [&](const Vector2i &c) { return c.x >= 0 && c.y >= 0 && c.x < columns.x && c.y < columns.y; };
	auto is_sea = [&](size_t i) { return sea_level > 0 && water_levels[i] == sea_level && column_heights[i] < sea_level; };
	std::vector<Vector2i> open;
	// 1. held where every side holds the water in
	for (int z = 0; z < columns.y; z++) {
		for (int x = 0; x < columns.x; x++) {
			size_t i = static_cast<size_t>(_column_index(x, z));
			if (!_is_wet(static_cast<int>(i))) {
				continue;
			}
			int level = water_levels[i];
			bool held = true;
			MCTraceStep *step = nullptr;
			if (r_trace && !is_sea(i)) { // the sea is always held: not traced
				step = &r_trace->emplace_back();
				step->column = static_cast<int>(i);
				step->text = vformat("Held or stream? %s: ground %d, water level %d. Each side must hold it in:",
						_column_name(static_cast<int>(i)), column_heights[i], level);
			}
			if (!is_sea(i)) {
				for (int k = 0; k < 4; k++) {
					Vector2i n(x + STEPS[k].x, z + STEPS[k].y);
					if (!inside(n)) {
						if (step) {
							step->text += vformat("\n%s: the grid's edge, holds", SIDE_NAMES[k]);
						}
						continue; // the grid's edge holds it
					}
					int other = _column_index(n.x, n.y);
					size_t o = static_cast<size_t>(other);
					bool holds = true;
					String why;
					if (!_is_wet(other)) {
						holds = !_spills_onto(column_heights[o], level, water_drop);
						why = holds ? vformat("dry ground %d, not below the surface: a bank, holds", column_heights[o])
									: vformat("dry ground %d, below the surface: the water runs out there", column_heights[o]);
					} else if (water_levels[o] < level) {
						holds = _fall_framed(Vector2i(x, z), STEPS[k], level);
						why = holds ? String("lower water, the fall is framed by banks on both ends: holds")
									: String("lower water, the fall is open (off a tier's edge): the water runs out there");
					} else {
						why = vformat("water at level %d (same or higher): holds", water_levels[o]);
					}
					held = held && holds;
					if (step) {
						step->text += vformat("\n%s %s: %s", SIDE_NAMES[k], _column_name(other), why);
						step->marks.push_back({ other, holds ? MCTraceStep::YES : MCTraceStep::NO });
					}
				}
			}
			r_held[i] = held ? 1 : 0;
			if (!held) {
				open.push_back(Vector2i(x, z));
			}
			if (step) {
				step->result = held ? MCTraceStep::HELD : MCTraceStep::STREAM;
				step->text += held ? String("\n-> every side holds: HELD, a full layer (surface just below the next tier).")
								   : String("\n-> a side lets it run out: STREAM, a thin sheet above its ground.");
			}
		}
	}
	// 2. same-level water beside a stream runs out through it — diagonal neighbours too: they share a
	// terrain cell, and the cell's water (mc_water_bodies.cpp) has one surface per level, so a held
	// corner diagonal to a stream corner would lift the stream to the held height (floating water)
	const Vector2i AROUND[8] = { Vector2i(1, 0), Vector2i(-1, 0), Vector2i(0, 1), Vector2i(0, -1), Vector2i(1, 1),
		Vector2i(1, -1), Vector2i(-1, 1), Vector2i(-1, -1) };
	for (size_t head = 0; head < open.size(); head++) {
		Vector2i c = open[head];
		int ci = _column_index(c.x, c.y);
		uint8_t level = water_levels[static_cast<size_t>(ci)];
		for (const Vector2i &step_dir : AROUND) {
			Vector2i n = c + step_dir;
			if (!inside(n)) {
				continue;
			}
			size_t i = static_cast<size_t>(_column_index(n.x, n.y));
			if (r_held[i] && !is_sea(i) && _is_wet(static_cast<int>(i)) && water_levels[i] == level) {
				r_held[i] = 0;
				open.push_back(n);
				if (r_trace) {
					MCTraceStep &step = r_trace->emplace_back();
					step.column = static_cast<int>(i);
					step.result = MCTraceStep::STREAM;
					step.marks = { { ci, MCTraceStep::LOOK } };
					step.text = vformat("Streams spread: %s was held, but it is water at the same level %d as the stream %s beside it%s.\n"
										"It drains out through that stream -> STREAM too.",
							_column_name(static_cast<int>(i)), level, _column_name(ci),
							(step_dir.x != 0 && step_dir.y != 0) ? " (diagonally: they share a cell, which has one surface per level)" : "");
				}
			}
		}
	}
	for (size_t i = 0; i < count; i++) {
		if (_is_wet(static_cast<int>(i))) {
			r_surface[i] = r_held[i] ? _surface_y(water_levels[i]) : static_cast<float>(column_heights[i]) - 0.5f + STREAM;
		}
	}
}

} // namespace godot
