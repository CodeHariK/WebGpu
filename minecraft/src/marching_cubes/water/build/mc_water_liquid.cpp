// MCGrid: the map's one liquid — water, lava or green poison.
//
// Each liquid is its own shader in project/material/shaders/terrain/liquid/ (water.gdshader,
// lava.gdshader, poison.gdshader; their shared steps in liquid_common.gdshaderinc), so a shader
// holds only its liquid's code: no branching on the liquid. They are the simple (mobile) look; their
// working copies, and a desktop look of each, are in the Water Shader Steps project
// (blender/sand_island/water_tutorial/: water_mobile/, water_desktop/, ...). The whole water mesh is
// one surface with that one material, so the map is one draw whatever the liquid. Changing the
// liquid only swaps the material; the mesh (shape, flow, depth) is the same for all of them.
// A material set with set_water_material is used instead. Not saved in the .mct yet: the liquid is
// the grid's property (the scene, or the Water tab's "Liquid" button).
#include "marching_cubes/terrain/mc_grid.h"
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/shader_material.hpp>
#include <algorithm>

namespace godot {

namespace {
// By Liquid.
const char *LIQUID_SHADERS[3] = {
	"res://material/shaders/terrain/liquid/water.gdshader",
	"res://material/shaders/terrain/liquid/lava.gdshader",
	"res://material/shaders/terrain/liquid/poison.gdshader",
};

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
} // namespace

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

// Called when the liquid or the override changes, and by the first water rebuild.
void MCGrid::_apply_liquid() {
	if (water_material_override.is_valid()) {
		water_material = water_material_override;
	} else {
		Ref<Material> &material = liquid_materials[liquid];
		if (material.is_null()) {
			material = shader_material(LIQUID_SHADERS[liquid]);
		}
		water_material = material;
	}
	_apply_water_view(); // onto the mesh (unless a debug view is showing)
}

} // namespace godot
