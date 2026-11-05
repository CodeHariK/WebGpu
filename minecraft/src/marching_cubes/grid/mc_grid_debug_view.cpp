// MCGrid: debug views for looking inside the terrain and the water.
//
//   corners   see inspect mode (mc_grid_inspect.cpp): pull a tile out and its corners are shown.
//   edge corners  every inactive corner joined by an edge (one of its 6 neighbours) to an active
//             corner: a light blue dot, and a thin line along each such edge to the active corner —
//             the edges the surface crosses. One MultiMesh for the dots, one line mesh.
//   wireframe the triangles of every chunk mesh drawn as dark lines over the shaded terrain
//             (an overlay shader with render_mode wireframe), so tile seams and tile shapes show.
//   water     an overlay above the water, drawn on top of everything:
//               - water_sampled: a dot per wet sample of the sample grid, coloured by height
//               - a square per wet column: blue = held one layer deep, green = stream, navy = sea
//               - a white arrow along the flow (toward the nearest fall)
//               - a line along the top of each fall: green when it is cut into the land (framed),
//                 red when it pours off an open edge
//               - the last pour (fill_water): a dot per column it reached, coloured by the order the
//                 flood fill reached it (yellow first -> magenta last); replay_water_debug() replays
//                 it, the dots appearing in that order over two seconds
//               - or one step of that pour (set_pour_step; editor keys . , /): done columns yellow,
//                 the queue cyan, the column the step takes outlined white, a line to each neighbour
//                 (green queued, grey already queued, red + cross higher ground) and a label
//                 explaining the step
#include "marching_cubes/grid/mc_grid.h"
#include "marching_cubes/util/mc_wire.h"
#include <godot_cpp/classes/array_mesh.hpp>
#include <godot_cpp/classes/geometry_instance3d.hpp>
#include <godot_cpp/classes/label3d.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/multi_mesh.hpp>
#include <godot_cpp/classes/multi_mesh_instance3d.hpp>
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
const Color SPILL_POINT(1.0f, 0.2f, 0.9f, 1.0f); // the circle spill: its point and circle
const float TURN = 6.2831853f; // a full turn in radians
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

} // namespace

// The water overlay's geometry, built up piece by piece.
struct MCDebugMesh {
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

// --- corners ---

// Corner nodes are only picking colliders: they draw nothing. (To look at corners, use inspect mode:
// mc_grid_inspect.cpp.)
void MCGrid::_style_debug_corner(
		MeshInstance3D *p_node,
		bool p_solid,
		int p_y
) {
	(void)p_solid;
	(void)p_y;
	p_node->set_mesh(Ref<Mesh>());
}

// --- edge corners ---

void MCGrid::set_debug_edge_corners(bool p_on) {
	debug_edge_corners = p_on;
	_rebuild_edge_corners();
}

// Rebuild the edge-corner view (or hide it when off). Returns how many corners it shows.
int MCGrid::_rebuild_edge_corners() {
	if (!debug_edge_corners) {
		if (edge_corners_root) {
			edge_corners_root->hide();
		}
		return 0;
	}
	if (!edge_corners_root) {
		edge_corners_root = memnew(Node3D);
		edge_corners_root->set_name("EdgeCorners");
		add_child(edge_corners_root);
		edge_corners_dots = memnew(MultiMeshInstance3D);
		edge_corners_dots->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
		edge_corners_root->add_child(edge_corners_dots);
		edge_corners_lines = memnew(MeshInstance3D);
		edge_corners_lines->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
		edge_corners_root->add_child(edge_corners_lines);
	}
	static const Vector3i NEIGHBOURS[6] = { Vector3i(1, 0, 0), Vector3i(-1, 0, 0), Vector3i(0, 1, 0),
		Vector3i(0, -1, 0), Vector3i(0, 0, 1), Vector3i(0, 0, -1) };
	Vector3i size = _dims().max_corner() + Vector3i(1, 1, 1);
	std::vector<Vector3> dots;
	PackedVector3Array lines;
	for (int y = 0; y < size.y; y++) {
		for (int z = 0; z < size.z; z++) {
			for (int x = 0; x < size.x; x++) {
				Vector3i corner(x, y, z);
				if (is_corner_active(corner)) {
					continue;
				}
				bool edge = false;
				for (const Vector3i &step : NEIGHBOURS) {
					Vector3i n = corner + step;
					if (_dims().contains_corner(n) && is_corner_active(n)) {
						edge = true;
						lines.push_back(Vector3(corner));
						lines.push_back(Vector3(n));
					}
				}
				if (edge) {
					dots.push_back(Vector3(corner));
				}
			}
		}
	}
	// the dots: one sphere mesh, one instance each
	Ref<SphereMesh> sphere;
	sphere.instantiate();
	sphere->set_radius(0.06f);
	sphere->set_height(0.12f);
	sphere->set_radial_segments(10);
	sphere->set_rings(5);
	Ref<StandardMaterial3D> dot_material;
	dot_material.instantiate();
	dot_material->set_albedo(Color(0.45f, 0.8f, 1.0f));
	dot_material->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
	Ref<StandardMaterial3D> dot_xray;
	dot_xray.instantiate();
	dot_xray->set_albedo(Color(0.45f, 0.8f, 1.0f, 0.3f));
	dot_xray->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
	dot_xray->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
	dot_xray->set_flag(BaseMaterial3D::FLAG_DISABLE_DEPTH_TEST, true);
	dot_material->set_next_pass(dot_xray);
	sphere->set_material(dot_material);
	Ref<MultiMesh> multi;
	multi.instantiate();
	multi->set_transform_format(MultiMesh::TRANSFORM_3D);
	multi->set_mesh(sphere);
	multi->set_instance_count(static_cast<int>(dots.size()));
	for (size_t i = 0; i < dots.size(); i++) {
		multi->set_instance_transform(static_cast<int>(i), Transform3D(Basis(), dots[i]));
	}
	edge_corners_dots->set_multimesh(multi);
	// the edges to the active corners
	if (lines.is_empty()) {
		edge_corners_lines->set_mesh(Ref<Mesh>());
	} else {
		Array arrays;
		arrays.resize(Mesh::ARRAY_MAX);
		arrays[Mesh::ARRAY_VERTEX] = lines;
		Ref<ArrayMesh> mesh;
		mesh.instantiate();
		mesh->add_surface_from_arrays(Mesh::PRIMITIVE_LINES, arrays);
		edge_corners_lines->set_mesh(mesh);
		edge_corners_lines->set_material_override(mc_wire_material(Color(1.0f, 1.0f, 1.0f, 0.55f)));
	}
	edge_corners_root->show();
	return static_cast<int>(dots.size());
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
	MCDebugMesh debug;
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
	// the circle spill: a dot at each spill point and its circle, at the water's surface
	for (const SpillPoint &spill : water_spill_points) {
		float y = p_surface[static_cast<size_t>(spill.from)] + LIFT * 3.0f;
		Vector3 at(spill.at.x, y, spill.at.y);
		debug.square(at, 0.06f, SPILL_POINT);
		const int SEGMENTS = 32;
		for (int k = 0; k < SEGMENTS; k++) {
			float a0 = TURN * static_cast<float>(k) / SEGMENTS;
			float a1 = TURN * static_cast<float>(k + 1) / SEGMENTS;
			debug.line(at + Vector3(std::cos(a0), 0, std::sin(a0)) * water_spill_radius,
					at + Vector3(std::cos(a1), 0, std::sin(a1)) * water_spill_radius, SPILL_POINT);
		}
	}
	// the sample grid (water_sampled): a dot per wet sample, coloured by its height
	if (!water_debug_samples.empty()) {
		float lo = 1e6f;
		float hi = -1e6f;
		for (const Vector3 &s : water_debug_samples) {
			lo = std::min(lo, s.y);
			hi = std::max(hi, s.y);
		}
		for (const Vector3 &s : water_debug_samples) {
			float t = hi > lo ? (s.y - lo) / (hi - lo) : 0.0f;
			Color c = Color(0.2f, 0.4f, 1.0f).lerp(Color(1.0f, 0.4f, 0.2f), t);
			debug.square(s + Vector3(0, LIFT * 1.5f, 0), 0.012f, c);
		}
	}
	// the last pour: one step of it (step-by-step debug), or a dot per column in the order it was reached
	if (pour_step >= 0) {
		_add_pour_step_debug(debug, p_surface);
	} else if (pour_step_label) {
		pour_step_label->hide();
	}
	size_t count = pour_step >= 0 ? 0 : last_pour.size();
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

// --- the pour, step by step ---

namespace {
const Color DONE_DOT(1.0f, 0.85f, 0.3f, 0.9f); // taken off the queue already
const Color QUEUE_DOT(0.3f, 0.9f, 1.0f, 1.0f); // waiting in the queue
const Color CURRENT(1.0f, 1.0f, 1.0f, 1.0f); // the column this step takes
const Color RESULT_COLOR[4] = {
	Color(0.3f, 1.0f, 0.4f, 1.0f), // QUEUED
	Color(0.6f, 0.6f, 0.6f, 1.0f), // SEEN
	Color(1.0f, 0.3f, 0.25f, 1.0f), // HIGHER
	Color(0.6f, 0.6f, 0.6f, 0.0f), // OUTSIDE (not drawn)
};
const char *RESULT_TEXT[4] = { "queued", "already queued", "higher ground, rejected", "off the grid" };
const char *SIDE_TEXT[4] = { "+x", "-x", "+z", "-z" };
const Vector2i SIDES[4] = { Vector2i(1, 0), Vector2i(-1, 0), Vector2i(0, 1), Vector2i(0, -1) };
} // namespace

void MCGrid::set_pour_step(int p_step) {
	int count = get_pour_step_count();
	pour_step = count == 0 ? -1 : std::clamp(p_step, -1, count - 1);
	if (!debug_water) {
		set_debug_water(true); // rebuilds
	} else {
		_rebuild_water();
	}
}

String MCGrid::get_pour_step_text() const {
	if (pour_step < 0 || pour_step >= get_pour_step_count()) {
		return String();
	}
	const MCPourStep &step = last_pour_trace[static_cast<size_t>(pour_step)];
	Vector2i columns = _column_counts();
	Vector2i c(step.column % columns.x, step.column / columns.x);
	int ground = column_heights[static_cast<size_t>(step.column)];
	String text = vformat("step %d / %d: take (%d, %d) off the queue, ground %d", pour_step + 1,
			get_pour_step_count(), c.x, c.y, ground);
	for (int k = 0; k < 4; k++) {
		Vector2i n = c + SIDES[k];
		if (step.result[k] == MCPourStep::OUTSIDE) {
			text += vformat("\n%s: off the grid", SIDE_TEXT[k]);
			continue;
		}
		int other = column_heights[static_cast<size_t>(_column_index(n.x, n.y))];
		text += vformat("\n%s (%d, %d) ground %d: %s", SIDE_TEXT[k], n.x, n.y, other, RESULT_TEXT[step.result[k]]);
	}
	int waiting = step.queue_size - pour_step - 1;
	text += vformat("\nqueue: %d waiting", waiting);
	return text;
}

// Draw the pour as it stood at pour_step: the columns already done (yellow dots), the queue (cyan
// dots), the column this step takes (white square) with a line to each neighbour coloured by what
// happened to it (green queued, grey already queued, red higher ground), and a label explaining it.
void MCGrid::_add_pour_step_debug(
		MCDebugMesh &r_debug,
		const std::vector<float> &p_surface
) {
	if (pour_step < 0 || pour_step >= get_pour_step_count()) {
		return;
	}
	Vector2i columns = _column_counts();
	// where to draw over a column: its water, or its ground if dry
	auto top_of = [&](int p_index) {
		size_t i = static_cast<size_t>(p_index);
		float y = _is_wet(p_index) ? p_surface[i] : static_cast<float>(column_heights[i]) - 0.5f;
		return Vector3(static_cast<float>(p_index % columns.x), y + LIFT * 3.0f, static_cast<float>(p_index / columns.x));
	};
	const MCPourStep &step = last_pour_trace[static_cast<size_t>(pour_step)];
	// done: the columns taken before this step; queue: found but not taken yet
	for (int k = 0; k < pour_step; k++) {
		r_debug.square(top_of(last_pour[static_cast<size_t>(k)]), 0.1f, DONE_DOT);
	}
	for (int k = pour_step + 1; k < step.queue_size && k < static_cast<int>(last_pour.size()); k++) {
		r_debug.square(top_of(last_pour[static_cast<size_t>(k)]), 0.16f, QUEUE_DOT);
	}
	// the column this step takes, outlined, and its four neighbours
	Vector3 centre = top_of(step.column);
	const Vector3 ring[4] = { Vector3(-0.45f, 0, -0.45f), Vector3(0.45f, 0, -0.45f), Vector3(0.45f, 0, 0.45f),
		Vector3(-0.45f, 0, 0.45f) };
	for (int k = 0; k < 4; k++) {
		r_debug.line(centre + ring[k], centre + ring[(k + 1) % 4], CURRENT);
	}
	Vector2i c(step.column % columns.x, step.column / columns.x);
	for (int k = 0; k < 4; k++) {
		if (step.result[k] == MCPourStep::OUTSIDE) {
			continue;
		}
		Vector2i n = c + SIDES[k];
		Vector3 to = top_of(_column_index(n.x, n.y));
		const Color &color = RESULT_COLOR[step.result[k]];
		r_debug.line(centre, to, color);
		if (step.result[k] == MCPourStep::HIGHER) { // a cross on the rejected column
			r_debug.line(to + Vector3(-0.2f, 0, -0.2f), to + Vector3(0.2f, 0, 0.2f), color);
			r_debug.line(to + Vector3(-0.2f, 0, 0.2f), to + Vector3(0.2f, 0, -0.2f), color);
		} else if (step.result[k] == MCPourStep::QUEUED) { // a square on the newly queued column
			r_debug.square(to, 0.16f, color);
		}
	}
	// the explanation, floating over the column
	if (!pour_step_label) {
		pour_step_label = memnew(Label3D);
		pour_step_label->set_billboard_mode(BaseMaterial3D::BILLBOARD_ENABLED);
		pour_step_label->set_draw_flag(Label3D::FLAG_DISABLE_DEPTH_TEST, true);
		pour_step_label->set_pixel_size(0.006f);
		pour_step_label->set_font_size(40);
		pour_step_label->set_outline_size(12);
		pour_step_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_LEFT);
		pour_step_label->set_vertical_alignment(VERTICAL_ALIGNMENT_BOTTOM);
		water_debug_node->add_child(pour_step_label);
	}
	pour_step_label->set_text(get_pour_step_text());
	pour_step_label->set_position(centre + Vector3(0.3f, 0.6f, 0));
	pour_step_label->show();
}

} // namespace godot
