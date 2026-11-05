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
// The circle spill (water_spill_radius > 0) replaces this. Its spill point is the centre of the
// cell where the water runs past the low corner (a wet corner, the dry column diagonally across, a
// lower corner beside them). For the mesh only, that dry column gets the water (_apply_circle_spill:
// the source column's level, surface, held and flow, so nothing else changes), but it is not one of
// the water's own corners: its ground is wet only inside a circle of water_spill_radius (up to 1 m)
// round the spill point (mc_water::Body::spill_circle). Every cell within one cell of the point
// cuts the same circle (_spill_mask), so the cells meet and the outline closes.
//
// Spills are never stored: _rebuild_water swaps the spilled levels in while it builds the mesh and
// then puts the stored ones back, so pours, settle_water, saves and undo never see them (stored
// spill water would become a source and flood its whole tier on the next settle).
#include "marching_cubes/grid/mc_grid.h"
#include <algorithm>
#include <vector>

namespace godot {

namespace {
const Vector2i SPILL_STEPS[4] = { Vector2i(1, 0), Vector2i(-1, 0), Vector2i(0, 1), Vector2i(0, -1) };
const Vector2i SPILL_DIAGONALS[4] = { Vector2i(1, 1), Vector2i(-1, 1), Vector2i(1, -1), Vector2i(-1, -1) };
const Vector2i CELL_CORNERS[4] = { Vector2i(0, 0), Vector2i(1, 0), Vector2i(1, 1), Vector2i(0, 1) }; // around the cell
} // namespace

void MCGrid::set_water_spill_reach(int p_reach) {
	water_spill_reach = std::clamp(p_reach, 0, 8);
	if (!chunks.empty() && water_levels.size() == column_heights.size()) {
		_queue_water();
	}
}

void MCGrid::set_water_spill_radius(float p_radius) {
	water_spill_radius = std::clamp(p_radius, 0.0f, 1.0f); // up to 1: within one cell of its point
	if (!chunks.empty() && water_levels.size() == column_heights.size()) {
		_queue_water();
	}
}

// The water levels with the spills added (see the top of this file).
std::vector<uint8_t> MCGrid::_spilled_levels() const {
	std::vector<uint8_t> levels = water_levels;
	if (water_spill_reach <= 0 || water_spill_radius > 0.0f || levels.size() != column_heights.size()) {
		return levels;
	}
	Vector2i columns = _column_counts();
	auto inside = [&](const Vector2i &c) { return c.x >= 0 && c.y >= 0 && c.x < columns.x && c.y < columns.y; };
	auto ground = [&](const Vector2i &c) { return static_cast<int>(column_heights[static_cast<size_t>(_column_index(c.x, c.y))]); };
	auto wet = [&](const Vector2i &c) {
		size_t i = static_cast<size_t>(_column_index(c.x, c.y));
		return levels[i] > column_heights[i];
	};
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
				if (ground(Vector2i(x + d.x, z)) < g || ground(Vector2i(x, z + d.y)) < g) {
					levels[static_cast<size_t>(_column_index(n.x, n.y))] = static_cast<uint8_t>(std::min(ground(n) + 1, top));
					queue.push_back({ n, 1 });
				}
			}
		}
	}
	// spread on from the landings, up to water_spill_reach columns in all
	for (size_t head = 0; head < queue.size(); head++) {
		auto [c, steps] = queue[head];
		if (steps >= water_spill_reach) {
			continue;
		}
		for (const Vector2i &s : SPILL_STEPS) {
			Vector2i n = c + s;
			if (!inside(n) || wet(n) || ground(n) > ground(c)) {
				continue;
			}
			levels[static_cast<size_t>(_column_index(n.x, n.y))] = static_cast<uint8_t>(std::min(ground(n) + 1, top));
			queue.push_back({ n, steps + 1 });
		}
	}
	return levels;
}

// The circle spill's points: the cells where the water of some level has a corner with a dry column
// diagonally across (not a bank, dry, not below the water's bed: its lowest ground in the cell) and
// a lower column beside them (below the bed). On the stored water.
std::vector<MCGrid::SpillPoint> MCGrid::_spill_points() const {
	std::vector<SpillPoint> points;
	if (water_levels.size() != column_heights.size()) {
		return points;
	}
	Vector2i columns = _column_counts();
	for (int cz = 0; cz + 1 < columns.y; cz++) {
		for (int cx = 0; cx + 1 < columns.x; cx++) {
			int index[4];
			int ground[4];
			for (int k = 0; k < 4; k++) {
				index[k] = _column_index(cx + CELL_CORNERS[k].x, cz + CELL_CORNERS[k].y);
				ground[k] = column_heights[static_cast<size_t>(index[k])];
			}
			auto wet_at = [&](int k, int level) {
				return _is_wet(index[k]) && water_levels[static_cast<size_t>(index[k])] == level;
			};
			bool found = false;
			for (int k = 0; k < 4 && !found; k++) {
				if (!_is_wet(index[k])) {
					continue;
				}
				int level = water_levels[static_cast<size_t>(index[k])];
				int bed = 1 << 20;
				for (int m = 0; m < 4; m++) {
					bed = wet_at(m, level) ? std::min(bed, ground[m]) : bed;
				}
				int across = (k + 2) % 4;
				bool dry = ground[across] < level && !_is_wet(index[across]) && ground[across] >= bed;
				bool lower = ground[(k + 1) % 4] < bed || ground[(k + 3) % 4] < bed;
				if (ground[k] < level && dry && lower) {
					SpillPoint point;
					point.at = Vector2(static_cast<float>(cx) + 0.5f, static_cast<float>(cz) + 0.5f);
					point.cell = cz * (columns.x - 1) + cx;
					point.from = index[k];
					point.onto = index[across];
					points.push_back(point);
					found = true;
				}
			}
		}
	}
	return points;
}

void MCGrid::_apply_circle_spill(
		std::vector<float> &r_surface,
		std::vector<uint8_t> &r_held,
		std::vector<Color> &r_flow
) {
	water_spill_wetted.assign(water_levels.size(), 0);
	Vector2i columns = _column_counts();
	water_spill_cell_level.assign(static_cast<size_t>(std::max(columns.x - 1, 0) * std::max(columns.y - 1, 0)), 0);
	for (const SpillPoint &point : water_spill_points) {
		size_t from = static_cast<size_t>(point.from);
		size_t onto = static_cast<size_t>(point.onto);
		water_spill_cell_level[static_cast<size_t>(point.cell)] = water_levels[from];
		if (_is_wet(point.onto)) {
			continue; // another spill wetted it already
		}
		water_levels[onto] = water_levels[from];
		r_surface[onto] = r_surface[from];
		r_held[onto] = r_held[from];
		r_flow[onto] = r_flow[from];
		water_spill_wetted[onto] = 1;
	}
}

uint16_t MCGrid::_spill_mask(
		int p_cx,
		int p_cz,
		int p_level
) const {
	if (water_spill_radius <= 0.0f || water_spill_cell_level.empty()) {
		return 0;
	}
	Vector2i columns = _column_counts();
	int cells_x = columns.x - 1;
	int cells_z = columns.y - 1;
	uint16_t mask = 0;
	for (int dz = -1; dz <= 1; dz++) {
		for (int dx = -1; dx <= 1; dx++) {
			int x = p_cx + dx;
			int z = p_cz + dz;
			if (x < 0 || z < 0 || x >= cells_x || z >= cells_z) {
				continue;
			}
			if (water_spill_cell_level[static_cast<size_t>(z * cells_x + x)] == p_level) {
				mask |= static_cast<uint16_t>(1u << ((dz + 1) * 3 + (dx + 1)));
			}
		}
	}
	return mask;
}

} // namespace godot
