// Merges placed tile meshes into one ArrayMesh (one surface per material) and the triangle list
// for its collider, by appending raw vertex arrays.
//
// Faster than SurfaceTool::append_from: each tile surface's arrays are read from the
// RenderingServer once (MCSurfaceCache) instead of on every rebuild, and vertices are copied
// straight into packed arrays. Optional arrays (normal, tangent, colour, uv, uv2) are kept when any
// tile has them; tiles without one get a default value, so mixed formats merge correctly.
#ifndef MC_CHUNK_BUILDER_H
#define MC_CHUNK_BUILDER_H

#include <godot_cpp/classes/array_mesh.hpp>
#include <godot_cpp/classes/material.hpp>
#include <godot_cpp/classes/mesh.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/packed_color_array.hpp>
#include <godot_cpp/variant/packed_float32_array.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>
#include <godot_cpp/variant/packed_vector2_array.hpp>
#include <godot_cpp/variant/packed_vector3_array.hpp>
#include <godot_cpp/variant/transform3d.hpp>
#include <unordered_map>
#include <vector>

namespace godot {

// Surface arrays of every tile mesh, read once (Mesh::surface_get_arrays is a slow copy).
class MCSurfaceCache {
	std::unordered_map<const Mesh *, std::vector<Array>> cache;

public:
	// Surface arrays of p_mesh (Mesh::surface_get_arrays per surface), read on first use.
	const std::vector<Array> &get(const Ref<Mesh> &p_mesh);
	// Forget every cached mesh (the tile library changed).
	void clear() { cache.clear(); }
};

class MCChunkBuilder {
	struct Surface {
		Ref<Material> material;
		PackedVector3Array vertices;
		PackedVector3Array normals;
		PackedFloat32Array tangents; // 4 floats per vertex
		PackedColorArray colors;
		PackedVector2Array uvs;
		PackedVector2Array uv2s;
		PackedInt32Array indices;
		bool has_normals = false;
		bool has_tangents = false;
		bool has_colors = false;
		bool has_uvs = false;
		bool has_uv2s = false;
	};

	std::vector<Surface> surfaces; // one per material, first-seen order
	PackedVector3Array faces; // every triangle, 3 vertices each, for ConcavePolygonShape3D
	bool collide = true; // the mesh being added goes into the collider too (decor does not)

	// The surface collecting p_material's triangles (created on first use).
	Surface &_surface_for(const Ref<Material> &p_material);
	void
	_append(Surface &r_surface,
			const Array &p_arrays,
			const Transform3D &p_xform);

public:
	// Add one placed tile (every surface of p_mesh, moved by p_xform); p_collide false: drawn only
	// (socket decor), not part of the collider.
	void
	add(const Ref<Mesh> &p_mesh,
		const Transform3D &p_xform,
		MCSurfaceCache &r_cache,
		bool p_collide = true);
	// Nothing added yet (or only empty meshes).
	bool is_empty() const { return faces.is_empty(); }
	// One ArrayMesh with a surface per material.
	Ref<ArrayMesh> commit() const;
	// Every triangle added (3 vertices each), for a ConcavePolygonShape3D.
	const PackedVector3Array &get_faces() const { return faces; }
};

} // namespace godot

#endif // MC_CHUNK_BUILDER_H
