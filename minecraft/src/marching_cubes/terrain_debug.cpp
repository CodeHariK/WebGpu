#include "mc.h"
#include "terrain.h"
#include "mc_physics.h"
#include <godot_cpp/classes/box_mesh.hpp>
#include <godot_cpp/classes/box_shape3d.hpp>
#include <godot_cpp/classes/collision_shape3d.hpp>
#include <godot_cpp/classes/label3d.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/static_body3d.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

void MCTerrain::set_show_debug_corners(bool p_show) {
	if (show_debug_corners == p_show) {
		return;
	}
	show_debug_corners = p_show;
	set_debug_corners_visible(show_debug_corners);
}

bool MCTerrain::get_show_debug_corners() const {
	return show_debug_corners;
}

void MCTerrain::set_debug_corners_visible(bool p_visible) {
	if (debug_corners_container) {
		debug_corners_container->set_visible(p_visible);
	}
}

bool MCTerrain::is_debug_corners_visible() const {
	if (debug_corners_container) {
		return debug_corners_container->is_visible();
	}
	return false;
}

void MCTerrain::set_corner_collision_enabled(bool p_enabled) {
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

int MCTerrain::_spawn_debug_cubes(const Chunk &p_chunk, const Ref<BoxMesh> &p_box_mesh) {
	int nx = p_chunk.size_x + 1;
	int ny = p_chunk.size_y + 1;
	int nz = p_chunk.size_z + 1;
	int count = 0;

	Ref<StandardMaterial3D> mat_red;
	mat_red.instantiate();
	mat_red->set_albedo(Color(1, 0, 0));
	mat_red->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);

	Ref<StandardMaterial3D> mat_white;
	mat_white.instantiate();
	mat_white->set_albedo(Color(1, 1, 1));
	mat_white->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);

	Ref<StandardMaterial3D> mat_blue;
	mat_blue.instantiate();
	mat_blue->set_albedo(Color(0, 0, 1));
	mat_blue->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);

	for (int ly = 0; ly < ny; ly++) {
		for (int lz = 0; lz < nz; lz++) {
			for (int lx = 0; lx < nx; lx++) {
				bool state = p_chunk.get_corner(lx, ly, lz);

				Vector3 world_pos = Vector3(
						static_cast<float>((p_chunk.loc_x * p_chunk.size_x) + lx),
						static_cast<float>((p_chunk.loc_y * p_chunk.size_y) + ly),
						static_cast<float>((p_chunk.loc_z * p_chunk.size_z) + lz));

				MeshInstance3D *mi = memnew(MeshInstance3D);
				mi->set_mesh(p_box_mesh);
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

				bool has_active = p_chunk.has_active_neighbor(lx, ly, lz);
				bool has_inactive = p_chunk.has_inactive_neighbor(lx, ly, lz);

				if (state) {
					mi->set_material_override(mat_red);

					if (has_inactive) {
						MCPhysics::create_static_box_collider(
								mi,
								LAYER_CORNERS,
								Vector3(1.0, 1.0, 1.0));
					}
				} else if (has_active) {
					mi->set_material_override(mat_blue);
				} else {
					mi->set_material_override(mat_white);
				}

				count++;
				total_debug_corners++;
			}
		}
	}

	return count;
}

} // namespace godot
