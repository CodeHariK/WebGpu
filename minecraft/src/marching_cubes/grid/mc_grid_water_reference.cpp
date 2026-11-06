// MCGrid: the builder's step 2 results per column, for water_from_scratch to check its own against.
//
// Runs the same passes _rebuild_water runs before it builds the mesh: the diagonal spills swapped in
// (_spilled_levels), held / stream and the surfaces (_water_surfaces), the flow (_water_flow); then
// the stored levels go back. Nothing is built or changed.
#include "marching_cubes/grid/mc_grid.h"
#include "marching_cubes/grid/mc_water_shape.h"
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/packed_color_array.hpp>
#include <godot_cpp/variant/packed_float32_array.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>
#include <vector>

namespace godot {

Dictionary MCGrid::get_water_columns_debug() {
	Dictionary out;
	if (!heightmap_mode || water_levels.size() != column_heights.size()) {
		return out;
	}
	std::vector<uint8_t> stored = _spilled_levels();
	water_levels.swap(stored); // the spilled levels in, as _rebuild_water does
	std::vector<float> surface;
	std::vector<uint8_t> held;
	_water_surfaces(surface, held);
	std::vector<Color> flow = _water_flow(surface);
	PackedInt32Array levels_out;
	PackedByteArray held_out;
	PackedFloat32Array surface_out;
	PackedColorArray flow_out;
	for (size_t i = 0; i < water_levels.size(); i++) {
		levels_out.push_back(water_levels[i]);
		held_out.push_back(held[i]);
		surface_out.push_back(surface[i]);
		flow_out.push_back(flow[i]);
	}
	water_levels.swap(stored); // the stored levels back
	out["levels"] = levels_out;
	out["held"] = held_out;
	out["surface"] = surface_out;
	out["flow"] = flow_out;
	return out;
}

Array MCGrid::get_water_bodies_debug() {
	Array out;
	if (!heightmap_mode || water_levels.size() != column_heights.size()) {
		return out;
	}
	// what _rebuild_water does before it builds the mesh
	water_spill_points = water_spill_radius > 0.0f ? _spill_points() : std::vector<SpillPoint>();
	std::vector<uint8_t> stored = _spilled_levels();
	water_levels.swap(stored);
	std::vector<float> surface;
	std::vector<uint8_t> held;
	_water_surfaces(surface, held);
	std::vector<Color> flow = _water_flow(surface);
	_apply_circle_spill(surface, held, flow);
	Vector2i columns = _column_counts();
	std::vector<mc_water::Body> bodies;
	std::vector<float> bottoms;
	for (int cz = 0; cz + 1 < columns.y; cz++) {
		for (int cx = 0; cx + 1 < columns.x; cx++) {
			_water_bodies(cx, cz, surface, held, flow, bodies, bottoms);
			for (size_t b = 0; b < bodies.size(); b++) {
				const mc_water::Body &body = bodies[b];
				PackedByteArray corners;
				for (int k = 0; k < 4; k++) {
					int c = body.above.solid[k] ? 0 : (!body.bed.solid[k] ? 2 : (body.wet.solid[k] ? 1 : 3));
					corners.push_back(static_cast<uint8_t>(c));
				}
				Dictionary d;
				d["cx"] = cx;
				d["cz"] = cz;
				d["corners"] = corners;
				d["top"] = body.top;
				d["stream"] = body.stream;
				d["bottom"] = bottoms[b];
				d["mask"] = body.spill_mask;
				out.push_back(d);
			}
		}
	}
	water_levels.swap(stored); // the stored levels back
	return out;
}

} // namespace godot
