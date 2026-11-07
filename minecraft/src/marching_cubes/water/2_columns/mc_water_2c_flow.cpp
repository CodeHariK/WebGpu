// Water step 2c, flow (_water_flow): which way every wet column runs.
//
// Water runs toward the nearest fall. A column next to lower water or a spill is an outlet (it
// points at the fall); a breadth-first walk from the outlets through same-level water points every
// other column of the body at the one it was reached from. A body with no outlet (a pond, the sea)
// is still. The column a fall lands in gets foam. The mesh carries this in its vertex COLOR for
// water.gdshader: rg direction as 0..1, b flows, a landing foam.
#include "marching_cubes/grid/mc_grid.h"
#include <cmath>
#include <vector>

namespace godot {

namespace {
const float SAME = 0.02f; // m: surfaces closer than this are the same water
const Vector2i STEPS[4] = { Vector2i(1, 0), Vector2i(-1, 0), Vector2i(0, 1), Vector2i(0, -1) };
} // namespace

// Per column: Color(direction x, direction z, 1 if it flows, 1 if a fall lands in it). Dry or still
// columns are Color(0, 0, 0, 0). p_surface: from _water_surfaces.
std::vector<Color> MCGrid::_water_flow(
		const std::vector<float> &p_surface,
		std::vector<MCTraceStep> *r_trace
) const {
	std::vector<Color> flow(water_levels.size(), Color(0, 0, 0, 0));
	Vector2i columns = _column_counts();
	auto inside = [&](const Vector2i &c) { return c.x >= 0 && c.y >= 0 && c.x < columns.x && c.y < columns.y; };
	auto is_sea = [&](size_t i) { return sea_level > 0 && water_levels[i] == sea_level && column_heights[i] < sea_level; };
	const char *side_names[4] = { "+x", "-x", "+z", "-z" };
	std::vector<Vector2i> queue;
	// 1. outlets: wet columns beside lower water or a spill point at it; the lower water gets foam
	for (int z = 0; z < columns.y; z++) {
		for (int x = 0; x < columns.x; x++) {
			int index = _column_index(x, z);
			if (!_is_wet(index)) {
				continue;
			}
			float surface = p_surface[static_cast<size_t>(index)];
			MCTraceStep *step = nullptr;
			if (r_trace && !is_sea(static_cast<size_t>(index))) { // the sea: not traced
				step = &r_trace->emplace_back();
				step->column = index;
				step->text = vformat("Outlet? %s: surface %.2f. Lower water or dry ground below the surface beside it?",
						_column_name(index), surface);
			}
			Vector2 out;
			for (int k = 0; k < 4; k++) {
				const Vector2i &dir = STEPS[k];
				Vector2i n(x + dir.x, z + dir.y);
				if (!inside(n)) {
					continue;
				}
				size_t other = static_cast<size_t>(_column_index(n.x, n.y));
				bool lower_water = _is_wet(static_cast<int>(other)) && p_surface[other] < surface - SAME;
				bool spill = !_is_wet(static_cast<int>(other)) && static_cast<float>(column_heights[other]) - 0.5f < surface - SAME;
				if (lower_water || spill) {
					out += Vector2(static_cast<float>(dir.x), static_cast<float>(dir.y));
				}
				if (lower_water) {
					flow[other].a = 1.0f;
				}
				if (step) {
					String why = lower_water ? vformat("lower water (surface %.2f): it falls there, an outlet (foam where it lands)", p_surface[other])
							: spill		 ? vformat("dry ground %d below the surface: it spills there, an outlet", column_heights[other])
										 : String("no way out there");
					step->text += vformat("\n%s %s: %s", side_names[k], _column_name(static_cast<int>(other)), why);
					step->marks.push_back({ static_cast<int>(other), (lower_water || spill) ? MCTraceStep::YES : MCTraceStep::SAME });
				}
			}
			if (out.length_squared() > 0.0f) {
				out = out.normalized();
				flow[static_cast<size_t>(index)] = Color(out.x, out.y, 1.0f, flow[static_cast<size_t>(index)].a);
				queue.push_back(Vector2i(x, z));
			}
			if (step) {
				step->result = out.length_squared() > 0.0f ? MCTraceStep::FLOWS : MCTraceStep::STILL;
				step->dir = out;
				step->text += out.length_squared() > 0.0f ? vformat("\n-> an OUTLET: it flows that way (%.2f, %.2f).", out.x, out.y)
														  : String("\n-> not an outlet (it may still be reached from one below).");
			}
		}
	}
	// 2. breadth-first from the outlets through water at the same surface: point where we came from
	for (size_t head = 0; head < queue.size(); head++) {
		Vector2i c = queue[head];
		int ci = _column_index(c.x, c.y);
		float surface = p_surface[static_cast<size_t>(ci)];
		for (const Vector2i &dir : STEPS) {
			Vector2i n = c + dir;
			if (!inside(n)) {
				continue;
			}
			size_t i = static_cast<size_t>(_column_index(n.x, n.y));
			if (_is_wet(static_cast<int>(i)) && std::abs(p_surface[i] - surface) < SAME && flow[i].b == 0.0f) {
				flow[i] = Color(static_cast<float>(-dir.x), static_cast<float>(-dir.y), 1.0f, flow[i].a);
				queue.push_back(n);
				if (r_trace && !is_sea(i)) {
					MCTraceStep &step = r_trace->emplace_back();
					step.column = static_cast<int>(i);
					step.result = MCTraceStep::FLOWS;
					step.dir = Vector2(static_cast<float>(-dir.x), static_cast<float>(-dir.y));
					step.marks = { { ci, MCTraceStep::LOOK } };
					step.text = vformat("Walk from the outlets: %s has the same surface as %s, which already flows.\n"
										"-> it flows toward %s (the way back to the nearest outlet).",
							_column_name(static_cast<int>(i)), _column_name(ci), _column_name(ci));
				}
			}
		}
	}
	return flow;
}

} // namespace godot
