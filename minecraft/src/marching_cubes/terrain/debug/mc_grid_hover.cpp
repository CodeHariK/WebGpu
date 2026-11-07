// MCGrid: the hover preview on the corner under the mouse: a wire cube round the corner (the space
// that corner owns) and the face the mouse is on outlined and lightly filled, like an editor gizmo.
// (In inspect mode the editor hovers cells, Ctrl / Cmd style, to pick the tile to pull out.)
// Lines stay faintly visible behind terrain (mc_wire_material), so the cube never disappears.
#include "marching_cubes/terrain/mc_grid.h"
#include "marching_cubes/util/mc_wire.h"
#include <godot_cpp/classes/camera3d.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>

namespace godot {

namespace {
const Color CUBE_COLOR(1.0f, 1.0f, 1.0f, 0.85f);
const Color FACE_COLOR(1.0f, 0.86f, 0.25f, 1.0f);
const Color FILL_COLOR(1.0f, 0.86f, 0.25f, 0.18f);
} // namespace

// Create the wire cube and the face (hidden until used).
void MCGrid::_initialize_hover_previews() {
	if (hover_root) {
		return;
	}
	hover_root = memnew(Node3D);
	hover_root->set_name("HoverPreview");
	add_child(hover_root);

	hover_cube = memnew(MeshInstance3D);
	hover_cube->set_mesh(mc_wire_box(Vector3(1.02f, 1.02f, 1.02f)));
	hover_cube->set_material_override(mc_wire_material(CUBE_COLOR));
	hover_root->add_child(hover_cube);

	hover_face = memnew(MeshInstance3D);
	Ref<ArrayMesh> face = mc_wire_face(0.94f);
	face->surface_set_material(0, mc_wire_material(FACE_COLOR));
	face->surface_set_material(1, mc_wire_material(FILL_COLOR, false));
	hover_face->set_mesh(face);
	hover_root->add_child(hover_face);
	hover_root->hide();
}

// Show the wire cube on the hovered corner (grid space) and outline the face p_hit_normal points out
// of. The camera isn't needed any more (kept so callers don't change).
void MCGrid::update_hover_preview(
		const Vector3 &p_corner_pos,
		const Vector3 &p_hit_normal,
		Camera3D *p_camera
) {
	if (!hover_root) {
		_initialize_hover_previews();
	}
	hover_root->set_position(p_corner_pos);
	hover_root->show();

	// the face: on the side the normal points to, turned so its +Z faces out
	Vector3 n = p_hit_normal;
	Vector3 axis = Math::abs(n.x) >= Math::abs(n.y) && Math::abs(n.x) >= Math::abs(n.z)
			? Vector3(Math::sign(n.x), 0, 0)
			: (Math::abs(n.y) >= Math::abs(n.z) ? Vector3(0, Math::sign(n.y), 0) : Vector3(0, 0, Math::sign(n.z)));
	if (axis.is_zero_approx()) {
		axis = Vector3(0, 1, 0);
	}
	hover_face->set_position(axis * 0.51f);
	if (axis.x != 0.0f) {
		hover_face->set_rotation_degrees(Vector3(0.0f, axis.x > 0.0f ? 90.0f : -90.0f, 0.0f));
	} else if (axis.y != 0.0f) {
		hover_face->set_rotation_degrees(Vector3(axis.y > 0.0f ? -90.0f : 90.0f, 0.0f, 0.0f));
	} else {
		hover_face->set_rotation_degrees(Vector3(0.0f, axis.z > 0.0f ? 0.0f : 180.0f, 0.0f));
	}
}

// Hide the hover preview.
void MCGrid::hide_hover_preview() {
	if (hover_root) {
		hover_root->hide();
	}

}

} // namespace godot
