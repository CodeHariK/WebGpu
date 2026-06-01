/**
 * @file mp_grid_debug.cpp
 * @brief Handles debug visualizations (cubes/spheres) and collision boundaries for Marching Prism chunks.
 * Module Path: src/terrain/marching_prism/mp_grid_debug.cpp
 * Build Dependencies: godot-cpp, mp.h, mp_grid.h
 */

#include "marching_cubes/mc_physics.h" // Use MCPhysics helper
#include "mp.h"
#include "mp_grid.h"
#include <godot_cpp/classes/box_mesh.hpp>
#include <godot_cpp/classes/box_shape3d.hpp>
#include <godot_cpp/classes/camera3d.hpp>
#include <godot_cpp/classes/collision_shape3d.hpp>
#include <godot_cpp/classes/cylinder_mesh.hpp>
#include <godot_cpp/classes/cylinder_shape3d.hpp>
#include <godot_cpp/classes/label3d.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/quad_mesh.hpp>
#include <godot_cpp/classes/sphere_mesh.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/static_body3d.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

Vector3 MPGrid::_get_corner_world_pos(const MPChunk &p_chunk, int lx, int ly, int lz) const {
	int global_z = (p_chunk.loc_z * p_chunk.size_z) + lz;
	float stagger = (global_z % 2 != 0) ? 0.5f : 0.0f;
	return Vector3(
			static_cast<float>((p_chunk.loc_x * p_chunk.size_x) + lx) + stagger,
			static_cast<float>((p_chunk.loc_y * p_chunk.size_y) + ly),
			static_cast<float>(global_z) * 0.866025f);
}

int MPGrid::_spawn_debug_spheres(const MPChunk &p_chunk, const Ref<SphereMesh> &p_corner_mesh) {
	int nx = p_chunk.size_x + 1;
	int ny = p_chunk.size_y + 1;
	int nz = p_chunk.size_z + 1;
	int count = 0;

	if (_debug_corner_mesh.is_null()) {
		_debug_corner_mesh.instantiate();
	}
	_debug_corner_mesh->set_radius(0.04f);
	_debug_corner_mesh->set_height(0.08f);

	if (_debug_mat_red.is_null()) {
		_debug_mat_red.instantiate();
		_debug_mat_red->set_albedo(Color(1.0f, 0.0f, 0.0f));
		_debug_mat_red->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
	}

	if (_debug_mat_blue.is_null()) {
		_debug_mat_blue.instantiate();
		_debug_mat_blue->set_albedo(Color(0.0f, 0.0f, 1.0f));
		_debug_mat_blue->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
	}

	for (int ly = 0; ly < ny; ly++) {
		for (int lz = 0; lz < nz; lz++) {
			for (int lx = 0; lx < nx; lx++) {
				bool state = p_chunk.get_corner(lx, ly, lz);
				bool has_active = p_chunk.has_active_neighbor(lx, ly, lz);
				bool has_inactive = p_chunk.has_inactive_neighbor(lx, ly, lz);

				Vector3 world_pos = _get_corner_world_pos(p_chunk, lx, ly, lz);

				MeshInstance3D *mi = memnew(MeshInstance3D);
				mi->set_mesh(_debug_corner_mesh);
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
						MCPhysics::create_static_sphere_collider(mi, toLayer(LAYER_CORNERS), 0.3f);
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

	if (!mi) {
		Vector3 world_pos = _get_corner_world_pos(chunk, lx, ly, lz);

		mi = memnew(MeshInstance3D);
		mi->set_mesh(_debug_corner_mesh);
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
			MCPhysics::create_static_sphere_collider(mi, toLayer(LAYER_CORNERS), 0.4f);
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
	hover_mat_white->set_albedo(Color(1, 1, 1, hover_cylinder_alpha));
	hover_mat_white->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
	hover_mat_white->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);

	Ref<CylinderMesh> cyl_mesh;
	cyl_mesh.instantiate();
	cyl_mesh->set_top_radius(0.5f);
	cyl_mesh->set_bottom_radius(0.5f);
	cyl_mesh->set_height(1.0f);

	hover_cylinder = memnew(MeshInstance3D);
	hover_cylinder->set_mesh(cyl_mesh);
	hover_cylinder->set_material_override(hover_mat_white);
	hover_root->add_child(hover_cylinder);

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

void MPGrid::update_hover_preview(const Vector3 &p_corner_pos, const Vector3 &p_hit_normal, Camera3D *p_camera, bool p_is_cell) {
	if (!p_camera)
		return;
	if (!hover_root)
		_initialize_hover_previews();

	hover_root->set_position(p_corner_pos);
	hover_root->show();

	if (p_is_cell) {
		hover_cylinder->show();
		for (int i = 0; i < 3; ++i) {
			hover_quads[i]->hide();
		}
	} else {
		hover_cylinder->show();

		Vector3 directions[8] = {
			Vector3(1.0f, 0.0f, 0.0f),
			Vector3(-1.0f, 0.0f, 0.0f),
			Vector3(0.5f, 0.0f, 0.866025f),
			Vector3(-0.5f, 0.0f, 0.866025f),
			Vector3(0.5f, 0.0f, -0.866025f),
			Vector3(-0.5f, 0.0f, -0.866025f),
			Vector3(0.0f, 1.0f, 0.0f),
			Vector3(0.0f, -1.0f, 0.0f)
		};

		Vector3 closest_dir = directions[0];
		float max_dot = -2.0f;
		for (int i = 0; i < 8; ++i) {
			float dot = p_hit_normal.dot(directions[i]);
			if (dot > max_dot) {
				max_dot = dot;
				closest_dir = directions[i];
			}
		}

		MeshInstance3D *quad = hover_quads[0];
		quad->show();
		hover_quads[1]->hide();
		hover_quads[2]->hide();

		Basis target_basis;
		if (Math::abs(closest_dir.y) > 0.99f) {
			target_basis = Basis::looking_at(-closest_dir, Vector3(0, 0, 1));
		} else {
			target_basis = Basis::looking_at(-closest_dir, Vector3(0, 1, 0));
		}

		quad->set_transform(Transform3D(target_basis, closest_dir * 0.505f));
		quad->set_material_override(hover_mat_yellow);
	}
}

void MPGrid::hide_hover_preview() {
	if (hover_root) {
		hover_root->hide();
	}
}

void MPGrid::_build_grid_wireframe() {
	if (!debug_corners_container) {
		debug_corners_container = memnew(Node3D);
		debug_corners_container->set_name("DebugCorners");
		add_child(debug_corners_container);
	}

	PackedVector3Array points;

	for (const MPChunk &chunk : chunks) {
		for (int y = 0; y < chunk.size_y; y++) {
			for (int z = 0; z < chunk.size_z; z++) {
				for (int cx = 0; cx < chunk.size_x * 2; cx++) {
					int vx = cx / 2;
					int v0_x, v0_z, v1_x, v1_z, v2_x, v2_z;
					bool points_down = (cx % 2 == 0);

					if (z % 2 == 0) {
						if (points_down) {
							v1_x = vx;
							v1_z = z;
							v0_x = vx + 1;
							v0_z = z;
							v2_x = vx;
							v2_z = z + 1;
						} else {
							v0_x = vx;
							v0_z = z + 1;
							v1_x = vx + 1;
							v1_z = z + 1;
							v2_x = vx + 1;
							v2_z = z;
						}
					} else {
						if (points_down) {
							v1_x = vx;
							v1_z = z;
							v0_x = vx + 1;
							v0_z = z;
							v2_x = vx + 1;
							v2_z = z + 1;
						} else {
							v0_x = vx + 1;
							v0_z = z + 1;
							v1_x = vx + 2;
							v1_z = z + 1;
							v2_x = vx + 1;
							v2_z = z;
						}
					}

					Vector3 v0 = _get_corner_world_pos(chunk, v0_x, y, v0_z);
					Vector3 v1 = _get_corner_world_pos(chunk, v1_x, y, v1_z);
					Vector3 v2 = _get_corner_world_pos(chunk, v2_x, y, v2_z);
					Vector3 v0_t = _get_corner_world_pos(chunk, v0_x, y + 1, v0_z);
					Vector3 v1_t = _get_corner_world_pos(chunk, v1_x, y + 1, v1_z);
					Vector3 v2_t = _get_corner_world_pos(chunk, v2_x, y + 1, v2_z);

					// Bottom triangle
					points.push_back(v0);
					points.push_back(v1);
					points.push_back(v1);
					points.push_back(v2);
					points.push_back(v2);
					points.push_back(v0);

					// Top triangle
					points.push_back(v0_t);
					points.push_back(v1_t);
					points.push_back(v1_t);
					points.push_back(v2_t);
					points.push_back(v2_t);
					points.push_back(v0_t);

					// Verticals
					points.push_back(v0);
					points.push_back(v0_t);
					points.push_back(v1);
					points.push_back(v1_t);
					points.push_back(v2);
					points.push_back(v2_t);
				}
			}
		}
	}

	if (points.size() == 0) {
		return;
	}

	Array arrays;
	arrays.resize(Mesh::ARRAY_MAX);
	arrays[Mesh::ARRAY_VERTEX] = points;

	Ref<ArrayMesh> arr_mesh;
	arr_mesh.instantiate();
	arr_mesh->add_surface_from_arrays(Mesh::PRIMITIVE_LINES, arrays);

	Ref<StandardMaterial3D> wireframe_mat;
	wireframe_mat.instantiate();
	wireframe_mat->set_albedo(Color(0.5f, 0.5f, 0.5f, 0.5f));
	wireframe_mat->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
	wireframe_mat->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);

	wireframe_instance = memnew(MeshInstance3D);
	wireframe_instance->set_mesh(arr_mesh);
	wireframe_instance->set_material_override(wireframe_mat);
	debug_corners_container->add_child(wireframe_instance);
	if (is_inside_tree()) {
		wireframe_instance->set_owner(get_owner() ? get_owner() : this);
	}
}

} // namespace godot
