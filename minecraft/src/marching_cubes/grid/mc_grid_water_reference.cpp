// MCGrid: the builder's step 2 results per column, for water_from_scratch to check its own against.
//
// Runs the same passes _rebuild_water runs before it builds the mesh: the diagonal spills swapped in
// (_spilled_levels), held / stream and the surfaces (_water_surfaces), the flow (_water_flow); then
// the stored levels go back. Nothing is built or changed.
#include "marching_cubes/grid/mc_grid.h"
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

} // namespace godot
