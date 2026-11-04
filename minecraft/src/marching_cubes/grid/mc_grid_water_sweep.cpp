// MCGrid: the waterfalls swept along the outlines (step 5 of the outline-first water mesh; on with
// water_outline_fill). Each run of FALL edges of an inset outline (mc_grid_water_fill.cpp) gets one
// strip, seen side-on:
//
//   surface ──────╮        bevel  from the inset point (the flat water's own edge) a quarter ellipse
//                 │               out to the fall line (the point before the inset) and down
//                 │        drop   straight down to where the water lands
//                 ╰──      foot   a short flare out into the water below
//
// The strip's top row lies exactly on the flat water's edge, so the two meet with no gap; one strip
// per run, so its UV is continuous along the whole run:
//   UV.x  metres along the run (along the fall line), UV.y  metres along the profile
//   UV2   x how far over the bevel (0 flat .. 1 straight down), y metres above the landing
// Every point's own drop comes from the nearest placed fall (MCWaterArrays::placed_falls) of the
// same surface; all points of a run have the same number of rows (from the run's highest drop).
#include "marching_cubes/grid/mc_grid.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace godot {

namespace {
const int BEVEL_ROWS = 6;
const int FOOT_ROWS = 2;
const float FOOT = 0.12f; // m
const float DROP_ROW = 0.25f; // m: drop rows at most this tall
const float HALF_PI = 1.5707963f;
const float SAME_TOP = 1e-3f;
const float NEAR = 0.6f; // m: a placed fall this close gives a point its drop
const float BEVEL_CLEAR = 0.02f; // m the bevel's lowest point keeps above the ground under it
const float INTO_BANK = 0.06f; // m: a run's ends reach on this far into the bank beside them
                               // (square tiles stop the water just short of a wall: no grass sliver)

float segment_distance_squared(
		const Vector3 &p,
		const Vector3 &a,
		const Vector3 &b
) {
	Vector2 ab(b.x - a.x, b.z - a.z);
	Vector2 ap(p.x - a.x, p.z - a.z);
	float l = ab.length_squared();
	float t = l > 1e-12f ? std::clamp(ap.dot(ab) / l, 0.0f, 1.0f) : 0.0f;
	return (ap - ab * t).length_squared();
}
} // namespace

void MCGrid::_sweep_falls(
		const std::vector<Color> &p_flow,
		MCWaterArrays &r_out
) const {
	Vector2i columns = _column_counts();
	auto column_at = [&](const Vector3 &p) {
		int x = std::clamp(static_cast<int>(std::lround(p.x)), 0, columns.x - 1);
		int z = std::clamp(static_cast<int>(std::lround(p.z)), 0, columns.y - 1);
		return static_cast<size_t>(_column_index(x, z));
	};
	// a point's drop: from the nearest placed fall of the same surface (0: none near)
	auto drop_at = [&](const Vector3 &p_origin) {
		float best = NEAR * NEAR;
		float drop = 0.0f;
		for (const PlacedFall &fall : r_out.placed_falls) {
			if (std::abs(fall.ta.y - p_origin.y) > SAME_TOP) {
				continue;
			}
			float d = segment_distance_squared(p_origin, fall.ta, fall.tb);
			if (d < best) {
				best = d;
				drop = fall.drop;
			}
		}
		return drop;
	};
	for (const WaterOutline &outline : water_outlines) {
		if (outline.points.size() < 3) {
			continue;
		}
		WaterOutline inset = _inset_outline(outline, water_fall_lip);
		int count = static_cast<int>(inset.points.size());
		int edges = static_cast<int>(inset.kinds.size());
		if (edges == 0 || inset.origins.size() != inset.points.size()) {
			continue;
		}
		// the runs of FALL edges: [first point, last point] (a closed all-FALL loop: one run round it)
		std::vector<std::pair<int, int>> runs;
		bool all_fall = std::all_of(inset.kinds.begin(), inset.kinds.end(), [](uint8_t k) { return k == WaterEdge::FALL; });
		if (all_fall && inset.closed) {
			runs.push_back({ 0, count }); // count: back to point 0
		} else {
			int start = 0;
			if (inset.closed) { // start the scan at a BANK edge, so no run is split at index 0
				while (start < edges && inset.kinds[static_cast<size_t>(start)] == WaterEdge::FALL) {
					start++;
				}
			}
			int i = 0;
			while (i < edges) {
				int e = (start + i) % edges;
				if (inset.kinds[static_cast<size_t>(e)] != WaterEdge::FALL) {
					i++;
					continue;
				}
				int first = start + i;
				while (i < edges && inset.kinds[static_cast<size_t>((start + i) % edges)] == WaterEdge::FALL) {
					i++;
				}
				runs.push_back({ first, start + i }); // points first .. start + i (mod count)
			}
		}
		for (const std::pair<int, int> &run : runs) {
			std::vector<int> points;
			for (int k = run.first; k <= run.second; k++) {
				points.push_back(((k % count) + count) % count);
			}
			if (points.size() < 2) {
				continue;
			}
			// per point: inset, origin, drop; the run's row count from its highest drop
			std::vector<float> drops;
			float most = 0.0f;
			for (int k : points) {
				float d = drop_at(inset.origins[static_cast<size_t>(k)]);
				drops.push_back(d);
				most = std::max(most, d);
			}
			if (most < 0.05f) {
				continue;
			}
			// the run's points (inset and origin), its two ends pushed on into the banks beside them
			std::vector<Vector3> inner;
			std::vector<Vector3> fall_line;
			for (int k : points) {
				inner.push_back(inset.points[static_cast<size_t>(k)]);
				fall_line.push_back(inset.origins[static_cast<size_t>(k)]);
			}
			bool wraps = all_fall && inset.closed;
			if (!wraps) {
				auto push_end = [&](size_t p_end, size_t p_neighbour) {
					Vector3 t = fall_line[p_end] - fall_line[p_neighbour];
					t.y = 0.0f;
					if (t.length_squared() > 1e-10f) {
						t = t.normalized() * INTO_BANK;
						fall_line[p_end] += t;
						inner[p_end] += t;
					}
				};
				push_end(0, 1);
				push_end(points.size() - 1, points.size() - 2);
			}
			int drop_rows = std::max(1, static_cast<int>(std::ceil(most / DROP_ROW)));
			float u = 0.0f;
			std::vector<int> previous;
			for (size_t n = 0; n < points.size(); n++) {
				const Vector3 &p = inner[n];
				const Vector3 &o = fall_line[n];
				if (n > 0) {
					u += o.distance_to(fall_line[n - 1]);
				}
				float drop = std::max(drops[n], 0.05f);
				Vector3 out(o.x - p.x, 0, o.z - p.z);
				float across = out.length();
				out = across > 1e-5f ? out / across : Vector3();
				if (across <= 1e-5f) { // no inset here: face away from the run's neighbours' water side
					Vector3 a = fall_line[n > 0 ? n - 1 : n];
					Vector3 b = fall_line[n + 1 < points.size() ? n + 1 : n];
					Vector3 t = b - a;
					out = Vector3(t.z, 0, -t.x).normalized(); // right of the run (the water is on the left)
				}
				float down = std::min(across, drop * 0.5f);
				float foot = std::min(FOOT, std::max(drop - down, 0.0f) * 0.5f);
				float straight = std::max(drop - down - foot, 0.0f);
				size_t column = column_at(p);
				Color c = column < p_flow.size() ? p_flow[column] : Color(0, 0, 0, 0);
				Color surface_flow(c.r * 0.5f + 0.5f, c.g * 0.5f + 0.5f, c.b, c.a);
				Color falling(out.x * 0.5f + 0.5f, out.z * 0.5f + 0.5f, 1.0f, 0.0f);
				float ground = column < column_heights.size() ? static_cast<float>(column_heights[column]) - 0.5f : p.y;
				float depth = std::max(p.y - ground, 0.0f);
				// the bevel never goes below the ground under it: a stream is only a thin sheet over the
				// grass, and a deeper bevel would dip under it (the grass showing through as a line)
				down = std::min(down, std::max(depth - BEVEL_CLEAR, 0.0f));
				foot = std::min(FOOT, std::max(drop - down, 0.0f) * 0.5f);
				straight = std::max(drop - down - foot, 0.0f);
				std::vector<int> current;
				float along = 0.0f;
				Vector3 last;
				auto row = [&](const Vector3 &p_position, const Vector3 &p_normal, float p_over, const Color &p_color) {
					if (!current.empty()) {
						along += p_position.distance_to(last);
					}
					last = p_position;
					current.push_back(static_cast<int>(r_out.vertices.size()));
					r_out.vertices.push_back(p_position);
					r_out.normals.push_back(p_normal.normalized());
					r_out.uvs.push_back(Vector2(u, along));
					r_out.uv2s.push_back(Vector2(p_over, p.y - p_position.y > drop ? 0.0f : drop - (p.y - p_position.y)));
					r_out.colors.push_back(p_color);
					r_out.depths.push_back(depth);
				};
				// the bevel: x = across sin(a), y = down (1 - cos(a)) from the inset point
				for (int i = 0; i <= BEVEL_ROWS; i++) {
					float a = static_cast<float>(i) / static_cast<float>(BEVEL_ROWS) * HALF_PI;
					Vector3 position = p + out * (across * std::sin(a)) - Vector3(0, down * (1.0f - std::cos(a)), 0);
					Vector3 normal = out * (down * std::sin(a)) + Vector3(0, across * std::cos(a), 0);
					if (across <= 1e-5f) {
						normal = i == 0 ? Vector3(0, 1, 0) : out;
					}
					row(position, normal, a / HALF_PI, i < BEVEL_ROWS ? surface_flow : falling);
				}
				Vector3 edge = p + out * across - Vector3(0, down, 0);
				for (int i = 1; i <= drop_rows; i++) {
					float t = static_cast<float>(i) / static_cast<float>(drop_rows);
					row(edge - Vector3(0, straight * t, 0), out, 1.0f, falling);
				}
				for (int i = 1; i <= FOOT_ROWS; i++) {
					float a = static_cast<float>(i) / static_cast<float>(FOOT_ROWS) * HALF_PI;
					Vector3 position = edge - Vector3(0, straight, 0) + out * (foot * (1.0f - std::cos(a))) -
							Vector3(0, foot * std::sin(a), 0);
					row(position, out * std::cos(a) + Vector3(0, std::sin(a), 0), 1.0f, falling);
				}
				if (!previous.empty()) {
					for (size_t r = 0; r + 1 < current.size(); r++) {
						int a0 = previous[r], a1 = previous[r + 1], b0 = current[r], b1 = current[r + 1];
						for (int index : { a0, b0, b1, a0, b1, a1 }) { // winding fixed in _water_mesh
							r_out.indices.push_back(index);
						}
					}
				}
				previous = current;
			}
		}
	}
}

} // namespace godot
