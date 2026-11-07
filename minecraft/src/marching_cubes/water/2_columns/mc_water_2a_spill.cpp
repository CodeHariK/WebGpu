// Water step 2a, column spill (_spilled_levels).
//
// MCGrid: diagonal spills, for the water mesh only (water_spill_reach, Water tab slider).
//
// The pour (fill_water) runs left / right / front / back only, so a dry column that touches water
// only diagonally stays dry and shows as a gap at the water's edge. A spill wets it anyway, at mesh
// build time: where a wet column has a dry diagonal neighbour whose ground is not higher, and one
// of the two columns in between is lower than the wet column's ground (a low corner the water runs
// past), the water spills onto that neighbour. From there it spreads on over dry ground that is not
// higher, left / right / front / back, up to water_spill_reach columns in all (1: only the column it
// spills onto; 0: no spills). Each spilled column stands one layer deep (level = ground + 1).
//
// Spills are never stored: _rebuild_water swaps the spilled levels in while it builds the mesh and
// then puts the stored ones back, so pours, settle_water, saves and undo never see them (stored
// spill water would become a source and flood its whole tier on the next settle).
#include "marching_cubes/grid/mc_grid.h"
#include "marching_cubes/water/mc_water_steps.h"
#include <algorithm>
#include <vector>

namespace godot {

using mc_water::SIDE_NAMES;
using mc_water::STEPS;

namespace {
const Vector2i SPILL_DIAGONALS[4] = { Vector2i(1, 1), Vector2i(-1, 1), Vector2i(1, -1), Vector2i(-1, -1) };
} // namespace

void MCGrid::set_water_spill_reach(int p_reach) {
	water_spill_reach = std::clamp(p_reach, 0, 8);
	if (!chunks.empty() && water_levels.size() == column_heights.size()) {
		_queue_water();
	}
}

// The water levels with the spills added (see the top of this file).
std::vector<uint8_t> MCGrid::_spilled_levels(
		std::vector<uint8_t> *r_steps,
		std::vector<MCTraceStep> *r_trace
) const {
	std::vector<uint8_t> levels = water_levels;
	if (r_steps) {
		r_steps->assign(levels.size(), 0);
	}
	if (water_spill_reach <= 0 || levels.size() != column_heights.size()) {
		return levels;
	}
	Vector2i columns = _column_counts();
	auto inside = [&](const Vector2i &c) { return c.x >= 0 && c.y >= 0 && c.x < columns.x && c.y < columns.y; };
	auto ground = [&](const Vector2i &c) { return static_cast<int>(column_heights[static_cast<size_t>(_column_index(c.x, c.y))]); };
	auto wet = [&](const Vector2i &c) {
		size_t i = static_cast<size_t>(_column_index(c.x, c.y));
		return levels[i] > column_heights[i];
	};
	auto name = [&](const Vector2i &c) { return _column_name(_column_index(c.x, c.y)); };
	int top = _max_column_height();
	// the spill landings: dry diagonal neighbours of stored water, past a low corner
	std::vector<std::pair<Vector2i, int>> queue; // column, steps taken (1 = the landing)
	for (int z = 0; z < columns.y; z++) {
		for (int x = 0; x < columns.x; x++) {
			Vector2i c(x, z);
			if (!_is_wet(_column_index(x, z))) {
				continue;
			}
			int g = ground(c);
			for (const Vector2i &d : SPILL_DIAGONALS) {
				Vector2i n = c + d;
				if (!inside(n) || wet(n) || ground(n) > g) {
					continue;
				}
				Vector2i side_x(x + d.x, z);
				Vector2i side_z(x, z + d.y);
				if (ground(side_x) < g || ground(side_z) < g) {
					levels[static_cast<size_t>(_column_index(n.x, n.y))] = static_cast<uint8_t>(std::min(ground(n) + 1, top));
					queue.push_back({ n, 1 });
					if (r_steps) {
						(*r_steps)[static_cast<size_t>(_column_index(n.x, n.y))] = 1;
					}
					if (r_trace) {
						Vector2i low = ground(side_x) < g ? side_x : side_z;
						MCTraceStep &step = r_trace->emplace_back();
						step.column = _column_index(n.x, n.y);
						step.result = MCTraceStep::SPILLED;
						step.marks = { { _column_index(c.x, c.y), MCTraceStep::LOOK }, { _column_index(low.x, low.y), MCTraceStep::YES } };
						step.text = vformat("Landing: %s (ground %d) is dry and diagonal to the water at %s (ground %d).\n"
											"%s between them is lower (ground %d): the water runs past that low corner.\n"
											"-> %s is spilled onto (step 1, level %d).",
								name(n), ground(n), name(c), g, name(low), ground(low), name(n), ground(n) + 1);
					}
				}
			}
		}
	}
	// spread on from the landings, up to water_spill_reach columns in all
	for (size_t head = 0; head < queue.size(); head++) {
		auto [c, steps] = queue[head];
		MCTraceStep *step = nullptr;
		if (r_trace) {
			step = &r_trace->emplace_back();
			step->column = _column_index(c.x, c.y);
			step->result = MCTraceStep::SPILLED;
			step->text = vformat("Spread from %s (spill step %d, ground %d), reach %d:", name(c), steps, ground(c), water_spill_reach);
		}
		if (steps >= water_spill_reach) {
			if (step) {
				step->text += "\nthe reach is used up: the spill stops here.";
			}
			continue;
		}
		for (int k = 0; k < 4; k++) {
			Vector2i n = c + STEPS[k];
			if (!inside(n)) {
				if (step) {
					step->text += vformat("\n%s: off the grid", SIDE_NAMES[k]);
				}
				continue;
			}
			int ni = _column_index(n.x, n.y);
			if (wet(n)) {
				if (step) {
					step->text += vformat("\n%s %s: already wet", SIDE_NAMES[k], name(n));
					step->marks.push_back({ ni, MCTraceStep::SAME });
				}
				continue;
			}
			if (ground(n) > ground(c)) {
				if (step) {
					step->text += vformat("\n%s %s ground %d: higher, the spill can't go there", SIDE_NAMES[k], name(n), ground(n));
					step->marks.push_back({ ni, MCTraceStep::NO });
				}
				continue;
			}
			levels[static_cast<size_t>(ni)] = static_cast<uint8_t>(std::min(ground(n) + 1, top));
			queue.push_back({ n, steps + 1 });
			if (r_steps) {
				(*r_steps)[static_cast<size_t>(ni)] = static_cast<uint8_t>(std::min(steps + 1, 255));
			}
			if (step) {
				step->text += vformat("\n%s %s ground %d: dry, not higher -> spilled (step %d)", SIDE_NAMES[k], name(n), ground(n), steps + 1);
				step->marks.push_back({ ni, MCTraceStep::YES });
			}
		}
	}
	return levels;
}

} // namespace godot
