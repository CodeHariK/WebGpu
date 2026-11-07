// MCGrid: the map's one liquid — water, lava or green poison.
//
// The whole water mesh is one surface with one material (liquid.gdshader), so the map is one draw
// whatever the liquid. The liquid is a uniform of that material (`liquid`): the same for every
// pixel, so the shader's per-liquid branches cost nothing. Changing it only sets the uniform; the
// mesh (shape, flow, depth) is the same for every liquid. Not saved in the .mct yet: it is the
// grid's property (the scene, or the Water tab's "Liquid" button).
#include "marching_cubes/terrain/mc_grid.h"
#include <godot_cpp/classes/shader_material.hpp>
#include <algorithm>

namespace godot {

void MCGrid::set_liquid(int p_liquid) {
	liquid = std::clamp(p_liquid, 0, static_cast<int>(LIQUID_COUNT) - 1);
	_apply_liquid();
}

const char *MCGrid::liquid_name(int p_liquid) {
	switch (p_liquid) {
		case LIQUID_LAVA:
			return "lava";
		case LIQUID_POISON:
			return "green poison";
		default:
			return "water";
	}
}

// The water material is made on the first rebuild (or set from a script): called then too.
void MCGrid::_apply_liquid() {
	Ref<ShaderMaterial> material = water_material;
	if (material.is_valid()) {
		material->set_shader_parameter("liquid", liquid);
	}
}

} // namespace godot
