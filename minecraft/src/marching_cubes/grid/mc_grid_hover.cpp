// MCGrid: the three hover quads shown on the corner under the mouse.
#include "marching_cubes/grid/mc_grid.h"
#include <godot_cpp/classes/box_mesh.hpp>
#include <godot_cpp/classes/box_shape3d.hpp>
#include <godot_cpp/classes/camera3d.hpp>
#include <godot_cpp/classes/collision_shape3d.hpp>
#include <godot_cpp/classes/label3d.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/quad_mesh.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/static_body3d.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

// Create the three hover quads and their materials (hidden until used).
void MCGrid::_initialize_hover_previews() {
	if (hover_root) {
		return;
	}

	hover_root = memnew(Node3D);
	hover_root->set_name("HoverPreview");
	add_child(hover_root);

	hover_mat_yellow.instantiate();
	hover_mat_yellow->set_albedo(Color(1, 1, 0, 0.4));
	hover_mat_yellow->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
	hover_mat_yellow->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);

	hover_mat_white.instantiate();
	hover_mat_white->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
	hover_mat_white->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);

	Ref<QuadMesh> quad_mesh;
	quad_mesh.instantiate();
	quad_mesh->set_size(Vector2(1.01, 1.01));

	for (int i = 0; i < 3; ++i) {
		hover_quads[i] = memnew(MeshInstance3D);
		hover_quads[i]->set_mesh(quad_mesh);
		hover_root->add_child(hover_quads[i]);
	}
	hover_root->hide();
}

// Show the quad facing the camera on the hovered corner (grid space).
void MCGrid::update_hover_preview(
		const Vector3 &p_corner_pos,
		const Vector3 &p_hit_normal,
		Camera3D *p_camera
) {
	if (!p_camera) {
		return;
	}

	if (!hover_root) {
		_initialize_hover_previews();
	}

	// p_corner_pos is in MCGrid space, so bring the camera into it too.
	Vector3 cam_pos = to_local(p_camera->get_global_position());
	Vector3 dir_to_cam = cam_pos - p_corner_pos;

	// Visible normals (3 faces) based on camera position relative to corner
	Vector3 normals[3] = { Vector3(dir_to_cam.x >= 0 ? 1.0f : -1.0f, 0.0f, 0.0f),
						   Vector3(0.0f, dir_to_cam.y >= 0 ? 1.0f : -1.0f, 0.0f),
						   Vector3(0.0f, 0.0f, dir_to_cam.z >= 0 ? 1.0f : -1.0f) };

	hover_root->set_position(p_corner_pos);
	hover_root->show();

	for (int i = 0; i < 3; ++i) {
		MeshInstance3D *quad = hover_quads[i];
		Vector3 n = normals[i];

		quad->set_position(n * 0.505f);

		if (n.x != 0.0f) {
			quad->set_rotation_degrees(Vector3(0.0f, n.x > 0.0f ? 90.0f : -90.0f, 0.0f));
		} else if (n.y != 0.0f) {
			quad->set_rotation_degrees(Vector3(n.y > 0.0f ? -90.0f : 90.0f, 0.0f, 0.0f));
		} else {
			quad->set_rotation_degrees(Vector3(0.0f, n.z > 0.0f ? 0.0f : 180.0f, 0.0f));
		}

		if (n.is_equal_approx(p_hit_normal)) {
			quad->set_material_override(hover_mat_yellow);
		} else {
			quad->set_material_override(hover_mat_white);
		}
	}
}

// Hide all hover quads.
void MCGrid::hide_hover_preview() {
	if (hover_root) {
		hover_root->hide();
	}
}

} // namespace godot
