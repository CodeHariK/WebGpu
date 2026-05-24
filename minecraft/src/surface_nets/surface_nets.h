#ifndef SURFACE_NETS_H
#define SURFACE_NETS_H

#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/variant/vector3i.hpp>
#include <godot_cpp/variant/packed_vector3_array.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>

namespace godot {

class SNGrid;

class SurfaceNets {
public:
	struct MeshData {
		PackedVector3Array vertices;
		PackedVector3Array normals;
		PackedInt32Array indices;
	};

	static MeshData generate_mesh(const SNGrid *p_grid, const Vector3i &p_chunk_loc, const Vector3i &p_chunk_size);
};

} // namespace godot

#endif // SURFACE_NETS_H
