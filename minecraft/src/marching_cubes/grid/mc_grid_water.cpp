// MCGrid: the water layer (heightmap mode) — Animal Crossing style ponds, rivers and the sea, plan in
// docs/TerrainStorage.md §7.
//
// Water is not a volume: each corner column has a water level L (0 = dry), like its ground height.
// The column is wet when L is above its ground (L > height); the surface then sits water_drop below where
// ground of height L would be (y = L - 0.5 - water_drop), so banks one tier up stand just above it.
// Drawn on the same grid as the cliffs: a column's water is a 1 x 1 square centred on the column,
// and a cliff between two columns stands half way between them — so where two wet neighbours have
// different levels, the step is exactly where the cliff is, and a waterfall sheet fills it.
// Waterfalls are never stored: raise or lower a level and they appear / go by themselves.
//
//   set_water_level   one column
//   fill_water        pour water on a column: a flood fill that flows left / right / front / back
//                     onto ground that is not higher (same tier: spreads; lower: falls and carries on
//                     down there) and stands one layer deep wherever it reaches (level = ground + 1).
//                     Higher ground is a bank. A dug channel fills and runs down its steps as
//                     waterfalls into a basin or the sea. (The Water tool's left click.)
//   paint_water       waterscaping: the column's ground dug one tier and water at its old height
//   clear_water       dry the connected water (any level: a river with its falls), not the sea
//   sea_level         randomize_heights floods the sea from the grid's corner at this level (0: none):
//                     every connected column below it
// Held water and streams (_water_surfaces): water stands one layer deep (surface water_drop below the next
// tier) only where banks hold it in — every side a bank, the same held water, or a waterfall cut into
// the land (_fall_framed) onto lower water; the sea is held. Elsewhere (it ran out of a channel onto
// open ground, or a tier's open edge is beside it) it is a stream: a thin sheet STREAM above the ground
// that keeps running and pours over the edge. Same-level water joined to a stream drains through it,
// so it is a stream too. Derived when the mesh is built; only the levels are stored.
// Spills: where water stands beside dry ground lower than its surface, a fall sheet runs down onto it.
// Flow (_water_flow): water runs toward the nearest fall. A column next to lower water or a spill
// is an outlet (it points at the fall); a breadth-first walk from the outlets through same-level
// water points every other column of the body at the one it was reached from. A body with no
// outlet (a pond, the sea) is still. The column a fall lands in gets foam. The mesh carries this in
// its vertex COLOR (averaged with same-level neighbours at the square's corners, so it turns
// smoothly) for water_surface.gdshader: rg direction as 0..1, b flows, a landing foam.
// The mesh itself is built cell by cell with the cliff tiles' shape (mc_grid_water_mesh.cpp), so its
// edges and falls follow the cliffs exactly.
// One water mesh for the whole grid (surface + falls, two surfaces), rebuilt at the end of a frame
// in which water or ground changed (flush_dirty_chunks) and on refresh_grid. Saved by save v2.
#include "marching_cubes/grid/mc_grid.h"
#include <godot_cpp/classes/array_mesh.hpp>
#include <godot_cpp/classes/geometry_instance3d.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/shader_material.hpp>
#include <godot_cpp/classes/time.hpp>
#include <godot_cpp/variant/packed_vector2_array.hpp>
#include <godot_cpp/variant/packed_vector3_array.hpp>
#include <godot_cpp/variant/packed_color_array.hpp>
#include <algorithm>
#include <cmath>
#include <vector>

namespace godot {

namespace {
const float STREAM = 0.1f; // m a stream's surface sits above its ground
const float SAME = 0.02f; // m: surfaces closer than this are the same water
const char *WATER_SHADER = "res://material/shaders/terrain/water.gdshader"; // surfaces + falls

// A material from a shader path (null if the shader isn't there).
Ref<Material> shader_material(const char *p_path) {
	Ref<Shader> shader = ResourceLoader::get_singleton()->load(p_path);
	if (shader.is_null()) {
		return Ref<Material>();
	}
	Ref<ShaderMaterial> material;
	material.instantiate();
	material->set_shader(shader);
	return material;
}

const Vector2i STEPS[4] = { Vector2i(1, 0), Vector2i(-1, 0), Vector2i(0, 1), Vector2i(0, -1) };
} // namespace

// The water surface height of level p_level (world y in grid space).
float MCGrid::_surface_y(int p_level) const { return static_cast<float>(p_level) - 0.5f - water_drop; }

void MCGrid::set_water_drop(float p_drop) {
	water_drop = Math::clamp(p_drop, 0.0f, 0.95f);
	if (!chunks.empty() && water_levels.size() == column_heights.size()) {
		_queue_water();
	}
}

// Does water at p_level spill onto dry ground of height p_ground beside it (ground below the surface)?
static bool _spills_onto(
		int p_ground,
		int p_level,
		float p_drop
) {
	return static_cast<float>(p_ground) - 0.5f < static_cast<float>(p_level) - 0.5f - p_drop;
}

int MCGrid::get_water_level(const Vector2i &p_column) const {
	int index = _column_index(p_column.x, p_column.y);
	if (index < 0 || static_cast<size_t>(index) >= water_levels.size()) {
		return 0;
	}
	return water_levels[static_cast<size_t>(index)];
}

// Is column p_index wet (water above its ground)?
bool MCGrid::_is_wet(int p_index) const {
	return static_cast<size_t>(p_index) < water_levels.size() && static_cast<size_t>(p_index) < column_heights.size() &&
			water_levels[static_cast<size_t>(p_index)] > column_heights[static_cast<size_t>(p_index)];
}

void MCGrid::set_water_level(
		const Vector2i &p_column,
		int p_level
) {
	int index = _column_index(p_column.x, p_column.y);
	if (!heightmap_mode || index < 0 || static_cast<size_t>(index) >= water_levels.size()) {
		return;
	}
	_record_water(index);
	water_levels[static_cast<size_t>(index)] = static_cast<uint8_t>(std::clamp(p_level, 0, _max_column_height()));
	_queue_water();
}

// The sea: every column connected to p_column whose ground is below p_level, all at p_level.
std::vector<int> MCGrid::_sea_region(
		const Vector2i &p_column,
		int p_level
) const {
	std::vector<int> region;
	int start = _column_index(p_column.x, p_column.y);
	if (!heightmap_mode || start < 0 || static_cast<size_t>(start) >= column_heights.size() ||
		column_heights[static_cast<size_t>(start)] >= p_level) {
		return region;
	}
	Vector2i columns = _column_counts();
	std::vector<uint8_t> seen(column_heights.size(), 0);
	std::vector<Vector2i> stack = { p_column };
	seen[static_cast<size_t>(start)] = 1;
	while (!stack.empty()) {
		Vector2i c = stack.back();
		stack.pop_back();
		region.push_back(_column_index(c.x, c.y));
		for (const Vector2i &step : STEPS) {
			Vector2i n = c + step;
			if (n.x < 0 || n.y < 0 || n.x >= columns.x || n.y >= columns.y) {
				continue;
			}
			size_t i = static_cast<size_t>(_column_index(n.x, n.y));
			if (!seen[i] && column_heights[i] < p_level) {
				seen[i] = 1;
				stack.push_back(n);
			}
		}
	}
	return region;
}

// Pour water at p_column: it flows left / right / front / back wherever the ground beside it is not
// higher than the ground under it (same tier: it spreads, lower: it falls and carries on down
// there), and stands one layer deep everywhere it reaches (level = ground + 1). Higher ground is a
// bank. So it fills a dug channel, runs down its steps as waterfalls and stops in a basin or the sea.
int MCGrid::fill_water(
		const Vector2i &p_column,
		int p_level
) {
	int start = _column_index(p_column.x, p_column.y);
	if (!heightmap_mode || start < 0 || static_cast<size_t>(start) >= column_heights.size()) {
		return 0;
	}
	int top = _max_column_height();
	auto depth_one = [&](size_t p_index) { return std::min(static_cast<int>(column_heights[p_index]) + 1, top); };
	// the poured level (p_level, at most one layer deep; below the ground: nothing)
	int level = std::min(p_level, depth_one(static_cast<size_t>(start)));
	if (level <= column_heights[static_cast<size_t>(start)]) {
		return 0;
	}
	std::vector<uint8_t> seen(column_heights.size(), 0);
	last_pour_trace.clear();
	water_trace_kind = 0; // the step buttons go through this pour
	std::vector<int> region = _flow_region(p_column, seen, &last_pour_trace);
	last_pour = region; // for the debug overlay's replay and step-by-step view
	pour_step = -1;
	for (int index : region) {
		size_t i = static_cast<size_t>(index);
		int wanted = index == start ? level : depth_one(i);
		if (water_levels[i] != wanted) {
			_record_water(index);
			water_levels[i] = static_cast<uint8_t>(wanted);
		}
	}
	_queue_water();
	return static_cast<int>(region.size());
}

// Where water poured at p_column runs: a breadth-first flood fill. The poured column goes in a queue;
// each step takes the column at the front and looks at its 4 neighbours (+x, -x, +z, -z): off the
// grid -> skipped; already queued -> skipped; ground higher than this column's -> rejected (a bank);
// otherwise (same or lower ground) -> marked and added to the back of the queue. It ends when the
// queue is used up; the queue in order is the region (nearest first). Columns already in r_seen are
// skipped (and the reached ones added). r_trace (optional) records every step for the debug view.
std::vector<int> MCGrid::_flow_region(
		const Vector2i &p_column,
		std::vector<uint8_t> &r_seen,
		std::vector<MCPourStep> *r_trace
) const {
	std::vector<int> region;
	int start = _column_index(p_column.x, p_column.y);
	if (start < 0 || r_seen[static_cast<size_t>(start)]) {
		return region;
	}
	Vector2i columns = _column_counts();
	std::vector<Vector2i> queue = { p_column };
	r_seen[static_cast<size_t>(start)] = 1;
	for (size_t head = 0; head < queue.size(); head++) {
		Vector2i c = queue[head];
		int index = _column_index(c.x, c.y);
		region.push_back(index);
		int ground = column_heights[static_cast<size_t>(index)];
		MCPourStep trace;
		trace.column = index;
		for (int k = 0; k < 4; k++) {
			Vector2i n = c + STEPS[k];
			if (n.x < 0 || n.y < 0 || n.x >= columns.x || n.y >= columns.y) {
				trace.result[k] = MCPourStep::OUTSIDE;
				continue;
			}
			size_t i = static_cast<size_t>(_column_index(n.x, n.y));
			if (r_seen[i]) {
				trace.result[k] = MCPourStep::SEEN;
			} else if (column_heights[i] > ground) {
				trace.result[k] = MCPourStep::HIGHER;
			} else {
				trace.result[k] = MCPourStep::QUEUED;
				r_seen[i] = 1;
				queue.push_back(n);
			}
		}
		if (r_trace) {
			trace.queue_size = static_cast<int>(queue.size());
			trace.head = static_cast<int>(head);
			r_trace->push_back(trace);
		}
	}
	return region;
}

// Let all water flow again (after a load or a terrain edit): every wet column, highest ground first,
// pours one layer deep wherever it can reach; a column already reached isn't poured again. Water
// that is left standing deeper than one layer is brought down to one layer. The sea stays as it is.
int MCGrid::settle_water() {
	if (!heightmap_mode || water_levels.size() != column_heights.size()) {
		return 0;
	}
	std::vector<int> sources;
	for (size_t i = 0; i < water_levels.size(); i++) {
		bool sea = sea_level > 0 && water_levels[i] == sea_level;
		if (_is_wet(static_cast<int>(i)) && !sea) {
			sources.push_back(static_cast<int>(i));
		}
	}
	std::stable_sort(sources.begin(), sources.end(), [&](int p_a, int p_b) {
		return column_heights[static_cast<size_t>(p_a)] > column_heights[static_cast<size_t>(p_b)];
	});
	Vector2i columns = _column_counts();
	int top = _max_column_height();
	std::vector<uint8_t> seen(column_heights.size(), 0);
	int changed = 0;
	for (int source : sources) {
		Vector2i column(source % columns.x, source / columns.x);
		for (int index : _flow_region(column, seen)) {
			size_t i = static_cast<size_t>(index);
			bool sea = sea_level > 0 && water_levels[i] == sea_level && column_heights[i] < sea_level;
			int wanted = sea ? sea_level : std::min(static_cast<int>(column_heights[i]) + 1, top);
			if (water_levels[i] != wanted) {
				_record_water(index);
				water_levels[i] = static_cast<uint8_t>(wanted);
				changed++;
			}
		}
	}
	if (changed > 0) {
		_queue_water();
	}
	return changed;
}

int MCGrid::trace_settle() {
	water_trace_kind = 0;
	last_pour.clear();
	last_pour_trace.clear();
	pour_step = -1;
	if (!heightmap_mode || water_levels.size() != column_heights.size()) {
		return 0;
	}
	// the sources, as settle_water picks them
	std::vector<int> sources;
	for (size_t i = 0; i < water_levels.size(); i++) {
		bool sea = sea_level > 0 && water_levels[i] == sea_level;
		if (_is_wet(static_cast<int>(i)) && !sea) {
			sources.push_back(static_cast<int>(i));
		}
	}
	std::stable_sort(sources.begin(), sources.end(), [&](int p_a, int p_b) {
		return column_heights[static_cast<size_t>(p_a)] > column_heights[static_cast<size_t>(p_b)];
	});
	Vector2i columns = _column_counts();
	std::vector<uint8_t> seen(column_heights.size(), 0);
	int pour = 0;
	for (int source : sources) {
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

// Every column in p_region gets water level p_level (recorded for undo).
void MCGrid::_set_water(
		const std::vector<int> &p_region,
		int p_level
) {
	for (int index : p_region) {
		_record_water(index);
		water_levels[static_cast<size_t>(index)] = static_cast<uint8_t>(p_level);
	}
	if (!p_region.empty()) {
		_queue_water();
	}
}

// The sea: flood from p_column at p_level over everything below it.
void MCGrid::_flood_sea(
		const Vector2i &p_column,
		int p_level
) {
	int level = std::clamp(p_level, 0, _max_column_height());
	_set_water(_sea_region(p_column, level), level);
}

bool MCGrid::paint_water(const Vector2i &p_column) {
	int index = _column_index(p_column.x, p_column.y);
	if (!heightmap_mode || index < 0 || static_cast<size_t>(index) >= column_heights.size() || _is_wet(index)) {
		return false;
	}
	int height = column_heights[static_cast<size_t>(index)];
	if (height < 1) {
		return false;
	}
	set_column_height(p_column, height - 1);
	if (column_heights[static_cast<size_t>(index)] != height - 1) {
		return false; // a locked rim column
	}
	set_water_level(p_column, height);
	return true;
}

// Dry the water connected to p_column (4 neighbours, any level: a whole river with its falls). The
// sea (water at sea_level) stays, so drying a river that runs into it doesn't empty the sea.
int MCGrid::clear_water(const Vector2i &p_column) {
	int start = _column_index(p_column.x, p_column.y);
	auto is_sea = [&](size_t p_index) { return sea_level > 0 && water_levels[p_index] == sea_level; };
	if (start < 0 || !_is_wet(start) || is_sea(static_cast<size_t>(start))) {
		return 0;
	}
	Vector2i columns = _column_counts();
	std::vector<Vector2i> stack = { p_column };
	_record_water(start);
	water_levels[static_cast<size_t>(start)] = 0;
	int cleared = 0;
	while (!stack.empty()) {
		Vector2i c = stack.back();
		stack.pop_back();
		cleared++;
		for (const Vector2i &step : STEPS) {
			Vector2i n = c + step;
			if (n.x < 0 || n.y < 0 || n.x >= columns.x || n.y >= columns.y) {
				continue;
			}
			size_t i = static_cast<size_t>(_column_index(n.x, n.y));
			if (_is_wet(static_cast<int>(i)) && !is_sea(i)) {
				_record_water(static_cast<int>(i));
				water_levels[i] = 0;
				stack.push_back(n);
			}
		}
	}
	_queue_water();
	return cleared;
}

// Sea level for randomize_heights (0: no sea).
void MCGrid::set_sea_level(int p_level) { sea_level = std::max(p_level, 0); }

// The water mesh needs rebuilding at the end of this frame.
void MCGrid::_queue_water() {
	water_dirty = true;
	_queue_rebuild();
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
	const char *side_names[4] = { "+x", "-x", "+z", "-z" };
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
							step->text += vformat("\n%s: the grid's edge, holds", side_names[k]);
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
						step->text += vformat("\n%s %s: %s", side_names[k], _column_name(other), why);
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
	// terrain cell, and the cell's water (mc_grid_water_mesh.cpp) has one surface per level, so a held
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

// Rebuild the water mesh: surfaces and falls cell by cell with the cliff tiles' shape
// (mc_grid_water_mesh.cpp), the flow in the vertex colour.
void MCGrid::_rebuild_water() {
	uint64_t started = Time::get_singleton()->get_ticks_usec();
	water_dirty = false;
	MCWaterArrays arrays_out;
	if (heightmap_mode && water_levels.size() == column_heights.size()) {
		// diagonal spills, for this build only (mc_grid_water_spill.cpp): swapped back below
		water_spill_points = water_spill_radius > 0.0f ? _spill_points() : std::vector<SpillPoint>(); // on the stored water
		std::vector<uint8_t> stored = _spilled_levels(&water_spill_steps);
		water_levels.swap(stored);
		std::vector<float> surface;
		std::vector<uint8_t> held;
		_water_surfaces(surface, held);
		std::vector<Color> flow = _water_flow(surface);
		_apply_circle_spill(surface, held, flow); // after held / flow: the spill changes neither
		if (water_sampled) {
			_emit_water_tiles(surface, held, flow, arrays_out);
		} else {
			_emit_water_cells(surface, held, flow, arrays_out);
			_merge_falls(arrays_out);
		}
		water_debug_samples = arrays_out.samples;
		uint64_t t0 = Time::get_singleton()->get_ticks_usec();
		bool stages = debug_water && water_debug_stage > 0;
		water_debug_edges = stages ? arrays_out.edges : std::vector<WaterEdge>();
		water_debug_chained.clear();
		water_outlines = _water_outlines(arrays_out.edges, stages ? &water_debug_chained : nullptr);
		uint64_t t1 = Time::get_singleton()->get_ticks_usec();
		uint64_t t2 = t1;
		uint64_t t3 = t1;
		if (water_sampled && water_outline_fill) {
			_fill_water_surface(flow, arrays_out);
			t2 = Time::get_singleton()->get_ticks_usec();
			_sweep_falls(flow, arrays_out);
			t3 = Time::get_singleton()->get_ticks_usec();
		}
		water_stage_usec[0] = t1 - t0; // outlines
		water_stage_usec[1] = t2 - t1; // fill
		water_stage_usec[2] = t3 - t2; // sweep
		if (debug_water) {
			_rebuild_water_debug(surface, held, flow);
		}
		water_levels.swap(stored); // the stored levels back
	}
	_rebuild_water_particles(arrays_out);
	water_rebuild_usec = Time::get_singleton()->get_ticks_usec() - started; // (mesh upload below not included)
	if (arrays_out.vertices.is_empty()) {
		if (water_node) {
			water_node->set_mesh(Ref<Mesh>());
		}
		if (water_debug_node) {
			water_debug_node->hide();
		}
		return;
	}
	if (!water_node) {
		water_node = memnew(MeshInstance3D);
		water_node->set_name("Water");
		water_node->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
		add_child(water_node);
	}
	if (water_material.is_null()) {
		water_material = shader_material(WATER_SHADER);
	}
	water_node->set_mesh(_water_mesh(arrays_out));
	_apply_water_view();
}

// The water mesh: ONE surface with one material (water.gdshader) for the surfaces and the falls, so
// where a fall's strip leaves the surface the same shader runs on both sides (no seam). The falls'
// triangles come first in the index list: they write depth, so a surface behind a fall is hidden
// and one in front blends over it. UV2 per vertex: over the lip (0 on the surfaces), and metres
// above the landing (falls) or -1 on a stream (see water.gdshader).
Ref<ArrayMesh> MCGrid::_water_mesh(const MCWaterArrays &p_arrays) const {
	int surface_count = static_cast<int>(p_arrays.vertices.size());
	int fall_count = static_cast<int>(p_arrays.fall_vertices.size());
	PackedVector3Array vertices = p_arrays.vertices;
	vertices.append_array(p_arrays.fall_vertices);
	PackedVector3Array normals = p_arrays.normals;
	normals.append_array(p_arrays.fall_normals);
	PackedVector2Array uvs = p_arrays.uvs;
	uvs.append_array(p_arrays.fall_uvs);
	PackedColorArray colors = p_arrays.colors;
	PackedVector2Array uv2s;
	if (p_arrays.final_uv2) {
		uv2s = p_arrays.uv2s; // the water tiles builder (falls included in the surface arrays)
	} else { // the per-cell builder: uv2 (stream, 0)
		for (int i = 0; i < surface_count; i++) {
			bool stream = i < p_arrays.uv2s.size() && p_arrays.uv2s[i].x > 0.5f;
			uv2s.push_back(Vector2(0.0f, stream ? -1.0f : 0.0f));
		}
	}
	for (int j = 0; j < fall_count; j++) { // the per-cell builder's flat sheets: all falling water
		colors.push_back(Color(0.5f, 0.5f, 1.0f, 0.0f));
		uv2s.push_back(Vector2(1.0f, j < p_arrays.fall_uv2s.size() ? p_arrays.fall_uv2s[j].x : 0.0f));
	}
	PackedInt32Array indices;
	auto add_indices = [&](const PackedInt32Array &p_list, int p_count, int p_offset) {
		if (p_list.is_empty()) {
			for (int i = 0; i < p_count; i++) {
				indices.push_back(i + p_offset);
			}
			return;
		}
		for (int i = 0; i < p_list.size(); i++) {
			indices.push_back(p_list[i] + p_offset);
		}
	};
	add_indices(p_arrays.fall_indices, fall_count, surface_count); // the falls first
	for (int i = 0; i < p_arrays.first_indices.size(); i++) { // (water tiles: the drops)
		indices.push_back(p_arrays.first_indices[i]);
	}
	add_indices(p_arrays.indices, surface_count, 0);
	// every triangle wound to face the way its vertex normals point: the builders put corners in
	// whatever order their shape gave (a fall's ends a / b come either way round), and a triangle
	// seen from its back gets its normal flipped by the renderer (cull_disabled), so it is lit as if
	// it faced away — dark pieces among light ones
	int32_t *ix = indices.ptrw();
	for (int64_t t = 0; t + 2 < indices.size(); t += 3) {
		const Vector3 &a = vertices[ix[t]];
		const Vector3 &b = vertices[ix[t + 1]];
		const Vector3 &c = vertices[ix[t + 2]];
		Vector3 average = normals[ix[t]] + normals[ix[t + 1]] + normals[ix[t + 2]];
		// Godot's front faces are clockwise: their (b - a) x (c - a) points away from the normal
		if ((b - a).cross(c - a).dot(average) > 0.0f) {
			std::swap(ix[t + 1], ix[t + 2]);
		}
	}

	Array arrays;
	arrays.resize(Mesh::ARRAY_MAX);
	arrays[Mesh::ARRAY_VERTEX] = vertices;
	arrays[Mesh::ARRAY_NORMAL] = normals;
	arrays[Mesh::ARRAY_TEX_UV] = uvs;
	arrays[Mesh::ARRAY_COLOR] = colors;
	arrays[Mesh::ARRAY_TEX_UV2] = uv2s;
	// CUSTOM0.x: the water's depth, baked (the per-cell builder has none: deep)
	PackedFloat32Array depths;
	depths.resize(vertices.size());
	depths.fill(2.0f);
	for (int i = 0; i < p_arrays.depths.size() && i < depths.size(); i++) {
		depths.set(i, p_arrays.depths[i]);
	}
	arrays[Mesh::ARRAY_CUSTOM0] = depths;
	arrays[Mesh::ARRAY_INDEX] = indices;
	Ref<ArrayMesh> mesh;
	mesh.instantiate();
	uint64_t format = static_cast<uint64_t>(Mesh::ARRAY_CUSTOM_R_FLOAT) << Mesh::ARRAY_FORMAT_CUSTOM0_SHIFT;
	mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, arrays, TypedArray<Array>(), Dictionary(),
			BitField<Mesh::ArrayFormat>(format));
	mesh->surface_set_material(0, water_material);
	return mesh;
}

} // namespace godot
