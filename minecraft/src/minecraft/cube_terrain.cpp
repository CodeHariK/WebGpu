#include "minecraft.h"

#include "godot_cpp/classes/node3d.hpp"
#include "godot_cpp/classes/object.hpp"
#include "godot_cpp/classes/ref.hpp"
#include "godot_cpp/core/memory.hpp"

#include "godot_cpp/variant/vector3.hpp"
#include <godot_cpp/variant/utility_functions.hpp>

#include <godot_cpp/classes/mesh_instance3d.hpp>

#include <godot_cpp/classes/box_mesh.hpp>

void MinecraftNode::generate_cube_terrain(String name, Vector3 pos) {
	// Parent node to hold all cubes
	Node3D *voxel_parent = Object::cast_to<Node3D>(get_node_or_null(name));
	if (!voxel_parent) {
		voxel_parent = memnew(Node3D);
		voxel_parent->set_name(name);
		add_child(voxel_parent);
	} else {
		// clear children cubes before reusing
		for (int i = voxel_parent->get_child_count() - 1; i >= 0; --i) {
			voxel_parent->get_child(i)->queue_free();
		}
	}

	// Create a shared cube mesh
	Ref<BoxMesh> cube_mesh;
	cube_mesh.instantiate();
	cube_mesh->set_size(Vector3(1, 1, 1));

	// Step 2: Place cubes for top + exposed sides
	for (int z = 0; z < terrain_depth; ++z) {
		for (int x = 0; x < terrain_width; ++x) {
			int h = get_height(x, z);

			// --- Top cube
			{
				MeshInstance3D *cube = memnew(MeshInstance3D);
				cube->set_mesh(cube_mesh);
				// cube->set_material_override(terrain_material);
				cube->set_position(Vector3(pos.x + x + 0.5f, pos.y + h + 0.5f, pos.z + z + 0.5f));
				cube->set_scale(Vector3(0.9f, 0.9f, 0.9f));
				voxel_parent->add_child(cube);
			}

			// --- Check neighbors
			const int dx[4] = { -1, 1, 0, 0 };
			const int dz[4] = { 0, 0, -1, 1 };

			for (int dir = 0; dir < 4; dir++) {
				int nx = x + dx[dir];
				int nz = z + dz[dir];

				int neighbor_h = 0;
				if (nx >= 0 && nx < terrain_width && nz >= 0 && nz < terrain_depth) {
					neighbor_h = get_height(nx, nz);
				}

				// If current column is higher, fill the side wall
				if (h > neighbor_h) {
					for (int y = neighbor_h + 1; y <= h; y++) {
						MeshInstance3D *cube = memnew(MeshInstance3D);
						cube->set_mesh(cube_mesh);
						// cube->set_material_override(terrain_material);
						cube->set_position(Vector3(pos.x + x + 0.5f, pos.y + y + 0.5f, pos.z + z + 0.5f));
						cube->set_scale(Vector3(0.9f, 0.9f, 0.9f));
						voxel_parent->add_child(cube);
					}
				}
			}
		}
	}
}
