// MCGrid: debug views of the water mesh (Stats → Diagnostics → "Water view" cycles them):
//
//   normal         the water and waterfall shaders
//   wireframe      the water mesh's triangles drawn over it in white (surface + falls)
//   uv             both surfaces drawn with a UV checker instead: red / green = fract(UV), a dim
//                  checker every 0.25, white lines on whole UV units (surface: UV = world x, z;
//                  falls: x along the ground, y metres down)
//   uv + wireframe both
//   pieces         every mesh piece in its own colour: each body's flat tile and each fall strip.
//                  Drawn from an unwelded copy of the mesh (_water_pieces_mesh), so shared
//                  vertices don't blend.
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
	int previous = water_view;
	water_view = std::clamp(p_view, 0, static_cast<int>(WATER_VIEW_COUNT) - 1);
	// "pieces" uses its own mesh, made in the rebuild: rebuild when going in or out of it
	if ((previous == WATER_VIEW_PIECES) != (water_view == WATER_VIEW_PIECES) && !chunks.empty() &&
			water_levels.size() == column_heights.size()) {
		_queue_water();
	}
	_apply_water_view();
}

// The "pieces" view's mesh: every triangle with its own three vertices (nothing welded), coloured
// by the piece it belongs to (MCWaterArrays::pieces / first_pieces), flat-shaded so the
// triangles show. Hues step by the golden ratio, so neighbouring pieces differ.
Ref<ArrayMesh> MCGrid::_water_pieces_mesh(const MCWaterArrays &p_arrays) const {
	PackedVector3Array vertices;
	PackedVector3Array normals;
	PackedColorArray colors;
	int piece = 0;
	auto add = [&](const PackedInt32Array &p_indices, const std::vector<int64_t> &p_starts) {
		for (size_t k = 0; k < p_starts.size(); k++) {
			int64_t from = p_starts[k];
			int64_t to = k + 1 < p_starts.size() ? p_starts[k + 1] : p_indices.size();
			if (to - from < 3) {
				continue; // an empty piece (a body with no surface): no colour used
			}
			float hue = std::fmod(static_cast<float>(piece) * 0.618034f, 1.0f);
			float value = piece % 2 == 0 ? 0.95f : 0.7f;
			Color colour = Color::from_hsv(hue, 0.75f, value);
			piece++;
			for (int64_t t = from; t + 2 < to; t += 3) {
				const Vector3 &a = p_arrays.vertices[p_indices[t]];
				const Vector3 *b = &p_arrays.vertices[p_indices[t + 1]];
				const Vector3 *c = &p_arrays.vertices[p_indices[t + 2]];
				// the face normal, turned the way the builder's vertex normals point (up on the flat
				// water, out on a fall)
				Vector3 average = p_arrays.normals[p_indices[t]] + p_arrays.normals[p_indices[t + 1]] +
						p_arrays.normals[p_indices[t + 2]];
				Vector3 cross = (*b - a).cross(*c - a);
				Vector3 normal = cross.length_squared() > 1e-12f ? cross.normalized() : Vector3(0, 1, 0);
				if (normal.dot(average) < 0.0f) {
					normal = -normal;
				}
				// wound to face it (Godot's front faces are clockwise: (b - a) x (c - a) points away);
				// a triangle seen from its back would have its normal flipped and render dark
				if (cross.dot(normal) > 0.0f) {
					std::swap(b, c);
				}
				for (const Vector3 *v : { &a, b, c }) {
					vertices.push_back(*v);
					normals.push_back(normal);
					colors.push_back(colour);
				}
			}
		}
	};
	add(p_arrays.first_indices, p_arrays.first_pieces); // fall strips (tiles mode)
	add(p_arrays.indices, p_arrays.pieces); // flat tiles / fill regions / swept runs
	Ref<ArrayMesh> mesh;
	mesh.instantiate();
	if (vertices.is_empty()) {
		return mesh;
	}
	Array arrays;
	arrays.resize(Mesh::ARRAY_MAX);
	arrays[Mesh::ARRAY_VERTEX] = vertices;
	arrays[Mesh::ARRAY_NORMAL] = normals;
	arrays[Mesh::ARRAY_COLOR] = colors;
	mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, arrays);
	return mesh;
}

const char *MCGrid::water_view_name(int p_view) {
	switch (p_view) {
		case WATER_VIEW_WIREFRAME:
			return "wireframe";
		case WATER_VIEW_UV:
			return "UV";
		case WATER_VIEW_UV_WIREFRAME:
			return "UV + wireframe";
		case WATER_VIEW_PIECES:
			return "pieces";
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
	// water debug stages: 1 .. 3 show how the shape is built without the mesh, 4 the mesh's triangles
	bool staged = debug_water && water_debug_stage > 0;
	water_node->set_visible(!staged || water_debug_stage >= WATER_DEBUG_STAGES);
	wire = wire || (staged && water_debug_stage >= WATER_DEBUG_STAGES);
	if (uv && water_uv_material.is_null()) {
		water_uv_material = water_view_material(WATER_UV_SHADER);
	}
	if (wire && water_wireframe_material.is_null()) {
		water_wireframe_material = water_view_material(WATER_WIREFRAME_SHADER);
	}
	if (water_view == WATER_VIEW_PIECES) {
		// the pieces mesh (made by _rebuild_water in this view): vertex colours, both sides
		if (water_pieces_material.is_null()) {
			Ref<StandardMaterial3D> material;
			material.instantiate();
			material->set_flag(BaseMaterial3D::FLAG_ALBEDO_FROM_VERTEX_COLOR, true);
			material->set_cull_mode(BaseMaterial3D::CULL_DISABLED);
			material->set_roughness(1.0f);
			water_pieces_material = material;
		}
		if (mesh.is_valid()) {
			for (int s = 0; s < mesh->get_surface_count(); s++) {
				mesh->surface_set_material(s, water_pieces_material);
			}
		}
		water_node->set_material_overlay(wire ? Ref<Material>(water_wireframe_material) : Ref<Material>());
		return;
	}
	if (mesh.is_valid()) {
		for (int s = 0; s < mesh->get_surface_count(); s++) {
			mesh->surface_set_material(s, uv ? Ref<Material>(water_uv_material) : water_material);
		}
	}
	water_node->set_material_overlay(wire ? Ref<Material>(water_wireframe_material) : Ref<Material>());
}

} // namespace godot
