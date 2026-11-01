// MCGrid: debug views for looking inside the terrain and the water.
//
//   corners   the hover shows the corners around it: a thick white wire cube (edges only, drawn
//             through terrain) on every active corner in the 3 x 3 x 3 block round the hovered
//             corner, or, on a cell hover (Ctrl / Cmd), on the cell's active corners among its 8 —
//             the ones that pick its tile. Nothing is drawn over the rest of the terrain. (The
//             per-corner nodes from mc_grid_debug_corners.cpp are only picking colliders.)
//   wireframe the triangles of every chunk mesh drawn as dark lines over the shaded terrain
//             (an overlay shader with render_mode wireframe), so tile seams and tile shapes show.
//   water     an overlay above the water, drawn on top of everything:
//               - a square per wet column: blue = held one layer deep, green = stream, navy = sea
//               - a white arrow along the flow (toward the nearest fall)
//               - a line along the top of each fall: green when it is cut into the land (framed),
//                 red when it pours off an open edge
//               - the last pour (fill_water): a dot per column it reached, coloured by the order the
//                 flood fill reached it (yellow first -> magenta last); replay_water_debug() replays
//                 it, the dots appearing in that order over two seconds
#include "marching_cubes/grid/mc_grid.h"
#include "marching_cubes/util/mc_wire.h"
#include <godot_cpp/classes/array_mesh.hpp>
#include <godot_cpp/classes/geometry_instance3d.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/property_tweener.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/shader_material.hpp>
#include <godot_cpp/classes/sphere_mesh.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/tween.hpp>
#include <godot_cpp/variant/packed_color_array.hpp>
#include <godot_cpp/variant/packed_vector2_array.hpp>
#include <godot_cpp/variant/packed_vector3_array.hpp>
#include <algorithm>
#include <cmath>

namespace godot {

namespace {
const Color LOW_CORNER(0.25f, 0.78f, 0.68f);
const Color HIGH_CORNER(1.0f, 0.62f, 0.25f);
const Color EMPTY_CORNER(0.55f, 0.75f, 1.0f, 0.45f);
const Color HELD(0.25f, 0.55f, 1.0f, 0.55f);
const Color STREAM_COLOR(0.35f, 1.0f, 0.55f, 0.55f);
const Color SEA(0.1f, 0.2f, 0.55f, 0.35f);
const Color ARROW(1.0f, 1.0f, 1.0f, 0.9f);
const Color FRAMED(0.3f, 1.0f, 0.4f, 1.0f);
const Color OPEN_EDGE(1.0f, 0.3f, 0.25f, 1.0f);
const Color FIRST(1.0f, 0.9f, 0.2f, 1.0f);
const Color LAST(1.0f, 0.2f, 0.85f, 1.0f);
const float LIFT = 0.04f; // m the water overlay floats above the surface
const float REPLAY_SECONDS = 2.0f;

// The terrain wireframe overlay: dark, slightly see-through lines.
const char *WIREFRAME_SHADER = R"(shader_type spatial;
render_mode wireframe, unshaded, cull_disabled, depth_draw_never;
void fragment() {
	ALBEDO = vec3(0.08, 0.1, 0.12);
	ALPHA = 0.55;
}
)";

// The water overlay: vertex colours, unshaded, drawn over everything; UV.x is the order a pour
// reached the vertex (0..1, or -1 for always shown); `reveal` hides what was reached later.
const char *WATER_DEBUG_SHADER = R"(shader_type spatial;
render_mode unshaded, cull_disabled, depth_test_disabled, depth_draw_never;
uniform float reveal = 1.0;
void fragment() {
	if (UV.x > reveal) {
		discard;
	}
	ALBEDO = COLOR.rgb;
	ALPHA = COLOR.a;
}
)";

Ref<ShaderMaterial> material_from_code(const char *p_code) {
	Ref<Shader> shader;
	shader.instantiate();
	shader->set_code(p_code);
	Ref<ShaderMaterial> material;
	material.instantiate();
	material->set_shader(shader);
	return material;
}

// The water overlay's geometry, built up piece by piece.
struct DebugLines {
	PackedVector3Array tri_vertices, line_vertices;
	PackedColorArray tri_colors, line_colors;
	PackedVector2Array tri_uvs, line_uvs;

	void square(const Vector3 &p_centre, float p_half, const Color &p_color, float p_order = -1.0f) {
		const Vector3 c[4] = { p_centre + Vector3(-p_half, 0, -p_half), p_centre + Vector3(p_half, 0, -p_half),
			p_centre + Vector3(p_half, 0, p_half), p_centre + Vector3(-p_half, 0, p_half) };
		for (int i : { 0, 1, 2, 0, 2, 3 }) {
			tri_vertices.push_back(c[i]);
			tri_colors.push_back(p_color);
			tri_uvs.push_back(Vector2(p_order, 0));
		}
	}

	void line(const Vector3 &p_a, const Vector3 &p_b, const Color &p_color) {
		line_vertices.push_back(p_a);
		line_vertices.push_back(p_b);
		line_colors.push_back(p_color);
		line_colors.push_back(p_color);
		line_uvs.push_back(Vector2(-1, 0));
		line_uvs.push_back(Vector2(-1, 0));
	}

	Ref<ArrayMesh> commit() const {
		Ref<ArrayMesh> mesh;
		mesh.instantiate();
		Array arrays;
		arrays.resize(Mesh::ARRAY_MAX);
		if (!tri_vertices.is_empty()) {
			arrays[Mesh::ARRAY_VERTEX] = tri_vertices;
			arrays[Mesh::ARRAY_COLOR] = tri_colors;
			arrays[Mesh::ARRAY_TEX_UV] = tri_uvs;
			mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, arrays);
		}
		if (!line_vertices.is_empty()) {
			arrays[Mesh::ARRAY_VERTEX] = line_vertices;
			arrays[Mesh::ARRAY_COLOR] = line_colors;
			arrays[Mesh::ARRAY_TEX_UV] = line_uvs;
			mesh->add_surface_from_arrays(Mesh::PRIMITIVE_LINES, arrays);
		}
		return mesh;
	}
};
} // namespace

// --- corners ---

// Corner nodes are only picking colliders now: they draw nothing. (The Corners view is the hover's
// 3 x 3 x 3 neighbourhood, below.)
void MCGrid::_style_debug_corner(
		MeshInstance3D *p_node,
		bool p_solid,
		int p_y
) {
	(void)p_solid;
	(void)p_y;
	p_node->set_mesh(Ref<Mesh>());
}

// The thick white wire cube drawn on an active corner near the hover (edges only, through terrain).
void MCGrid::_ensure_corner_cube_look() {
	if (_debug_dot_solid.is_valid()) {
		return;
	}
	Ref<ArrayMesh> cube = mc_wire_cube(Vector3(1, 1, 1), 0.03f);
	cube->surface_remove(1); // no faces: see-through
	_debug_dot_solid = cube;
	Ref<StandardMaterial3D> material;
	material.instantiate();
	material->set_albedo(Color(1, 1, 1, 0.95f));
	material->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
	material->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
	material->set_cull_mode(BaseMaterial3D::CULL_DISABLED);
	material->set_flag(BaseMaterial3D::FLAG_DISABLE_DEPTH_TEST, true);
	material->set_render_priority(1);
	_debug_corner_materials.assign(1, material);
}

// Show a wire cube on every active corner around the hover: the 3 x 3 x 3 corners round the hovered
// corner, or (cell hover, p_cell) the cell's 8 corners — the ones that pick its tile. Grid space.
void MCGrid::_show_hover_corners(
		const Vector3 &p_pos,
		bool p_cell
) {
	_ensure_corner_cube_look();
	if (!hover_corners_root) {
		hover_corners_root = memnew(Node3D);
		hover_corners_root->set_name("HoverCorners");
		add_child(hover_corners_root);
		for (MeshInstance3D *&cube : hover_corner_cubes) {
			cube = memnew(MeshInstance3D);
			cube->set_mesh(_debug_dot_solid);
			cube->set_material_override(_debug_corner_materials[0]);
			cube->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
			hover_corners_root->add_child(cube);
		}
	}
	hover_corners_root->show();
	int used = 0;
	auto show_if_active = [&](const Vector3i &p_corner) {
		if (used < 27 && _dims().contains_corner(p_corner) && is_corner_active(p_corner)) {
			hover_corner_cubes[used]->set_position(Vector3(p_corner));
			hover_corner_cubes[used]->show();
			used++;
		}
	};
	if (p_cell) {
		Vector3i base(static_cast<int>(Math::floor(p_pos.x)), static_cast<int>(Math::floor(p_pos.y)),
				static_cast<int>(Math::floor(p_pos.z)));
		for (int i = 0; i < 8; i++) {
			show_if_active(base + Vector3i(i & 1, (i >> 1) & 1, (i >> 2) & 1));
		}
	} else {
		Vector3i centre(static_cast<int>(Math::round(p_pos.x)), static_cast<int>(Math::round(p_pos.y)),
				static_cast<int>(Math::round(p_pos.z)));
		for (int dy = -1; dy <= 1; dy++) {
			for (int dz = -1; dz <= 1; dz++) {
				for (int dx = -1; dx <= 1; dx++) {
					show_if_active(centre + Vector3i(dx, dy, dz));
				}
			}
		}
	}
	for (int i = used; i < 27; i++) {
		hover_corner_cubes[i]->hide();
	}
}

// --- wireframe ---

void MCGrid::set_debug_wireframe(bool p_on) {
	debug_wireframe = p_on;
	for (Chunk &chunk : chunks) {
		if (chunk.mesh_node) {
			_apply_wireframe(chunk.mesh_node);
		}
	}
}

// Put the wireframe overlay on a chunk mesh, or take it off (debug_wireframe).
void MCGrid::_apply_wireframe(MeshInstance3D *p_node) {
	if (_wireframe_material.is_null()) {
		_wireframe_material = material_from_code(WIREFRAME_SHADER);
	}
	p_node->set_material_overlay(debug_wireframe ? Ref<Material>(_wireframe_material) : Ref<Material>());
}

// --- water ---

void MCGrid::set_debug_water(bool p_on) {
	debug_water = p_on;
	if (!p_on && water_debug_node) {
		water_debug_node->hide();
	}
	_rebuild_water();
}

// Replay the last pour: its dots appear in the order the flood fill reached them.
void MCGrid::replay_water_debug() {
	if (!debug_water) {
		set_debug_water(true);
	}
	if (water_debug_material.is_null()) {
		return;
	}
	water_debug_material->set_shader_parameter("reveal", 0.0f);
	Ref<Tween> tween = create_tween();
	tween->tween_property(water_debug_material.ptr(), "shader_parameter/reveal", 1.0f, REPLAY_SECONDS);
}

// Build the water overlay from what _rebuild_water worked out (surfaces, held, flow).
void MCGrid::_rebuild_water_debug(
		const std::vector<float> &p_surface,
		const std::vector<uint8_t> &p_held,
		const std::vector<Color> &p_flow
) {
	if (!water_debug_node) {
		water_debug_node = memnew(MeshInstance3D);
		water_debug_node->set_name("WaterDebug");
		water_debug_node->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
		add_child(water_debug_node);
		water_debug_material = material_from_code(WATER_DEBUG_SHADER);
		water_debug_material->set_render_priority(2);
	}
	Vector2i columns = _column_counts();
	auto inside = [&](const Vector2i &c) { return c.x >= 0 && c.y >= 0 && c.x < columns.x && c.y < columns.y; };
	DebugLines debug;
	for (int z = 0; z < columns.y; z++) {
		for (int x = 0; x < columns.x; x++) {
			size_t i = static_cast<size_t>(_column_index(x, z));
			if (!_is_wet(static_cast<int>(i))) {
				continue;
			}
			float y = p_surface[i] + LIFT;
			Vector3 centre(static_cast<float>(x), y, static_cast<float>(z));
			bool sea = sea_level > 0 && water_levels[i] == sea_level && column_heights[i] < sea_level;
			debug.square(centre, 0.4f, sea ? SEA : (p_held[i] ? HELD : STREAM_COLOR));
			// the flow arrow: a shaft and two barbs
			const Color &flow = p_flow[i];
			if (flow.b > 0.5f) {
				Vector3 dir(flow.r, 0, flow.g);
				Vector3 side(-dir.z, 0, dir.x);
				Vector3 tip = centre + dir * 0.35f;
				debug.line(centre - dir * 0.25f, tip, ARROW);
				debug.line(tip, tip - dir * 0.15f + side * 0.1f, ARROW);
				debug.line(tip, tip - dir * 0.15f - side * 0.1f, ARROW);
			}
			// the top edge of each fall from this column: framed (green) or open (red)
			int level = water_levels[i];
			for (const Vector2i &step : { Vector2i(1, 0), Vector2i(-1, 0), Vector2i(0, 1), Vector2i(0, -1) }) {
				Vector2i n(x + step.x, z + step.y);
				if (!inside(n)) {
					continue;
				}
				size_t j = static_cast<size_t>(_column_index(n.x, n.y));
				if (!_is_wet(static_cast<int>(j)) || p_surface[j] > p_surface[i] - 0.02f) {
					continue;
				}
				Vector3 out(static_cast<float>(step.x), 0, static_cast<float>(step.y));
				Vector3 across(std::abs(out.z), 0, std::abs(out.x));
				Vector3 mid = centre + out * 0.5f;
				bool framed = water_levels[j] < level && _fall_framed(Vector2i(x, z), step, level);
				debug.line(mid - across * 0.45f, mid + across * 0.45f, framed ? FRAMED : OPEN_EDGE);
			}
		}
	}
	// the last pour, a dot per column in the order it was reached
	size_t count = last_pour.size();
	for (size_t k = 0; k < count; k++) {
		size_t i = static_cast<size_t>(last_pour[k]);
		if (i >= p_surface.size() || !_is_wet(static_cast<int>(i))) {
			continue;
		}
		float order = count > 1 ? static_cast<float>(k) / static_cast<float>(count - 1) : 0.0f;
		Vector2i columns_xy = _column_counts();
		Vector3 centre(static_cast<float>(static_cast<int>(i) % columns_xy.x), p_surface[i] + LIFT * 2.0f,
				static_cast<float>(static_cast<int>(i) / columns_xy.x));
		debug.square(centre, 0.12f, FIRST.lerp(LAST, order), order * 0.999f);
	}
	Ref<ArrayMesh> mesh = debug.commit();
	for (int s = 0; s < mesh->get_surface_count(); s++) {
		mesh->surface_set_material(s, water_debug_material);
	}
	water_debug_node->set_mesh(mesh);
	water_debug_node->show();
}

} // namespace godot
