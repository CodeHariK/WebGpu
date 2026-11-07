// MCGrid: step 2's debug traces, stepped through with the Debug tab's Back / Next buttons.
//
// trace_water_step(kind) runs step 2 the way _rebuild_water does (the column spill's levels swapped
// in, then held / stream, then the flow) and asks one of its functions to record every decision it
// makes, with the reason, into water_trace. The functions are the builder's own, so the trace is
// exactly what the mesh is built from. Nothing is changed.
//   2  the spills: each landing past a low corner and each spread step (column spill)
//   3  held / stream: each wet column's four sides (holds or lets it run out), then the streams
//      spreading through same-level water
//   4  the flow: each wet column checked for an outlet, then the walk from the outlets
// The sea is left out of the traces (it is always held and never an outlet).
//
// The overlay at step i: every column decided so far (steps 0 .. i) as a square in its result's
// colour (held blue, stream green, spilled orange, flows cyan with an arrow,
// does not flow dark grey), the column step i decides outlined in white, a line to each neighbour it looked at
// (green: lets it / holds it in, red: stops it / lets it run out, grey: no effect, cyan: where it
// comes from). The explanation is in the Debug tab.
#include "marching_cubes/grid/mc_debug_mesh.h"
#include "marching_cubes/grid/mc_grid.h"
#include <algorithm>

namespace godot {

namespace {
const float LIFT = 0.12f; // m above the water
const Color RESULT_COLOR[6] = {
	Color(0.6f, 0.6f, 0.6f, 0.0f), // NONE
	Color(0.25f, 0.55f, 1.0f, 0.85f), // HELD
	Color(0.3f, 1.0f, 0.45f, 0.85f), // STREAM
	Color(1.0f, 0.55f, 0.1f, 0.85f), // SPILLED
	Color(0.35f, 0.9f, 1.0f, 0.85f), // FLOWS
	Color(0.3f, 0.3f, 0.32f, 0.85f), // STILL
};
const Color MARK_COLOR[4] = {
	Color(0.3f, 1.0f, 0.4f), // YES
	Color(1.0f, 0.3f, 0.25f), // NO
	Color(0.6f, 0.6f, 0.6f), // SAME
	Color(0.3f, 0.9f, 1.0f), // LOOK
};
const Color CURRENT(1.0f, 1.0f, 1.0f, 1.0f);
} // namespace

String MCGrid::_column_name(int p_index) const {
	int width = _column_counts().x;
	return vformat("(%d, %d)", p_index % width, p_index / width);
}

int MCGrid::trace_water_step(int p_kind) {
	water_trace.clear();
	water_trace_kind = std::clamp(p_kind, 2, 4);
	pour_step = -1;
	if (!heightmap_mode || water_levels.size() != column_heights.size()) {
		return 0;
	}
	std::vector<MCTraceStep> *spills = water_trace_kind == 2 ? &water_trace : nullptr;
	std::vector<uint8_t> stored = _spilled_levels(nullptr, spills);
	water_levels.swap(stored); // the column spill in, as _rebuild_water does
	std::vector<float> surface;
	std::vector<uint8_t> held;
	_water_surfaces(surface, held, water_trace_kind == 3 ? &water_trace : nullptr);
	if (water_trace_kind == 4) {
		_water_flow(surface, &water_trace);
	}
	water_levels.swap(stored); // the stored levels back
	return static_cast<int>(water_trace.size());
}

void MCGrid::_add_water_trace_debug(
		MCDebugMesh &r_debug,
		const std::vector<float> &p_surface
) {
	if (water_trace_kind == 0 || pour_step < 0 || pour_step >= static_cast<int>(water_trace.size())) {
		return;
	}
	if (water_trace[static_cast<size_t>(pour_step)].draw) { // steps 3 - 4 draw their own (mc_water_trace_cell.cpp)
		r_debug.append(*water_trace[static_cast<size_t>(pour_step)].draw);
		return;
	}
	Vector2i columns = _column_counts();
	auto top_of = [&](int p_index) {
		size_t i = static_cast<size_t>(p_index);
		float y = _is_wet(p_index) ? p_surface[i] : static_cast<float>(column_heights[i]) - 0.5f;
		return Vector3(static_cast<float>(p_index % columns.x), y + LIFT, static_cast<float>(p_index / columns.x));
	};
	// every column decided so far: its latest result
	std::vector<int> latest(column_heights.size(), -1);
	for (int k = 0; k <= pour_step; k++) {
		latest[static_cast<size_t>(water_trace[static_cast<size_t>(k)].column)] = k;
	}
	for (size_t i = 0; i < latest.size(); i++) {
		if (latest[i] < 0) {
			continue;
		}
		const MCTraceStep &step = water_trace[static_cast<size_t>(latest[i])];
		Vector3 at = top_of(static_cast<int>(i));
		r_debug.square(at, 0.3f, RESULT_COLOR[step.result]);
		if (step.result == MCTraceStep::FLOWS && step.dir.length_squared() > 0.0f) {
			Vector3 dir(step.dir.x, 0, step.dir.y);
			r_debug.arrow(at + Vector3(0, LIFT * 0.5f, 0), dir.normalized(), 0.55f, Color(0.05f, 0.12f, 0.35f));
		}
	}
	// the column this step decides, and what it looked at
	const MCTraceStep &step = water_trace[static_cast<size_t>(pour_step)];
	Vector3 centre = top_of(step.column) + Vector3(0, LIFT * 0.5f, 0);
	const Vector3 ring[4] = { Vector3(-0.45f, 0, -0.45f), Vector3(0.45f, 0, -0.45f), Vector3(0.45f, 0, 0.45f),
		Vector3(-0.45f, 0, 0.45f) };
	for (int k = 0; k < 4; k++) {
		r_debug.line(centre + ring[k], centre + ring[(k + 1) % 4], CURRENT);
	}
	for (const std::pair<int, uint8_t> &mark : step.marks) {
		Vector3 to = top_of(mark.first) + Vector3(0, LIFT * 0.5f, 0);
		const Color &color = MARK_COLOR[mark.second];
		r_debug.line(centre, to, color);
		if (mark.second == MCTraceStep::NO) { // a cross
			r_debug.line(to + Vector3(-0.2f, 0, -0.2f), to + Vector3(0.2f, 0, 0.2f), color);
			r_debug.line(to + Vector3(-0.2f, 0, 0.2f), to + Vector3(0.2f, 0, -0.2f), color);
		} else {
			r_debug.square(to, 0.12f, color);
		}
	}
}

void MCGrid::_reset_pour_trace() {
	water_trace_kind = 0; // the step buttons go through the pour (step 1)
	last_pour.clear();
	last_pour_trace.clear();
	pour_step = -1;
}

// Step 1 for the whole map as the pour trace, without changing any water (see mc_grid.h).
int MCGrid::trace_settle() {
	_reset_pour_trace();
	if (!heightmap_mode || water_levels.size() != column_heights.size()) {
		return 0;
	}
	Vector2i columns = _column_counts();
	std::vector<uint8_t> seen(column_heights.size(), 0);
	int pour = 0;
	for (int source : _settle_sources()) { // settle_water's order
		std::vector<MCPourStep> trace;
		std::vector<int> region = _flow_region(Vector2i(source % columns.x, source / columns.x), seen, &trace);
		if (region.empty()) {
			continue; // an earlier pour reached it already
		}
		for (MCPourStep &step : trace) {
			step.pour = pour;
		}
		last_pour.insert(last_pour.end(), region.begin(), region.end());
		last_pour_trace.insert(last_pour_trace.end(), trace.begin(), trace.end());
		pour++;
	}
	return static_cast<int>(last_pour_trace.size());
}

} // namespace godot
