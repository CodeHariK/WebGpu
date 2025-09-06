#include "godot_cpp/classes/array_mesh.hpp"
#include "godot_cpp/classes/object.hpp"
#include "godot_cpp/classes/ref.hpp"
#include "godot_cpp/core/memory.hpp"
#include "godot_cpp/variant/packed_vector2_array.hpp"
#include "godot_cpp/variant/packed_vector3_array.hpp"
#include "minecraft.h"

#include "godot_cpp/variant/packed_int32_array.hpp"
#include "godot_cpp/variant/vector3.hpp"
#include <cstdint>
#include <godot_cpp/variant/utility_functions.hpp>

#include <godot_cpp/classes/mesh_instance3d.hpp>

struct FaceData {
	Vector3i vertices[4];
	Vector2 uvs[4];
};

enum FaceDirection : int8_t {
	FACE_TOP = 0, // +Y
	FACE_BOTTOM, // -Y
	FACE_RIGHT, // +X
	FACE_LEFT, // -X
	FACE_FRONT, // +Z
	FACE_BACK // -Z
};

const FaceData face_lookup_arr[6] = {
	// FACE_TOP (+Y)
	{
			{ Vector3i(0, 1, 0), Vector3i(1, 1, 0), Vector3i(1, 1, 1), Vector3i(0, 1, 1) },
			{ Vector2(0, 0), Vector2(0.125, 0), Vector2(0.125, 0.125), Vector2(0, 0.125) } },
	// FACE_BOTTOM (-Y)
	{
			{ Vector3i(0, 0, 0), Vector3i(0, 0, 1), Vector3i(1, 0, 1), Vector3i(1, 0, 0) },
			{ Vector2(0.125, 0), Vector2(0.125, 0.125), Vector2(0.25, 0.125), Vector2(0.25, 0) } },
	// FACE_RIGHT (+X)
	{
			{ Vector3i(1, 1, 1), Vector3i(1, 1, 0), Vector3i(1, 0, 0), Vector3i(1, 0, 1) },
			{ Vector2(0.25, 0), Vector2(0.375, 0), Vector2(0.375, 0.125), Vector2(0.25, 0.125) } },
	// FACE_LEFT (-X)
	{
			{ Vector3i(0, 1, 0), Vector3i(0, 1, 1), Vector3i(0, 0, 1), Vector3i(0, 0, 0) },
			{ Vector2(0.375, 0), Vector2(0.5, 0), Vector2(0.5, 0.125), Vector2(0.375, 0.125) } },
	// FACE_FRONT (+Z)
	{
			{ Vector3i(0, 1, 1), Vector3i(1, 1, 1), Vector3i(1, 0, 1), Vector3i(0, 0, 1) },
			{ Vector2(0.5, 0), Vector2(0.625, 0), Vector2(0.625, 0.125), Vector2(0.5, 0.125) } },
	// FACE_BACK (-Z)
	{
			{ Vector3i(1, 1, 0), Vector3i(0, 1, 0), Vector3i(0, 0, 0), Vector3i(1, 0, 0) },
			{ Vector2(0.625, 0), Vector2(0.75, 0), Vector2(0.75, 0.125), Vector2(0.625, 0.125) } }
};

inline int direction_to_face_index(const Vector3i &dir) {
	if (dir == Vector3i(0, 1, 0))
		return FACE_TOP;
	if (dir == Vector3i(0, -1, 0))
		return FACE_BOTTOM;
	if (dir == Vector3i(1, 0, 0))
		return FACE_RIGHT;
	if (dir == Vector3i(-1, 0, 0))
		return FACE_LEFT;
	if (dir == Vector3i(0, 0, 1))
		return FACE_FRONT;
	if (dir == Vector3i(0, 0, -1))
		return FACE_BACK;
	return -1; // Invalid direction
}

// Then modify build_chunk_mesh to use the lookup map:
void add_face(PackedVector3Array &vertices, PackedVector3Array &normals,
		PackedVector2Array &uvs, PackedInt32Array &indices,
		const Vector3i &pos, const Vector3i &dir, int &index_offset) {
	int face_index = direction_to_face_index(dir);
	if (face_index != -1) {
		const FaceData &face_data = face_lookup_arr[face_index];

		// Add vertices
		for (const auto &v : face_data.vertices) {
			vertices.push_back(pos + v);
			normals.push_back(dir);
		}

		// Add UVs
		for (const auto &uv : face_data.uvs) {
			uvs.push_back(uv);
		}

		// Add indices
		indices.push_back(index_offset + 0);
		indices.push_back(index_offset + 1);
		indices.push_back(index_offset + 2);
		indices.push_back(index_offset + 2);
		indices.push_back(index_offset + 3);
		indices.push_back(index_offset + 0);

		index_offset += 4;
	}
}

Ref<ArrayMesh> MinecraftNode::build_chunk_mesh(int size) {
	PackedVector3Array vertices;
	PackedVector3Array normals;
	PackedVector2Array uvs;
	PackedInt32Array indices;
	int index_offset = 0;

	for (int x = 0; x < size; x++) {
		for (int z = 0; z < size; z++) {
			int h = get_height(x, z);

			// Top face
			add_face(vertices, normals, uvs, indices,
					Vector3i(x, h, z), Vector3i(0, 1, 0), index_offset);

			// Side faces
			const Vector3i dirs[4] = {
				Vector3i(-1, 0, 0), Vector3i(1, 0, 0),
				Vector3i(0, 0, -1), Vector3i(0, 0, 1)
			};
			const int dx[4] = { -1, 1, 0, 0 };
			const int dz[4] = { 0, 0, -1, 1 };

			for (int dir_idx = 0; dir_idx < 4; dir_idx++) {
				int nx = x + dx[dir_idx];
				int nz = z + dz[dir_idx];
				int neighbor_h = 0;
				if (nx >= 0 && nx < size && nz >= 0 && nz < size) {
					neighbor_h = get_height(nx, nz);
				}
				if (h > neighbor_h) {
					for (int y = neighbor_h + 1; y <= h; y++) {
						add_face(vertices, normals, uvs, indices,
								Vector3i(x, y, z), dirs[dir_idx], index_offset);
					}
				}
			}
		}
	}

	Array arrays;
	arrays.resize(Mesh::ARRAY_MAX);
	arrays[Mesh::ARRAY_VERTEX] = vertices;
	arrays[Mesh::ARRAY_NORMAL] = normals;
	arrays[Mesh::ARRAY_TEX_UV] = uvs;
	arrays[Mesh::ARRAY_INDEX] = indices;

	Ref<ArrayMesh> mesh;
	mesh.instantiate();
	mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, arrays);
	return mesh;
}

void MinecraftNode::generate_chunked_terrain(String name, Vector3 pos) {
	// Create voxel data
	int size = terrain_width;

	// Build one mesh directly from heights
	Ref<ArrayMesh> mesh = build_chunk_mesh(size);

	// Place a MeshInstance3D in the scene
	MeshInstance3D *mi = Object::cast_to<MeshInstance3D>(get_node_or_null(name));
	if (!mi) {
		mi = memnew(MeshInstance3D);
		mi->set_name(name);
		add_child(mi);
	}
	mi->set_mesh(mesh);
	mi->set_material_override(terrain_material);
	mi->set_position(pos);
}
