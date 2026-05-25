#ifndef SURFACE_NETS_H
#define SURFACE_NETS_H

#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/variant/vector3i.hpp>
#include <godot_cpp/variant/packed_vector3_array.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>
#include <vector>
#include <algorithm>

namespace godot {

class SNGrid;

struct SurfaceNetsBuffer {
	std::vector<Vector3> positions;
	std::vector<Vector3> normals;
	std::vector<int32_t> indices;

	std::vector<Vector3i> surface_points;
	std::vector<int> surface_strides;
	std::vector<int> stride_to_index;

	void reset(size_t array_size) {
		positions.clear();
		normals.clear();
		indices.clear();
		surface_points.clear();
		surface_strides.clear();

		if (stride_to_index.size() < array_size) {
			stride_to_index.resize(array_size, -1);
		} else {
			std::fill(stride_to_index.begin(), stride_to_index.begin() + array_size, -1);
		}
	}
};

class SurfaceNets {
public:
	struct MeshData {
		PackedVector3Array vertices;
		PackedVector3Array normals;
		PackedInt32Array indices;
	};

	static MeshData generate_mesh(const SNGrid *p_grid, const Vector3i &p_chunk_loc, const Vector3i &p_chunk_size, SurfaceNetsBuffer &p_buffer, bool p_cell_center = false, bool p_smooth_normal = true);
};

} // namespace godot

#endif // SURFACE_NETS_H
