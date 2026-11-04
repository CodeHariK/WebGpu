// MCGrid: the water's outlines (steps 1-2 of the outline-first water mesh).
//
// Every body of water is a flat polygon at its surface height; its edge is a closed loop (or an
// open run where it meets the grid's border) of segments, each tagged by what is beyond it:
//   BANK   higher ground: the water ends under the rock
//   FALL   lower water or lower / dry ground: the water falls down there
// The water tiles builder (mc_grid_water_tiles.cpp) hands over every edge segment it finds
// (MCWaterArrays::edges); this file chains them into loops by their shared end points (welded,
// at the same height) and relaxes the points a little along the loop, so the sample grid's
// point-to-point wobble is smoothed out (each point moves toward the middle of its neighbours; the
// ends of a run and the points where BANK turns to FALL stay put, so runs keep their ends).
// The loops are kept in water_outlines and drawn by the "outline" water view (mc_grid_water_view.cpp):
// BANK edges green, FALL edges orange, loop points white.
//
// Later steps build on the loops: inset the FALL runs by the bevel radius, fill the inset loop as
// the flat water, sweep the bevel / drop profile along the FALL runs.
#include "marching_cubes/grid/mc_grid.h"
#include <godot_cpp/templates/hash_map.hpp>
#include <algorithm>
#include <cmath>
#include <vector>

namespace godot {

namespace {
const float WELD_SCALE = 4096.0f;
const int RELAX_STEPS = 3; // relaxation passes
const float RELAX = 0.5f; // how far a point moves toward its neighbours' middle per pass
// m: a run's end this close to another run's start (same height) is joined to it. Where a BANK
// edge in one cell meets a FALL edge in the next, the two ends sit up to 2 x mc_water::EDGE apart
// (tucked under the bank on one side, reaching to the fall line on the other).
const float BRIDGE = 0.15f;

Vector3i weld(const Vector3 &p_v) {
	return Vector3i(static_cast<int>(std::lround(p_v.x * WELD_SCALE)), static_cast<int>(std::lround(p_v.y * WELD_SCALE)),
			static_cast<int>(std::lround(p_v.z * WELD_SCALE)));
}
} // namespace

// Chain p_edges into loops / runs (see the top of this file).
std::vector<WaterOutline> MCGrid::_water_outlines(const std::vector<WaterEdge> &p_edges) const {
	// every edge by its start point; edges are oriented with the water on the same side
	HashMap<Vector3i, std::vector<int>> starts;
	HashMap<Vector3i, int> ends; // how many edges end at a point (a run starts where none does)
	for (int i = 0; i < static_cast<int>(p_edges.size()); i++) {
		Vector3i a = weld(p_edges[i].a);
		if (!starts.has(a)) {
			starts.insert(a, std::vector<int>());
		}
		starts[a].push_back(i);
		Vector3i b = weld(p_edges[i].b);
		int *n = ends.getptr(b);
		if (n) {
			(*n)++;
		} else {
			ends.insert(b, 1);
		}
	}
	std::vector<uint8_t> used(p_edges.size(), 0);
	std::vector<WaterOutline> outlines;
	// one chain from edge p_first, following each edge's end to an unused edge starting there
	auto chain = [&](int p_first) {
		WaterOutline outline;
		outline.top = p_edges[p_first].a.y;
		outline.points.push_back(p_edges[p_first].a);
		int edge = p_first;
		while (edge >= 0) {
			used[edge] = 1;
			outline.points.push_back(p_edges[edge].b);
			outline.kinds.push_back(p_edges[edge].kind);
			int next = -1;
			if (const std::vector<int> *list = starts.getptr(weld(p_edges[edge].b))) {
				for (int candidate : *list) {
					if (!used[candidate]) {
						next = candidate;
						break;
					}
				}
			}
			edge = next;
		}
		// back where it started: a closed loop (the last point is the first again)
		if (outline.points.size() > 2 && weld(outline.points.back()) == weld(outline.points.front())) {
			outline.points.pop_back();
			outline.closed = true;
		}
		return outline;
	};
	// open runs first (they start where no edge ends), then the closed loops
	for (int i = 0; i < static_cast<int>(p_edges.size()); i++) {
		if (!used[i] && !ends.has(weld(p_edges[i].a))) {
			outlines.push_back(chain(i));
		}
	}
	for (int i = 0; i < static_cast<int>(p_edges.size()); i++) {
		if (!used[i]) {
			outlines.push_back(chain(i));
		}
	}
	_bridge_outlines(outlines);
	for (WaterOutline &outline : outlines) {
		_relax_outline(outline);
	}
	return outlines;
}

// Join open runs whose ends nearly meet (see BRIDGE): a run's end to the closest open run's start
// at the same height, with one bridging edge of the next run's kind; a run whose end comes back to
// its own start closes. Repeated until nothing joins.
void MCGrid::_bridge_outlines(std::vector<WaterOutline> &r_outlines) {
	bool joined = true;
	while (joined) {
		joined = false;
		for (size_t o = 0; o < r_outlines.size() && !joined; o++) {
			WaterOutline &run = r_outlines[o];
			if (run.closed || run.points.size() < 2) {
				continue;
			}
			const Vector3 &end = run.points.back();
			size_t best = r_outlines.size();
			float best_distance = BRIDGE;
			for (size_t q = 0; q < r_outlines.size(); q++) {
				const WaterOutline &other = r_outlines[q];
				if (other.closed || other.points.empty() || std::abs(other.top - run.top) > 1e-3f) {
					continue;
				}
				const Vector3 &start = other.points.front();
				float d = Vector2(start.x - end.x, start.z - end.z).length();
				if (d < best_distance) {
					best_distance = d;
					best = q;
				}
			}
			if (best == r_outlines.size()) {
				continue;
			}
			if (best == o) { // back to its own start: a loop
				run.kinds.push_back(run.kinds.empty() ? WaterEdge::BANK : run.kinds.front());
				run.closed = true;
				joined = true;
				continue;
			}
			WaterOutline other = r_outlines[best];
			run.kinds.push_back(other.kinds.empty() ? WaterEdge::BANK : other.kinds.front()); // the bridge
			run.points.insert(run.points.end(), other.points.begin(), other.points.end());
			run.kinds.insert(run.kinds.end(), other.kinds.begin(), other.kinds.end());
			r_outlines.erase(r_outlines.begin() + static_cast<long>(best));
			joined = true;
		}
	}
}

// One line per outline: closed / open, how many points, BANK / FALL edges and runs, its height.
String MCGrid::get_water_outline_summary() const {
	String text;
	for (const WaterOutline &outline : water_outlines) {
		int bank = 0;
		int fall = 0;
		int runs = 0;
		int edges = static_cast<int>(outline.kinds.size());
		for (int i = 0; i < edges; i++) {
			(outline.kinds[static_cast<size_t>(i)] == WaterEdge::FALL ? fall : bank)++;
			bool first = i == 0;
			uint8_t before = outline.kinds[static_cast<size_t>((i - 1 + edges) % edges)];
			if ((first && !outline.closed) || before != outline.kinds[static_cast<size_t>(i)]) {
				runs++;
			}
		}
		text += vformat("%s points %d  bank %d  fall %d  runs %d  top %.2f", outline.closed ? "closed" : "open  ",
				static_cast<int>(outline.points.size()), bank, fall, std::max(runs, 1), outline.top);
		if (!outline.closed && !outline.points.empty()) {
			const Vector3 &a = outline.points.front();
			const Vector3 &b = outline.points.back();
			text += vformat("  from (%.3f, %.3f) to (%.3f, %.3f)", a.x, a.z, b.x, b.z);
		}
		text += "\n";
	}
	return text;
}

// Smooth an outline's points (see the top of this file). Heights don't change.
void MCGrid::_relax_outline(WaterOutline &r_outline) {
	int count = static_cast<int>(r_outline.points.size());
	int edges = static_cast<int>(r_outline.kinds.size());
	if (count < 3) {
		return;
	}
	// a point stays put at a run's ends and where the edge kind changes (BANK <-> FALL)
	std::vector<uint8_t> pinned(static_cast<size_t>(count), 0);
	for (int i = 0; i < count; i++) {
		bool first = i == 0;
		bool last = i == count - 1;
		if (!r_outline.closed && (first || last)) {
			pinned[static_cast<size_t>(i)] = 1;
			continue;
		}
		int before = (i - 1 + edges) % edges; // the edge into point i
		int after = i % edges; // the edge out of it
		if (r_outline.kinds[static_cast<size_t>(before)] != r_outline.kinds[static_cast<size_t>(after)]) {
			pinned[static_cast<size_t>(i)] = 1;
		}
	}
	for (int step = 0; step < RELAX_STEPS; step++) {
		std::vector<Vector3> moved = r_outline.points;
		for (int i = 0; i < count; i++) {
			if (pinned[static_cast<size_t>(i)]) {
				continue;
			}
			const Vector3 &prev = r_outline.points[static_cast<size_t>((i - 1 + count) % count)];
			const Vector3 &next = r_outline.points[static_cast<size_t>((i + 1) % count)];
			Vector3 middle = (prev + next) * 0.5f;
			Vector3 p = r_outline.points[static_cast<size_t>(i)];
			moved[static_cast<size_t>(i)] = Vector3(Math::lerp(p.x, middle.x, RELAX), p.y, Math::lerp(p.z, middle.z, RELAX));
		}
		r_outline.points = moved;
	}
}

} // namespace godot
