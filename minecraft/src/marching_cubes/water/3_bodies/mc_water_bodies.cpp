// MCGrid: water step 3, the bodies. Each cell between 4 corner columns gets one flat sheet of water
// (a body) per water level among its corners, with its corner classes (bank, wet, lower, dry), its
// surface height, flow and where its fall lands. The shape of a body (mc_water_shape.h) uses the
// same function as the cliff tiles (blender/mc_tiles/scripts/cliff_tiles.py: the rounded bilinear
// blend, or the blocky quarters), so the water's edges land on the cliff edges. The water tiles
// (mc_water_tiles.cpp) turn the bodies into the mesh.
//
// Coordinates: the shape works in the tiles' Blender plane (x, y), cell-local -0.5 .. 0.5, corners
// 0 (-x, -y), 1 (+x, -y), 2 (+x, +y), 3 (-x, +y) in hash bit order; Godot z = -y, so corner 0 is the
// column (x, z + 1), 1 (x + 1, z + 1), 2 (x + 1, z), 3 (x, z).
#include "marching_cubes/grid/mc_grid.h"
#include "marching_cubes/water/3_bodies/mc_water_shape.h"
#include "marching_cubes/water/4_tiles/mc_water_tile.h"
#include <godot_cpp/templates/hash_map.hpp>
#include <algorithm>
#include <cmath>
#include <vector>

namespace godot {

using namespace mc_water;

namespace {
const float BED_CUT = -0.05f; // per-mesh banks: the fall line's cut, metres under the bed tile's grass
} // namespace

void MCGrid::set_water_square(bool p_on) {
	water_square = p_on;
	clear_tile_cache(); // tiles of the old style aren't used again
	if (!chunks.empty() && water_levels.size() == column_heights.size()) {
		_queue_water();
	}
}

void MCGrid::set_water_blocky_groups(const String &p_groups) {
	water_blocky_groups = p_groups;
	clear_tile_cache(); // tiles of the old style aren't used again
	if (!chunks.empty() && water_levels.size() == column_heights.size()) {
		_queue_water(); // not while the scene is still loading (the grid isn't built yet)
	}
}

// Whether the water of cell (p_cx, p_cz) over bed tier p_bed is blocky (its group is listed).
bool MCGrid::is_water_blocky(
		int p_cx,
		int p_cz,
		int p_bed
) const {
	int cell_levels = std::max(_dims().max_corner().y, 1);
	char group = _group_at(Vector3i(p_cx, std::clamp(p_bed - 1, 0, cell_levels - 1), p_cz));
	return water_blocky_groups.contains(String::chr(static_cast<char32_t>(group)));
}

// The water bodies of cell (p_cx, p_cz): one per water level among its 4 corners, with its shape
// layers, surface height and flow; r_bottoms[k]: where body k's fall lands (below its bed's edge:
// the water below, or the dry ground below; -1e6 when nothing falls).
void MCGrid::_water_bodies(
		int p_cx,
		int p_cz,
		const std::vector<float> &p_surface,
		const std::vector<uint8_t> &p_held,
		const std::vector<Color> &p_flow,
		std::vector<mc_water::Body> &r_bodies,
		std::vector<float> &r_bottoms
) const {
	r_bodies.clear();
	r_bottoms.clear();
	// the 4 corner columns, in hash bit order
	const Vector2i at[4] = { Vector2i(p_cx, p_cz + 1), Vector2i(p_cx + 1, p_cz + 1), Vector2i(p_cx + 1, p_cz),
		Vector2i(p_cx, p_cz) };
	size_t index[4];
	int ground[4];
	int level[4];
	bool wet[4];
	bool any_wet = false;
	for (int i = 0; i < 4; i++) {
		index[i] = static_cast<size_t>(_column_index(at[i].x, at[i].y));
		ground[i] = column_heights[index[i]];
		wet[i] = _is_wet(static_cast<int>(index[i]));
		level[i] = wet[i] ? water_levels[index[i]] : 0;
		any_wet = any_wet || wet[i];
	}
	if (!any_wet) {
		return;
	}
	// each water level in the cell is a body (done from its first corner)
	for (int i = 0; i < 4; i++) {
		bool first = wet[i];
		for (int k = 0; k < i && first; k++) {
			first = !(wet[k] && level[k] == level[i]);
		}
		if (!first) {
			continue;
		}
		int body_level = level[i];
		int bed = 255;
		float top = -1e6f;
		bool stream = false;
		Color body_flow(0, 0, 0, 0);
		int members = 0;
		for (int k = 0; k < 4; k++) {
			if (wet[k] && level[k] == body_level) {
				bed = std::min(bed, ground[k]);
				top = std::max(top, p_surface[index[k]]);
				stream = stream || !p_held[index[k]];
				body_flow += p_flow[index[k]];
				members++;
			}
		}
		body_flow = body_flow * (1.0f / static_cast<float>(members));
		bool blocky = is_water_blocky(p_cx, p_cz, bed); // the shape style of the bed's cell (its group)
		Body body;
		float rounding = water_square ? 0.0f : ROUNDING;
		for (Layer *layer : { &body.above, &body.bed, &body.wet }) {
			layer->blocky = blocky;
			layer->rounding = rounding;
		}
		for (int k = 0; k < 4; k++) {
			body.above.solid[k] = ground[k] >= body_level;
			body.bed.solid[k] = ground[k] >= bed;
			body.wet.solid[k] = body.above.solid[k] || (wet[k] && level[k] == body_level) || ground[k] < bed;
			body.flow[k] = wet[k] && level[k] == body_level ? p_flow[index[k]] : body_flow;
			body.ground[k] = ground[k];
		}
		body.top = top;
		// the banks and the fall line: slices of the tile meshes, baked once per mesh (mc_water_banks_slice.cpp)
		int waterline = static_cast<int>(std::floor(top)); // the tile layer the surface is in
		uint64_t above_key = 0;
		uint64_t bed_key = 0;
		body.baked_above = _slice_field(Vector3i(p_cx, waterline, p_cz), top - (static_cast<float>(waterline) + 0.5f), above_key).data();
		// the bed's edge (the fall line): the tile holding the bed's grass (at its middle, local 0), cut
		// just under the grass, above the cliff face's grooves (cut in them, at -0.25, the fall line
		// wanders in and out with the grooves and the low falls break up)
		body.baked_bed = _slice_field(Vector3i(p_cx, bed - 1, p_cz), BED_CUT, bed_key).data();
		body.baked_key = (above_key * 0x9E3779B97F4A7C15ULL) ^ (bed_key + 0x632BE59BD9B4E019ULL);
		body.fall_lip = water_fall_lip;
		body.square = water_square;
		body.stream = stream;
		// where its fall lands: the water below, or the dry ground below
		float bottom = -1e6f;
		for (int k = 0; k < 4; k++) {
			if (ground[k] < bed) {
				float y = wet[k] ? p_surface[index[k]] : static_cast<float>(ground[k]) - 0.5f;
				bottom = std::max(bottom, y);
			}
		}
		r_bodies.push_back(body);
		r_bottoms.push_back(bottom);
	}
}

} // namespace godot
