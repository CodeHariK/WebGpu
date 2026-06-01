#include "marching_cubes/mc_physics.h" // Use MCPhysics helper
#include "mp.h"
#include "mp_grid.h"
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

int MPGrid::_spawn_debug_cubes(const MPChunk &p_chunk, const Ref<BoxMesh> &p_box_mesh) {
	int nx = p_chunk.size_x + 1;
	int ny = p_chunk.size_y + 1;
	int nz = p_chunk.size_z + 1;
	int count = 0;

	if (_debug_box_mesh.is_null()) {
		_debug_box_mesh.instantiate();
		_debug_box_mesh->set_size(Vector3(0.1, 0.1, 0.1));
	}

	// ... Material instancing remains the same ...

	for (int ly = 0; ly < ny; ly++) {
		for (int lz = 0; lz < nz; lz++) {
			for (int lx = 0; lx < nx; lx++) {
				bool state = p_chunk.get_corner(lx, ly, lz);
				bool has_active = p_chunk.has_active_neighbor(lx, ly, lz);
				bool has_inactive = p_chunk.has_inactive_neighbor(lx, ly, lz);

				if (!state && !has_active)
					continue;

				// --- THE PRISM STAGGER MATH ---
				int global_z = (p_chunk.loc_z * p_chunk.size_z) + lz;
				float stagger = (global_z % 2 != 0) ? 0.5f : 0.0f;

				Vector3 world_pos = Vector3(
						static_cast<float>((p_chunk.loc_x * p_chunk.size_x) + lx) + stagger,
						static_cast<float>((p_chunk.loc_y * p_chunk.size_y) + ly),
						static_cast<float>(global_z) * 0.866025f);

				MeshInstance3D *mi = memnew(MeshInstance3D);
				mi->set_mesh(_debug_box_mesh);
				mi->set_position(world_pos);

				if (!debug_corners_container) {
					debug_corners_container = memnew(Node3D);
					debug_corners_container->set_name("DebugCorners");
					add_child(debug_corners_container);
				}
				debug_corners_container->add_child(mi);
				if (is_inside_tree())
					mi->set_owner(get_owner() ? get_owner() : this);

				int corner_idx = (ly * nx * nz) + (lz * nx) + lx;
				const_cast<MPChunk &>(p_chunk).debug_visuals[corner_idx] = mi;

				if (state) {
					mi->set_material_override(_debug_mat_red);
					if (has_inactive) {
						MCPhysics::create_static_box_collider(mi, toLayer(LAYER_CORNERS), Vector3(1.0, 1.0, 1.0));
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

void MPGrid::_update_debug_at(int gx, int gy, int gz) {
	int cx = (gx >= grid_size.x * chunk_size.x) ? grid_size.x - 1 : gx / chunk_size.x;
	int cy = (gy >= grid_size.y * chunk_size.y) ? grid_size.y - 1 : gy / chunk_size.y;
	int cz = (gz >= grid_size.z * chunk_size.z) ? grid_size.z - 1 : gz / chunk_size.z;

	if (cx < 0 || cx >= grid_size.x || cy < 0 || cy >= grid_size.y || cz < 0 || cz >= grid_size.z)
		return;

	int lx = gx - (cx * chunk_size.x);
	int ly = gy - (cy * chunk_size.y);
	int lz = gz - (cz * chunk_size.z);

	MPChunk &chunk = chunks[_get_chunk_index(cx, cy, cz)];
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
		// --- APPLY STAGGER TO NEWLY SPAWNED CORNER ---
		float stagger = (gz % 2 != 0) ? 0.5f : 0.0f;
		Vector3 world_pos = Vector3(
				static_cast<float>(gx) + stagger,
				static_cast<float>(gy),
				static_cast<float>(gz) * 0.866025f);

		mi = memnew(MeshInstance3D);
		mi->set_mesh(_debug_box_mesh);
		mi->set_position(world_pos);

		if (!debug_corners_container) {
			debug_corners_container = memnew(Node3D);
			debug_corners_container->set_name("DebugCorners");
			add_child(debug_corners_container);
		}
		debug_corners_container->add_child(mi);
		if (is_inside_tree())
			mi->set_owner(get_owner() ? get_owner() : this);

		chunk.debug_visuals[corner_idx] = mi;
		total_debug_corners++;
	} else {
		TypedArray<Node> col_children = mi->get_children();
		for (int i = 0; i < col_children.size(); i++) {
			Node *child = Object::cast_to<Node>(col_children[i]);
			if (Object::cast_to<StaticBody3D>(child))
				child->queue_free();
		}
	}
	if (state) {
		mi->set_material_override(_debug_mat_red);
		if (has_inactive) {
			MCPhysics::create_static_box_collider(mi, toLayer(LAYER_CORNERS), Vector3(1.0, 1.0, 1.0));
		}
	} else {
		mi->set_material_override(_debug_mat_blue);
	}
}

void MPGrid::_initialize_hover_previews() {
	if (hover_root)
		return;

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

void MPGrid::update_hover_preview(const Vector3 &p_corner_pos, const Vector3 &p_hit_normal, Camera3D *p_camera) {
	if (!p_camera)
		return;
	if (!hover_root)
		_initialize_hover_previews();

	Vector3 cam_pos = p_camera->get_global_position();
	Vector3 dir_to_cam = cam_pos - p_corner_pos;

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

void MPGrid::hide_hover_preview() {
	if (hover_root) {
		hover_root->hide();
	}
}

} // namespace godot
