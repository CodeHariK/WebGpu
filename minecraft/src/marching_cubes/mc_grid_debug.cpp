#include "mc.h"
#include "mc_grid.h"
#include "mc_physics.h"
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

void MCGrid::set_show_debug_corners(bool p_show) {
	if (show_debug_corners == p_show) {
		return;
	}
	show_debug_corners = p_show;
	set_debug_corners_visible(show_debug_corners);
}

bool MCGrid::get_show_debug_corners() const {
	return show_debug_corners;
}

void MCGrid::set_debug_corners_visible(bool p_visible) {
	if (debug_corners_container) {
		debug_corners_container->set_visible(p_visible);
	}
}

bool MCGrid::is_debug_corners_visible() const {
	if (debug_corners_container) {
		return debug_corners_container->is_visible();
	}
	return false;
}

void MCGrid::set_corner_collision_enabled(bool p_enabled) {
	if (!debug_corners_container) {
		return;
	}

	uint32_t layer = p_enabled ? LAYER_CORNERS : 0;

	TypedArray<Node> children = debug_corners_container->get_children();
	for (int i = 0; i < children.size(); i++) {
		Node *child = Object::cast_to<Node>(children[i]);
		if (!child)
			continue;

		// Corners are MeshInstances with StaticBody3D children
		TypedArray<Node> sub_children = child->get_children();
		for (int j = 0; j < sub_children.size(); j++) {
			StaticBody3D *sb = Object::cast_to<StaticBody3D>(sub_children[j]);
			if (sb && (sb->get_collision_layer() == LAYER_CORNERS || sb->get_collision_layer() == 0)) {
				sb->set_collision_layer(layer);
			}
		}
	}
}

int MCGrid::_spawn_debug_cubes(const Chunk &p_chunk, const Ref<BoxMesh> &p_box_mesh) {
	int nx = p_chunk.size_x + 1;
	int ny = p_chunk.size_y + 1;
	int nz = p_chunk.size_z + 1;
	int count = 0;

	if (_debug_box_mesh.is_null()) {
		_debug_box_mesh.instantiate();
		_debug_box_mesh->set_size(Vector3(0.1, 0.1, 0.1));
	}

	if (_debug_mat_red.is_null()) {
		_debug_mat_red.instantiate();
		_debug_mat_red->set_albedo(Color(1, 0, 0));
		_debug_mat_red->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
	}

	if (_debug_mat_blue.is_null()) {
		_debug_mat_blue.instantiate();
		_debug_mat_blue->set_albedo(Color(0, 0, 1));
		_debug_mat_blue->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
	}

	for (int ly = 0; ly < ny; ly++) {
		for (int lz = 0; lz < nz; lz++) {
			for (int lx = 0; lx < nx; lx++) {
				bool state = p_chunk.get_corner(lx, ly, lz);
				bool has_active = p_chunk.has_active_neighbor(lx, ly, lz);
				bool has_inactive = p_chunk.has_inactive_neighbor(lx, ly, lz);

				if (!state && !has_active) {
					continue;
				}

				Vector3 world_pos = Vector3(
						static_cast<float>((p_chunk.loc_x * p_chunk.size_x) + lx),
						static_cast<float>((p_chunk.loc_y * p_chunk.size_y) + ly),
						static_cast<float>((p_chunk.loc_z * p_chunk.size_z) + lz));

				MeshInstance3D *mi = memnew(MeshInstance3D);
				mi->set_mesh(_debug_box_mesh);
				mi->set_position(world_pos);

				if (!debug_corners_container) {
					debug_corners_container = memnew(Node3D);
					debug_corners_container->set_name("DebugCorners");
					add_child(debug_corners_container);
				}
				debug_corners_container->add_child(mi);
				if (is_inside_tree()) {
					mi->set_owner(get_owner() ? get_owner() : this);
				}

				// Store for granular updates
				int corner_idx = (ly * nx * nz) + (lz * nx) + lx;
				const_cast<Chunk&>(p_chunk).debug_visuals[corner_idx] = mi;

				if (state) {
					mi->set_material_override(_debug_mat_red);

					if (has_inactive) {
						MCPhysics::create_static_box_collider(
								mi,
								LAYER_CORNERS,
								Vector3(1.0, 1.0, 1.0));
					}
				} else {
					mi->set_material_override(_debug_mat_blue);
				}

				count++;
				total_debug_corners++;
			}
		}
	}

	return count;
}

void MCGrid::_update_debug_at(int gx, int gy, int gz) {
	int cx = (gx >= grid_size.x * chunk_size.x) ? grid_size.x - 1 : gx / chunk_size.x;
	int cy = (gy >= grid_size.y * chunk_size.y) ? grid_size.y - 1 : gy / chunk_size.y;
	int cz = (gz >= grid_size.z * chunk_size.z) ? grid_size.z - 1 : gz / chunk_size.z;

	if (cx < 0 || cx >= grid_size.x || cy < 0 || cy >= grid_size.y || cz < 0 || cz >= grid_size.z) {
		return;
	}

	int lx = gx - (cx * chunk_size.x);
	int ly = gy - (cy * chunk_size.y);
	int lz = gz - (cz * chunk_size.z);

	Chunk &chunk = chunks[_get_chunk_index(cx, cy, cz)];
	int nx = chunk.size_x + 1;
	int nz = chunk.size_z + 1;
	int corner_idx = (ly * nx * nz) + (lz * nx) + lx;
	MeshInstance3D *mi = chunk.debug_visuals[corner_idx];
	bool state = chunk.get_corner(lx, ly, lz);
	bool has_active = chunk.has_active_neighbor(lx, ly, lz);
	bool has_inactive = chunk.has_inactive_neighbor(lx, ly, lz);

	if (!state && !has_active) {
		if (mi) {
			mi->queue_free();
			chunk.debug_visuals[corner_idx] = nullptr;
			total_debug_corners--;
		}
		return;
	}

	if (!mi) {
		// Spawn since it's now needed
		mi = memnew(MeshInstance3D);
		mi->set_mesh(_debug_box_mesh);
		mi->set_position(Vector3(gx, gy, gz));

		if (!debug_corners_container) {
			debug_corners_container = memnew(Node3D);
			debug_corners_container->set_name("DebugCorners");
			add_child(debug_corners_container);
		}
		debug_corners_container->add_child(mi);
		if (is_inside_tree()) {
			mi->set_owner(get_owner() ? get_owner() : this);
		}
		chunk.debug_visuals[corner_idx] = mi;
		total_debug_corners++;
	} else {
		// Remove old collisions if any (only for active corners with inactive neighbors)
		TypedArray<Node> col_children = mi->get_children();
		for (int i = 0; i < col_children.size(); i++) {
			Node *child = Object::cast_to<Node>(col_children[i]);
			if (Object::cast_to<StaticBody3D>(child)) {
				child->queue_free();
			}
		}
	}

	if (state) {
		mi->set_material_override(_debug_mat_red);
		if (has_inactive) {
			MCPhysics::create_static_box_collider(mi, LAYER_CORNERS, Vector3(1.0, 1.0, 1.0));
		}
	} else {
		mi->set_material_override(_debug_mat_blue);
	}
}

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

void MCGrid::update_hover_preview(const Vector3 &p_corner_pos, const Vector3 &p_hit_normal, Camera3D *p_camera) {
	if (!p_camera) {
		return;
	}

	if (!hover_root) {
		_initialize_hover_previews();
	}

	Vector3 cam_pos = p_camera->get_global_position();
	Vector3 dir_to_cam = cam_pos - p_corner_pos;

	// Visible normals (3 faces) based on camera position relative to corner
	Vector3 normals[3] = {
		Vector3(dir_to_cam.x >= 0 ? 1.0f : -1.0f, 0.0f, 0.0f),
		Vector3(0.0f, dir_to_cam.y >= 0 ? 1.0f : -1.0f, 0.0f),
		Vector3(0.0f, 0.0f, dir_to_cam.z >= 0 ? 1.0f : -1.0f)
	};

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

void MCGrid::hide_hover_preview() {
	if (hover_root) {
		hover_root->hide();
	}
}

} // namespace godot
