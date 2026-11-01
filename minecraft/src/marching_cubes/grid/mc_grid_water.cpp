// MCGrid: the water layer (heightmap mode) — Animal Crossing style ponds, rivers and the sea, plan in
// docs/TerrainStorage.md §7.
//
// Water is not a volume: each corner column has a water level L (0 = dry), like its ground height.
// The column is wet when L is above its ground (L > height); the surface then sits DROP below where
// ground of height L would be (y = L - 0.5 - DROP), so banks one tier up stand just above it.
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
// Held water and streams (_water_surfaces): water stands one layer deep (surface DROP below the next
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
// One water mesh for the whole grid (surface + falls, two surfaces), rebuilt at the end of a frame
// in which water or ground changed (flush_dirty_chunks) and on refresh_grid. Saved by save v2.
#include "marching_cubes/grid/mc_grid.h"
#include <godot_cpp/classes/array_mesh.hpp>
#include <godot_cpp/classes/geometry_instance3d.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/shader_material.hpp>
#include <godot_cpp/variant/packed_vector2_array.hpp>
#include <godot_cpp/variant/packed_vector3_array.hpp>
#include <godot_cpp/variant/packed_color_array.hpp>
#include <algorithm>
#include <cmath>
#include <vector>

namespace godot {

namespace {
const float DROP = 0.15f; // m held water's surface sits below the bank one tier up
const float STREAM = 0.1f; // m a stream's surface sits above its ground
const float SAME = 0.02f; // m: surfaces closer than this are the same water
const char *SURFACE_SHADER = "res://material/shaders/terrain/water_surface.gdshader";
const char *FALL_SHADER = "res://material/shaders/terrain/waterfall.gdshader";

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

// Append one quad (a, b, c, d counter-clockwise seen from where it faces) as two triangles.
void add_quad(
		PackedVector3Array &r_vertices,
		PackedVector3Array &r_normals,
		PackedVector2Array &r_uvs,
		PackedVector2Array &r_uv2s,
		PackedColorArray *r_colors,
		const Vector3 p_corners[4],
		const Vector2 p_uv[4],
		const Vector2 p_uv2[4],
		const Color *p_colors,
		const Vector3 &p_normal
) {
	const int order[6] = { 0, 1, 2, 0, 2, 3 };
	for (int i : order) {
		r_vertices.push_back(p_corners[i]);
		r_normals.push_back(p_normal);
		r_uvs.push_back(p_uv[i]);
		r_uv2s.push_back(p_uv2[i]);
		if (r_colors) {
			r_colors->push_back(p_colors[i]);
		}
	}
}

const Vector2i STEPS[4] = { Vector2i(1, 0), Vector2i(-1, 0), Vector2i(0, 1), Vector2i(0, -1) };
} // namespace

// The water surface height of level p_level (world y in grid space).
static float surface_y(int p_level) { return static_cast<float>(p_level) - 0.5f - DROP; }

// Does water at p_level spill onto dry ground of height p_ground beside it (ground below the surface)?
static bool _spills_onto(
		int p_ground,
		int p_level
) {
	return static_cast<float>(p_ground) - 0.5f < surface_y(p_level);
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
	std::vector<int> region = _flow_region(p_column, seen);
	last_pour = region; // for the debug overlay's replay
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

// Where water poured at p_column runs: breadth-first onto 4-neighbours whose ground is not higher
// than the column it comes from. Columns already in p_seen are skipped (and the reached ones added).
std::vector<int> MCGrid::_flow_region(
		const Vector2i &p_column,
		std::vector<uint8_t> &r_seen
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
		for (const Vector2i &step : STEPS) {
			Vector2i n = c + step;
			if (n.x < 0 || n.y < 0 || n.x >= columns.x || n.y >= columns.y) {
				continue;
			}
			size_t i = static_cast<size_t>(_column_index(n.x, n.y));
			if (!r_seen[i] && column_heights[i] <= ground) {
				r_seen[i] = 1;
				queue.push_back(n);
			}
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
		std::vector<uint8_t> &r_held
) const {
	size_t count = water_levels.size();
	r_surface.assign(count, -1e6f);
	r_held.assign(count, 0);
	Vector2i columns = _column_counts();
	auto inside = [&](const Vector2i &c) { return c.x >= 0 && c.y >= 0 && c.x < columns.x && c.y < columns.y; };
	auto is_sea = [&](size_t i) { return sea_level > 0 && water_levels[i] == sea_level && column_heights[i] < sea_level; };
	std::vector<Vector2i> open;
	for (int z = 0; z < columns.y; z++) {
		for (int x = 0; x < columns.x; x++) {
			size_t i = static_cast<size_t>(_column_index(x, z));
			if (!_is_wet(static_cast<int>(i))) {
				continue;
			}
			int level = water_levels[i];
			bool held = true;
			if (!is_sea(i)) {
				for (const Vector2i &step : STEPS) {
					Vector2i n(x + step.x, z + step.y);
					if (!inside(n)) {
						continue; // the grid's edge holds it
					}
					int other = _column_index(n.x, n.y);
					if (!_is_wet(other)) {
						held = held && !_spills_onto(column_heights[static_cast<size_t>(other)], level);
					} else if (water_levels[static_cast<size_t>(other)] < level) {
						held = held && _fall_framed(Vector2i(x, z), step, level);
					}
				}
			}
			r_held[i] = held ? 1 : 0;
			if (!held) {
				open.push_back(Vector2i(x, z));
			}
		}
	}
	// same-level water beside a stream runs out through it
	for (size_t head = 0; head < open.size(); head++) {
		Vector2i c = open[head];
		uint8_t level = water_levels[static_cast<size_t>(_column_index(c.x, c.y))];
		for (const Vector2i &step : STEPS) {
			Vector2i n = c + step;
			if (!inside(n)) {
				continue;
			}
			size_t i = static_cast<size_t>(_column_index(n.x, n.y));
			if (r_held[i] && !is_sea(i) && _is_wet(static_cast<int>(i)) && water_levels[i] == level) {
				r_held[i] = 0;
				open.push_back(n);
			}
		}
	}
	for (size_t i = 0; i < count; i++) {
		if (_is_wet(static_cast<int>(i))) {
			r_surface[i] = r_held[i] ? surface_y(water_levels[i]) : static_cast<float>(column_heights[i]) - 0.5f + STREAM;
		}
	}
}

// Per column: Color(direction x, direction z, 1 if it flows, 1 if a fall lands in it). Dry or still
// columns are Color(0, 0, 0, 0). p_surface: from _water_surfaces.
std::vector<Color> MCGrid::_water_flow(const std::vector<float> &p_surface) const {
	std::vector<Color> flow(water_levels.size(), Color(0, 0, 0, 0));
	Vector2i columns = _column_counts();
	auto inside = [&](const Vector2i &c) { return c.x >= 0 && c.y >= 0 && c.x < columns.x && c.y < columns.y; };
	std::vector<Vector2i> queue;
	// outlets: wet columns beside lower water or a spill point at it; the lower water gets foam
	for (int z = 0; z < columns.y; z++) {
		for (int x = 0; x < columns.x; x++) {
			int index = _column_index(x, z);
			if (!_is_wet(index)) {
				continue;
			}
			float surface = p_surface[static_cast<size_t>(index)];
			Vector2 out;
			for (const Vector2i &step : STEPS) {
				Vector2i n(x + step.x, z + step.y);
				if (!inside(n)) {
					continue;
				}
				size_t other = static_cast<size_t>(_column_index(n.x, n.y));
				bool lower_water = _is_wet(static_cast<int>(other)) && p_surface[other] < surface - SAME;
				bool spill = !_is_wet(static_cast<int>(other)) && static_cast<float>(column_heights[other]) - 0.5f < surface - SAME;
				if (lower_water || spill) {
					out += Vector2(static_cast<float>(step.x), static_cast<float>(step.y));
				}
				if (lower_water) {
					flow[other].a = 1.0f;
				}
			}
			if (out.length_squared() > 0.0f) {
				out = out.normalized();
				flow[static_cast<size_t>(index)] = Color(out.x, out.y, 1.0f, flow[static_cast<size_t>(index)].a);
				queue.push_back(Vector2i(x, z));
			}
		}
	}
	// breadth-first from the outlets through water at the same surface: point where we came from
	for (size_t head = 0; head < queue.size(); head++) {
		Vector2i c = queue[head];
		float surface = p_surface[static_cast<size_t>(_column_index(c.x, c.y))];
		for (const Vector2i &step : STEPS) {
			Vector2i n = c + step;
			if (!inside(n)) {
				continue;
			}
			size_t i = static_cast<size_t>(_column_index(n.x, n.y));
			if (_is_wet(static_cast<int>(i)) && std::abs(p_surface[i] - surface) < SAME && flow[i].b == 0.0f) {
				flow[i] = Color(static_cast<float>(-step.x), static_cast<float>(-step.y), 1.0f, flow[i].a);
				queue.push_back(n);
			}
		}
	}
	return flow;
}

// Rebuild the water mesh: a square per wet column at its surface (held or stream), a fall sheet
// wherever two wet neighbours' surfaces differ and wherever water stands above dry ground beside it.
void MCGrid::_rebuild_water() {
	water_dirty = false;
	PackedVector3Array vertices, normals, fall_vertices, fall_normals;
	PackedVector2Array uvs, uv2s, fall_uvs, fall_uv2s;
	PackedColorArray colors;
	Vector2i columns = _column_counts();
	float max_x = static_cast<float>(columns.x - 1);
	float max_z = static_cast<float>(columns.y - 1);
	if (heightmap_mode && water_levels.size() == column_heights.size()) {
		std::vector<float> surface;
		std::vector<uint8_t> held;
		_water_surfaces(surface, held);
		std::vector<Color> flow = _water_flow(surface);
		// Is column p_c dry land whose ground stands above p_y (a bank the water tucks under)? The bank's
		// own terrain hides whatever of the water reaches into it, so the visible water edge follows the
		// rounded cliff instead of the column's square.
		auto is_bank = [&](const Vector2i &p_c, float p_y) {
			if (p_c.x < 0 || p_c.y < 0 || p_c.x >= columns.x || p_c.y >= columns.y) {
				return false;
			}
			int i = _column_index(p_c.x, p_c.y);
			return !_is_wet(i) && static_cast<float>(column_heights[static_cast<size_t>(i)]) - 0.5f > p_y + SAME;
		};
		// a fall sheet from column p_from toward its neighbour p_step, from p_top down to p_bottom,
		// half way between the columns, facing the lower side; an end beside a bank runs on into it
		auto add_fall = [&](const Vector2i &p_from, const Vector2i &p_step, float p_top, float p_bottom) {
			Vector3 out(static_cast<float>(p_step.x), 0, static_cast<float>(p_step.y));
			Vector3 mid = Vector3(static_cast<float>(p_from.x), 0, static_cast<float>(p_from.y)) + out * 0.5f;
			Vector2i side(std::abs(p_step.y), std::abs(p_step.x));
			Vector3 across(static_cast<float>(side.x), 0, static_cast<float>(side.y));
			float reach_minus = is_bank(p_from - side, p_top) ? 1.0f : 0.5f;
			float reach_plus = is_bank(p_from + side, p_top) ? 1.0f : 0.5f;
			float width = reach_minus + reach_plus;
			float drop = p_top - p_bottom;
			Vector3 a = mid - across * reach_minus;
			Vector3 b = mid + across * reach_plus;
			Vector3 sheet[4] = { a + Vector3(0, p_bottom, 0), b + Vector3(0, p_bottom, 0), b + Vector3(0, p_top, 0),
				a + Vector3(0, p_top, 0) };
			// UV: x across in metres, y metres down from the top; UV2.x: metres above the landing
			const Vector2 fall_uv[4] = { Vector2(0, drop), Vector2(width, drop), Vector2(width, 0), Vector2(0, 0) };
			const Vector2 fall_uv2[4] = { Vector2(0, 0), Vector2(0, 0), Vector2(drop, 0), Vector2(drop, 0) };
			add_quad(fall_vertices, fall_normals, fall_uvs, fall_uv2s, nullptr, sheet, fall_uv, fall_uv2, nullptr, out);
		};
		// the vertex colour at a square's corner (toward p_sx, p_sz): the average flow of the up to
		// four columns around that corner whose water is at the same surface p_y
		auto corner_flow = [&](int p_x, int p_z, int p_sx, int p_sz, float p_y) {
			Color sum(0, 0, 0, 0);
			int count = 0;
			const Vector2i around[4] = { Vector2i(p_x, p_z), Vector2i(p_x + p_sx, p_z), Vector2i(p_x, p_z + p_sz),
				Vector2i(p_x + p_sx, p_z + p_sz) };
			for (const Vector2i &c : around) {
				if (c.x < 0 || c.y < 0 || c.x >= columns.x || c.y >= columns.y) {
					continue;
				}
				size_t i = static_cast<size_t>(_column_index(c.x, c.y));
				if (_is_wet(static_cast<int>(i)) && std::abs(surface[i] - p_y) < SAME) {
					sum += flow[i];
					count++;
				}
			}
			sum /= static_cast<float>(std::max(count, 1));
			// direction to 0..1 for the vertex colour
			return Color(sum.r * 0.5f + 0.5f, sum.g * 0.5f + 0.5f, sum.b, sum.a);
		};
		for (int z = 0; z < columns.y; z++) {
			for (int x = 0; x < columns.x; x++) {
				int index = _column_index(x, z);
				if (!_is_wet(index)) {
					continue;
				}
				float y = surface[static_cast<size_t>(index)];
				// UV2.x: 1 on a stream (the shader keeps its shore foam down)
				float stream = held[static_cast<size_t>(index)] ? 0.0f : 1.0f;
				// a flat rectangle of water between (p_x0, p_z0) and (p_x1, p_z1), kept inside the grid, with
				// the flow colour at its four corners (c00 at x0 z0, c10 at x1 z0, c11 at x1 z1, c01 at x0 z1)
				auto rect = [&](float p_x0, float p_z0, float p_x1, float p_z1, Color c00, Color c10, Color c11, Color c01) {
					if (p_x0 > p_x1) {
						std::swap(p_x0, p_x1);
						std::swap(c00, c10);
						std::swap(c01, c11);
					}
					if (p_z0 > p_z1) {
						std::swap(p_z0, p_z1);
						std::swap(c00, c01);
						std::swap(c10, c11);
					}
					p_x0 = std::max(p_x0, 0.0f);
					p_z0 = std::max(p_z0, 0.0f);
					p_x1 = std::min(p_x1, max_x);
					p_z1 = std::min(p_z1, max_z);
					if (p_x1 - p_x0 < 1e-4f || p_z1 - p_z0 < 1e-4f) {
						return;
					}
					const Vector3 quad[4] = { Vector3(p_x0, y, p_z1), Vector3(p_x1, y, p_z1), Vector3(p_x1, y, p_z0),
						Vector3(p_x0, y, p_z0) };
					const Vector2 uv[4] = { Vector2(p_x0, p_z1), Vector2(p_x1, p_z1), Vector2(p_x1, p_z0), Vector2(p_x0, p_z0) };
					const Vector2 uv2[4] = { Vector2(stream, 0), Vector2(stream, 0), Vector2(stream, 0), Vector2(stream, 0) };
					const Color tint[4] = { c01, c11, c10, c00 };
					add_quad(vertices, normals, uvs, uv2s, &colors, quad, uv, uv2, tint, Vector3(0, 1, 0));
				};
				// the flow colour at the square's corners, edge middles and centre
				Color corner[2][2]; // [sx > 0][sz > 0]
				for (int i = 0; i < 2; i++) {
					for (int j = 0; j < 2; j++) {
						corner[i][j] = corner_flow(x, z, i ? 1 : -1, j ? 1 : -1, y);
					}
				}
				Color centre = (corner[0][0] + corner[0][1] + corner[1][0] + corner[1][1]) * 0.25f;
				float fx = static_cast<float>(x);
				float fz = static_cast<float>(z);
				// four quarters; each runs on under a bank beside it (and the diagonal, if both sides are)
				for (int i = 0; i < 2; i++) {
					for (int j = 0; j < 2; j++) {
						int sx = i ? 1 : -1;
						int sz = j ? 1 : -1;
						Color c = corner[i][j];
						Color edge_x = (corner[i][0] + corner[i][1]) * 0.5f; // middle of the side toward sx
						Color edge_z = (corner[0][j] + corner[1][j]) * 0.5f; // middle of the side toward sz
						float hx = fx + 0.5f * static_cast<float>(sx);
						float hz = fz + 0.5f * static_cast<float>(sz);
						float ox = fx + static_cast<float>(sx);
						float oz = fz + static_cast<float>(sz);
						rect(fx, fz, hx, hz, centre, edge_x, c, edge_z);
						bool bank_x = is_bank(Vector2i(x + sx, z), y);
						bool bank_z = is_bank(Vector2i(x, z + sz), y);
						if (bank_x) {
							rect(hx, fz, ox, hz, edge_x, edge_x, c, c);
						}
						if (bank_z) {
							rect(fx, hz, hx, oz, edge_z, c, c, edge_z);
						}
						if (bank_x && bank_z && is_bank(Vector2i(x + sx, z + sz), y)) {
							rect(hx, hz, ox, oz, c, c, c, c);
						}
					}
				}
				// spills: a fall onto dry ground lower than the surface, any side
				for (const Vector2i &step : STEPS) {
					Vector2i n(x + step.x, z + step.y);
					if (n.x < 0 || n.y < 0 || n.x >= columns.x || n.y >= columns.y) {
						continue;
					}
					int other = _column_index(n.x, n.y);
					float ground = static_cast<float>(column_heights[static_cast<size_t>(other)]) - 0.5f;
					if (!_is_wet(other) && ground < y - SAME) {
						add_fall(Vector2i(x, z), step, y, ground);
					}
				}
				// falls to the +x and +z neighbours (each pair once): half way between the columns
				const Vector2i sides[2] = { Vector2i(1, 0), Vector2i(0, 1) };
				for (const Vector2i &side : sides) {
					int nx = x + side.x;
					int nz = z + side.y;
					if (nx >= columns.x || nz >= columns.y) {
						continue;
					}
					int other = _column_index(nx, nz);
					if (!_is_wet(other)) {
						continue;
					}
					float y_other = surface[static_cast<size_t>(other)];
					if (y > y_other + SAME) {
						add_fall(Vector2i(x, z), side, y, y_other);
					} else if (y_other > y + SAME) {
						add_fall(Vector2i(nx, nz), -side, y_other, y);
					}
				}
			}
		}
		if (debug_water) {
			_rebuild_water_debug(surface, held, flow);
		}
	}
	if (vertices.is_empty()) {
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
		water_material = shader_material(SURFACE_SHADER);
	}
	if (waterfall_material.is_null()) {
		waterfall_material = shader_material(FALL_SHADER);
	}
	Ref<ArrayMesh> mesh;
	mesh.instantiate();
	Array arrays;
	arrays.resize(Mesh::ARRAY_MAX);
	arrays[Mesh::ARRAY_VERTEX] = vertices;
	arrays[Mesh::ARRAY_NORMAL] = normals;
	arrays[Mesh::ARRAY_TEX_UV] = uvs;
	arrays[Mesh::ARRAY_TEX_UV2] = uv2s;
	arrays[Mesh::ARRAY_COLOR] = colors;
	mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, arrays);
	mesh->surface_set_material(0, water_material);
	if (!fall_vertices.is_empty()) {
		Array falls;
		falls.resize(Mesh::ARRAY_MAX);
		falls[Mesh::ARRAY_VERTEX] = fall_vertices;
		falls[Mesh::ARRAY_NORMAL] = fall_normals;
		falls[Mesh::ARRAY_TEX_UV] = fall_uvs;
		falls[Mesh::ARRAY_TEX_UV2] = fall_uv2s;
		mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, falls);
		mesh->surface_set_material(1, waterfall_material);
	}
	water_node->set_mesh(mesh);
}

} // namespace godot
