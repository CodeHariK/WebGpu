#include "surface_nets/surface_nets.h"
#include "surface_nets/sdf_helpers.h"
#include "surface_nets/sn_grid.h"

namespace godot {

SurfaceNets::MeshData SurfaceNets::generate_mesh(const SNGrid *p_grid, const Vector3i &p_chunk_loc, const Vector3i &p_chunk_size, SurfaceNetsBuffer &p_buffer, bool p_cell_center, bool p_smooth_normal) {
	Vector3i start_cell = p_chunk_loc * p_chunk_size - Vector3i(1, 1, 1);
	Vector3i end_cell = (p_chunk_loc + Vector3i(1, 1, 1)) * p_chunk_size;
	Vector3i cache_end = end_cell + Vector3i(1, 1, 1);
	Vector3i size = cache_end - start_cell + Vector3i(1, 1, 1);

	auto linearize = [&](const Vector3i &cell) -> int {
		int lx = cell.x - start_cell.x;
		int ly = cell.y - start_cell.y;
		int lz = cell.z - start_cell.z;
		return lx + ly * size.x + lz * (size.x * size.y);
	};

	// Cell helper definitions
	const Vector3i corner_offsets[8] = {
		Vector3i(0, 0, 1), // c0
		Vector3i(1, 0, 1), // c1
		Vector3i(1, 0, 0), // c2
		Vector3i(0, 0, 0), // c3
		Vector3i(0, 1, 1), // c4
		Vector3i(1, 1, 1), // c5
		Vector3i(1, 1, 0), // c6
		Vector3i(0, 1, 0) // c7
	};

	const int cell_edges[12][2] = {
		{ 0, 1 }, { 1, 2 }, { 2, 3 }, { 3, 0 }, // Bottom face
		{ 4, 5 },
		{ 5, 6 },
		{ 6, 7 },
		{ 7, 4 }, // Top face
		{ 0, 4 },
		{ 1, 5 },
		{ 2, 6 },
		{ 3, 7 } // Vertical edges
	};

	// 1. Cache all density values for the volume to minimize grid checks
	std::vector<int8_t> densities_cache(size.x * size.y * size.z);
	for (int gy = start_cell.y; gy <= cache_end.y; ++gy) {
		for (int gz = start_cell.z; gz <= cache_end.z; ++gz) {
			for (int gx = start_cell.x; gx <= cache_end.x; ++gx) {
				Vector3i global_cell(gx, gy, gz);
				densities_cache[linearize(global_cell)] = p_grid->get_density(global_cell);
			}
		}
	}

	p_buffer.reset(size.x * size.y * size.z);

	// 2. Scan and compute vertices for crossing cells
	for (int gy = start_cell.y; gy <= end_cell.y; ++gy) {
		for (int gz = start_cell.z; gz <= end_cell.z; ++gz) {
			for (int gx = start_cell.x; gx <= end_cell.x; ++gx) {
				Vector3i global_cell(gx, gy, gz);
				int stride = linearize(global_cell);

				int8_t densities[8];
				bool has_solid = false;
				bool has_empty = false;
				for (int i = 0; i < 8; ++i) {
					densities[i] = densities_cache[linearize(global_cell + corner_offsets[i])];
					if (densities[i] < 0) {
						has_solid = true;
					} else {
						has_empty = true;
					}
				}

				if (has_solid && has_empty) {
					Vector3 sum_pts(0, 0, 0);
					int count = 0;
					for (int e = 0; e < 12; ++e) {
						int idxA = cell_edges[e][0];
						int idxB = cell_edges[e][1];
						int8_t dA = densities[idxA];
						int8_t dB = densities[idxB];
						if ((dA < 0 && dB >= 0) || (dA >= 0 && dB < 0)) {
							float t = -static_cast<float>(dA) / (static_cast<float>(dB) - static_cast<float>(dA));
							Vector3 pA = Vector3(corner_offsets[idxA]);
							Vector3 pB = Vector3(corner_offsets[idxB]);
							sum_pts += pA.lerp(pB, t);
							count++;
						}
					}

					Vector3 c;
					if (p_cell_center) {
						c = Vector3(0.5f, 0.5f, 0.5f);
					} else {
						if (count > 0) {
							c = sum_pts / static_cast<float>(count);
						} else {
							c = Vector3(0.5f, 0.5f, 0.5f);
						}
					}

					Vector3 v_pos = Vector3(global_cell) + c;
					p_buffer.positions.push_back(v_pos);

					// Compute normal using sdf_gradient
					float corner_dists[8];
					for (int i = 0; i < 8; ++i) {
						corner_dists[i] = static_cast<float>(densities[BINARY_TO_CPP_CORNER[i]]);
					}
					// If smooth normal is enabled, interpolate gradient at the surface point c.
					// Otherwise, sample the gradient at the cell center (0.5, 0.5, 0.5) for flat shading.
					Vector3 norm_sample_pt = p_smooth_normal ? c : Vector3(0.5f, 0.5f, 0.5f);
					Vector3 norm = sdf_gradient(corner_dists, norm_sample_pt);
					if (norm.length_squared() > 0.001f) {
						norm.normalize();
					} else {
						norm = Vector3(0, 1, 0);
					}
					p_buffer.normals.push_back(norm);

					p_buffer.stride_to_index[stride] = p_buffer.positions.size() - 1;
					p_buffer.surface_points.push_back(global_cell);
					p_buffer.surface_strides.push_back(stride);
				}
			}
		}
	}

	MeshData data;
	if (p_buffer.positions.empty()) {
		return data;
	}

	// 3. Helper lambda to add a quad (2 triangles) connecting 4 cells sharing a crossing edge
	auto maybe_make_quad = [&](int p1, int p2, int axis_b_stride, int axis_c_stride) {
		int8_t d1 = densities_cache[p1];
		int8_t d2 = densities_cache[p2];

		bool negative_face = false;
		if (d1 < 0 && d2 >= 0) {
			negative_face = true; // Flipped
		} else if (d1 >= 0 && d2 < 0) {
			negative_face = false; // Flipped
		} else {
			return; // No face.
		}

		int v1 = p_buffer.stride_to_index[p1];
		int v2 = p_buffer.stride_to_index[p1 - axis_b_stride];
		int v3 = p_buffer.stride_to_index[p1 - axis_c_stride];
		int v4 = p_buffer.stride_to_index[p1 - axis_b_stride - axis_c_stride];

		if (v1 == -1 || v2 == -1 || v3 == -1 || v4 == -1) {
			return;
		}

		Vector3 pos1 = p_buffer.positions[v1];
		Vector3 pos2 = p_buffer.positions[v2];
		Vector3 pos3 = p_buffer.positions[v3];
		Vector3 pos4 = p_buffer.positions[v4];

		// Split the quad along the shorter diagonal rather than the longer one
		if (pos1.distance_squared_to(pos4) < pos2.distance_squared_to(pos3)) {
			if (negative_face) {
				p_buffer.indices.push_back(v1);
				p_buffer.indices.push_back(v4);
				p_buffer.indices.push_back(v2);
				p_buffer.indices.push_back(v1);
				p_buffer.indices.push_back(v3);
				p_buffer.indices.push_back(v4);
			} else {
				p_buffer.indices.push_back(v1);
				p_buffer.indices.push_back(v2);
				p_buffer.indices.push_back(v4);
				p_buffer.indices.push_back(v1);
				p_buffer.indices.push_back(v4);
				p_buffer.indices.push_back(v3);
			}
		} else {
			if (negative_face) {
				p_buffer.indices.push_back(v2);
				p_buffer.indices.push_back(v3);
				p_buffer.indices.push_back(v4);
				p_buffer.indices.push_back(v2);
				p_buffer.indices.push_back(v1);
				p_buffer.indices.push_back(v3);
			} else {
				p_buffer.indices.push_back(v2);
				p_buffer.indices.push_back(v4);
				p_buffer.indices.push_back(v3);
				p_buffer.indices.push_back(v2);
				p_buffer.indices.push_back(v3);
				p_buffer.indices.push_back(v1);
			}
		}
	};

	// 4. Scan internal edges of the chunk and generate indices
	int start_x = p_chunk_loc.x * p_chunk_size.x;
	int end_x = (p_chunk_loc.x + 1) * p_chunk_size.x;
	int start_y = p_chunk_loc.y * p_chunk_size.y;
	int end_y = (p_chunk_loc.y + 1) * p_chunk_size.y;
	int start_z = p_chunk_loc.z * p_chunk_size.z;
	int end_z = (p_chunk_loc.z + 1) * p_chunk_size.z;

	int x_stride = 1;
	int y_stride = size.x;
	int z_stride = size.x * size.y;

	for (size_t i = 0; i < p_buffer.surface_points.size(); ++i) {
		Vector3i global_cell = p_buffer.surface_points[i];
		int gx = global_cell.x;
		int gy = global_cell.y;
		int gz = global_cell.z;

		if (gx >= start_x && gx < end_x && gy >= start_y && gy < end_y && gz >= start_z && gz < end_z) {
			int p1 = p_buffer.surface_strides[i];
			// X-edge: from (gx, gy, gz) to (gx+1, gy, gz)
			maybe_make_quad(p1, p1 + x_stride, y_stride, z_stride);
			// Y-edge: from (gx, gy, gz) to (gx, gy+1, gz)
			maybe_make_quad(p1, p1 + y_stride, z_stride, x_stride);
			// Z-edge: from (gx, gy, gz) to (gx, gy, gz+1)
			maybe_make_quad(p1, p1 + z_stride, x_stride, y_stride);
		}
	}

	// 5. Copy final mesh data for Godot compatibility
	data.vertices.resize(p_buffer.positions.size());
	if (!p_buffer.positions.empty()) {
		memcpy(data.vertices.ptrw(), p_buffer.positions.data(), p_buffer.positions.size() * sizeof(Vector3));
	}

	data.normals.resize(p_buffer.normals.size());
	if (!p_buffer.normals.empty()) {
		memcpy(data.normals.ptrw(), p_buffer.normals.data(), p_buffer.normals.size() * sizeof(Vector3));
	}

	data.indices.resize(p_buffer.indices.size());
	if (!p_buffer.indices.empty()) {
		memcpy(data.indices.ptrw(), p_buffer.indices.data(), p_buffer.indices.size() * sizeof(int32_t));
	}

	return data;
}

} // namespace godot
