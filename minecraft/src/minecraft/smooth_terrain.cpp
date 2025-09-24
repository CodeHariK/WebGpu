#include "minecraft.h"

#include "godot_cpp/classes/array_mesh.hpp"
#include "godot_cpp/classes/object.hpp"
#include "godot_cpp/classes/ref.hpp"
#include "godot_cpp/core/memory.hpp"
#include "godot_cpp/variant/packed_int32_array.hpp"
#include "godot_cpp/variant/packed_vector3_array.hpp"
#include "godot_cpp/variant/string.hpp"
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include <cstdlib>

MeshInstance3D *MinecraftNode::generate_smooth_part_mesh(String name, Vector2i indexPos, bool height_curve_sampling, bool create_collision) {
	PackedVector3Array vertices;
	PackedInt32Array indices;

	const int vert_dim = part_size + 1;
	PackedInt32Array heights = generate_terrain_heights(indexPos, vert_dim, 1, height_curve_sampling);

	// Generate vertices from height map
	for (int z = 0; z < vert_dim; ++z) {
		for (int x = 0; x < vert_dim; ++x) {
			float y = heights[x + z * vert_dim];
			vertices.push_back(Vector3((float)x, y, (float)z));
		}
	}

	// Generate indices (two triangles per quad)
	for (int z = 0; z < part_size; ++z) {
		for (int x = 0; x < part_size; ++x) {
			int i0 = x + z * vert_dim;
			int i1 = (x + 1) + z * vert_dim;
			int i2 = x + (z + 1) * vert_dim;
			int i3 = (x + 1) + (z + 1) * vert_dim;
			// First triangle
			indices.push_back(i0);
			indices.push_back(i1);
			indices.push_back(i2);
			// Second triangle
			indices.push_back(i1);
			indices.push_back(i3);
			indices.push_back(i2);
		}
	}

	PackedVector3Array normals;
	normals.resize(vertices.size());
	for (int i = 0; i < normals.size(); ++i) {
		normals[i] = Vector3(0, 1, 0);
	}

	// Build surface array
	Array arrays;
	arrays.resize(Mesh::ARRAY_MAX);
	arrays[Mesh::ARRAY_VERTEX] = vertices;
	arrays[Mesh::ARRAY_NORMAL] = normals;
	arrays[Mesh::ARRAY_INDEX] = indices;

	Ref<ArrayMesh> mesh;
	mesh.instantiate();
	mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, arrays);

	MeshInstance3D *mi = Object::cast_to<MeshInstance3D>(get_node_or_null(name));
	if (!mi) {
		mi = memnew(MeshInstance3D);
		mi->set_name(name);
	}
	mi->set_mesh(mesh);
	mi->set_material_override(terrain_material);
	mi->set_position(Vector3(indexPos.x * part_size, 0, indexPos.y * part_size));

	// If requested, create a static collision body for the mesh.
	if (create_collision) {
		mi->create_trimesh_collision();
	}

	return mi;
}
