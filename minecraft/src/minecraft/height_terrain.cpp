#include "minecraft.h"

#include "godot_cpp/classes/array_mesh.hpp"
#include "godot_cpp/classes/object.hpp"
#include "godot_cpp/classes/ref.hpp"
#include "godot_cpp/core/memory.hpp"

#include "godot_cpp/variant/packed_int32_array.hpp"
#include "godot_cpp/variant/packed_vector3_array.hpp"
#include <godot_cpp/variant/utility_functions.hpp>

#include <godot_cpp/classes/mesh_instance3d.hpp>

void MinecraftNode::generate_terrain(String name, Vector3 pos) {
	PackedVector3Array vertices;
	PackedInt32Array indices;

	// Generate vertices from height map
	for (int z = 0; z < terrain_len_z; ++z) {
		for (int x = 0; x < terrain_len_x; ++x) {
			float y = get_height(x, z);
			vertices.push_back(Vector3(pos.x + (float)x, pos.y + y, pos.z + (float)z));
		}
	}

	// Generate indices (two triangles per quad)
	for (int z = 0; z < terrain_len_z - 1; ++z) {
		for (int x = 0; x < terrain_len_x - 1; ++x) {
			int i0 = x + z * terrain_len_x;
			int i1 = (x + 1) + z * terrain_len_x;
			int i2 = x + (z + 1) * terrain_len_x;
			int i3 = (x + 1) + (z + 1) * terrain_len_x;
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

	// Create MeshInstance3D
	MeshInstance3D *mi = Object::cast_to<MeshInstance3D>(get_node_or_null(name));
	if (!mi) {
		mi = memnew(MeshInstance3D);
		mi->set_name(name);
		add_child(mi);
	}
	mi->set_mesh(mesh);
	mi->set_material_override(terrain_material);
}
