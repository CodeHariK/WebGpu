// MCGrid: debug views of the water mesh (Stats → Diagnostics → "Water view" cycles them):
//
//   normal         the water and waterfall shaders
//   wireframe      the water mesh's triangles drawn over it in white (surface + falls)
//   uv             both surfaces drawn with a UV checker instead: red / green = fract(UV), a dim
//                  checker every 0.25, white lines on whole UV units (surface: UV = world x, z;
//                  falls: x along the ground, y metres down)
//   uv + wireframe both
//
// Applied to the water node after every rebuild (_rebuild_water) and whenever the view changes.
#include "marching_cubes/grid/mc_grid.h"
#include <godot_cpp/classes/array_mesh.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/shader_material.hpp>
#include <algorithm>

namespace godot {

namespace {
const char *WATER_WIREFRAME_SHADER = R"(shader_type spatial;
render_mode wireframe, unshaded, cull_disabled, depth_draw_never;
void fragment() {
	ALBEDO = vec3(1.0);
	ALPHA = 0.85;
}
)";

const char *WATER_UV_SHADER = R"(shader_type spatial;
render_mode unshaded, cull_disabled;
void fragment() {
	vec2 f = fract(UV);
	vec2 cell = floor(UV * 4.0);
	float check = mod(cell.x + cell.y, 2.0);
	vec3 colour = vec3(f, 0.25) * (0.7 + 0.3 * check);
	vec2 to_line = abs(fract(UV - 0.5) - 0.5) / max(fwidth(UV), vec2(1e-4));
	float line = 1.0 - min(min(to_line.x, to_line.y), 1.0);
	ALBEDO = mix(colour, vec3(1.0), line);
}
)";

Ref<ShaderMaterial> water_view_material(const char *p_code) {
	Ref<Shader> shader;
	shader.instantiate();
	shader->set_code(p_code);
	Ref<ShaderMaterial> material;
	material.instantiate();
	material->set_shader(shader);
	return material;
}
} // namespace

void MCGrid::set_water_view(int p_view) {
	water_view = std::clamp(p_view, 0, static_cast<int>(WATER_VIEW_COUNT) - 1);
	_apply_water_view();
}

const char *MCGrid::water_view_name(int p_view) {
	switch (p_view) {
		case WATER_VIEW_WIREFRAME:
			return "wireframe";
		case WATER_VIEW_UV:
			return "UV";
		case WATER_VIEW_UV_WIREFRAME:
			return "UV + wireframe";
		default:
			return "normal";
	}
}

// Put the view's materials on the water node: the UV checker on both surfaces (or the water /
// waterfall materials back), the wireframe as an overlay (or none).
void MCGrid::_apply_water_view() {
	if (!water_node) {
		return;
	}
	Ref<ArrayMesh> mesh = water_node->get_mesh();
	bool uv = water_view == WATER_VIEW_UV || water_view == WATER_VIEW_UV_WIREFRAME;
	bool wire = water_view == WATER_VIEW_WIREFRAME || water_view == WATER_VIEW_UV_WIREFRAME;
	if (uv && water_uv_material.is_null()) {
		water_uv_material = water_view_material(WATER_UV_SHADER);
	}
	if (wire && water_wireframe_material.is_null()) {
		water_wireframe_material = water_view_material(WATER_WIREFRAME_SHADER);
	}
	if (mesh.is_valid()) {
		for (int s = 0; s < mesh->get_surface_count(); s++) {
			Ref<Material> normal = s == 0 ? water_material : waterfall_material;
			mesh->surface_set_material(s, uv ? Ref<Material>(water_uv_material) : normal);
		}
	}
	water_node->set_material_overlay(wire ? Ref<Material>(water_wireframe_material) : Ref<Material>());
}

} // namespace godot
