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
#include <godot_cpp/classes/standard_material3d.hpp>
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
		case WATER_VIEW_OUTLINE:
			return "outline";
		default:
			return "normal";
	}
}

// Put the view's materials on the water node: the UV checker on both surfaces (or the water /
// waterfall materials back), the wireframe as an overlay (or none).
void MCGrid::_apply_water_view() {
	_draw_water_outlines();
	if (!water_node) {
		return;
	}
	Ref<ArrayMesh> mesh = water_node->get_mesh();
	bool uv = water_view == WATER_VIEW_UV || water_view == WATER_VIEW_UV_WIREFRAME;
	bool wire = water_view == WATER_VIEW_WIREFRAME || water_view == WATER_VIEW_UV_WIREFRAME;
	// water debug stages: 1 .. 5 show how the shape is built without the mesh, 6 the mesh's triangles
	bool staged = debug_water && water_debug_stage > 0;
	water_node->set_visible(!staged || water_debug_stage >= 6);
	wire = wire || (staged && water_debug_stage >= 6);
	if (uv && water_uv_material.is_null()) {
		water_uv_material = water_view_material(WATER_UV_SHADER);
	}
	if (wire && water_wireframe_material.is_null()) {
		water_wireframe_material = water_view_material(WATER_WIREFRAME_SHADER);
	}
	if (mesh.is_valid()) {
		for (int s = 0; s < mesh->get_surface_count(); s++) {
			mesh->surface_set_material(s, uv ? Ref<Material>(water_uv_material) : water_material);
		}
	}
	water_node->set_material_overlay(wire ? Ref<Material>(water_wireframe_material) : Ref<Material>());
}

// The "outline" view: each outline as lines a little above the water (bank edges green, fall
// edges orange), a white tick where a run starts / ends or the edge kind changes.
void MCGrid::_draw_water_outlines() {
	if (water_view != WATER_VIEW_OUTLINE) {
		if (water_outline_node) {
			water_outline_node->hide();
		}
		return;
	}
	if (!water_outline_node) {
		water_outline_node = memnew(MeshInstance3D);
		water_outline_node->set_name("WaterOutlines");
		water_outline_node->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
		Ref<StandardMaterial3D> material;
		material.instantiate();
		material->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
		material->set_flag(BaseMaterial3D::FLAG_ALBEDO_FROM_VERTEX_COLOR, true);
		water_outline_node->set_material_override(material);
		add_child(water_outline_node);
	}
	PackedVector3Array lines;
	PackedColorArray colors;
	const Vector3 lift(0, 0.05f, 0);
	const Color bank(0.2f, 1.0f, 0.3f);
	const Color fall(1.0f, 0.55f, 0.1f);
	const Color point(1.0f, 1.0f, 1.0f);
	for (const WaterOutline &outline : water_outlines) {
		int count = static_cast<int>(outline.points.size());
		for (int i = 0; i < static_cast<int>(outline.kinds.size()); i++) {
			Color c = outline.kinds[static_cast<size_t>(i)] == WaterEdge::FALL ? fall : bank;
			lines.push_back(outline.points[static_cast<size_t>(i)] + lift);
			lines.push_back(outline.points[static_cast<size_t>((i + 1) % count)] + lift);
			colors.push_back(c);
			colors.push_back(c);
		}
		// the inset outline (the flat water's edge in outline mode) in cyan
		if (water_outline_fill && outline.closed) {
			WaterOutline inset = _inset_outline(outline, water_fall_lip);
			int n = static_cast<int>(inset.points.size());
			for (int i = 0; i < n; i++) {
				lines.push_back(inset.points[static_cast<size_t>(i)] + lift * 1.5f);
				lines.push_back(inset.points[static_cast<size_t>((i + 1) % n)] + lift * 1.5f);
				colors.push_back(Color(0.2f, 0.9f, 1.0f));
				colors.push_back(Color(0.2f, 0.9f, 1.0f));
			}
		}
		int edges = static_cast<int>(outline.kinds.size());
		for (int i = 0; i < count; i++) {
			bool end = !outline.closed && (i == 0 || i == count - 1);
			bool change = edges > 0 && outline.kinds[static_cast<size_t>((i - 1 + edges) % edges)] != outline.kinds[static_cast<size_t>(i % edges)];
			if (!end && !change) {
				continue;
			}
			const Vector3 &p = outline.points[static_cast<size_t>(i)];
			lines.push_back(p + lift);
			lines.push_back(p + lift + Vector3(0, 0.25f, 0));
			colors.push_back(point);
			colors.push_back(point);
		}
	}
	Ref<ArrayMesh> mesh;
	mesh.instantiate();
	if (!lines.is_empty()) {
		Array arrays;
		arrays.resize(Mesh::ARRAY_MAX);
		arrays[Mesh::ARRAY_VERTEX] = lines;
		arrays[Mesh::ARRAY_COLOR] = colors;
		mesh->add_surface_from_arrays(Mesh::PRIMITIVE_LINES, arrays);
	}
	water_outline_node->set_mesh(mesh);
	water_outline_node->show();
}

} // namespace godot
