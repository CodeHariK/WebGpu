// MCGrid: debug views for looking inside the terrain (the water's: ../water/debug/).
//
//   corners   see inspect mode (mc_grid_inspect.cpp): pull a tile out and its corners are shown.
//   edge corners  every inactive corner joined by an edge (one of its 6 neighbours) to an active
//             corner: a light blue dot, and a thin line along each such edge to the active corner —
//             the edges the surface crosses. One MultiMesh for the dots, one line mesh.
//   wireframe the triangles of every chunk mesh drawn as dark lines over the shaded terrain
//             (an overlay shader with render_mode wireframe), so tile seams and tile shapes show.
#include "marching_cubes/grid/mc_grid.h"
#include "marching_cubes/util/mc_wire.h"
#include <godot_cpp/classes/array_mesh.hpp>
#include <godot_cpp/classes/geometry_instance3d.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/multi_mesh.hpp>
#include <godot_cpp/classes/multi_mesh_instance3d.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/shader_material.hpp>
#include <godot_cpp/classes/sphere_mesh.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/variant/packed_vector3_array.hpp>

namespace godot {

namespace {
// The terrain wireframe overlay: dark, slightly see-through lines.
const char *WIREFRAME_SHADER = R"(shader_type spatial;
render_mode wireframe, unshaded, cull_disabled, depth_draw_never;
void fragment() {
	ALBEDO = vec3(0.08, 0.1, 0.12);
	ALPHA = 0.55;
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

// Put the wireframe overlay on a chunk mesh, or take it off (debug_wireframe); the tile seams
// overlay instead while that is on.
void MCGrid::_apply_wireframe(MeshInstance3D *p_node) {
	if (_wireframe_material.is_null()) {
		_wireframe_material = material_from_code(WIREFRAME_SHADER);
	}
	if (debug_seams) { // the tile seams (mc_grid_debug_seams.cpp), the wireframe as its next pass
		p_node->set_material_overlay(_seams_overlay());
		return;
	}
	p_node->set_material_overlay(debug_wireframe ? Ref<Material>(_wireframe_material) : Ref<Material>());
}


} // namespace godot
