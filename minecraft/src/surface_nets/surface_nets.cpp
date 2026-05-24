#include "surface_nets/surface_nets.h"
#include "surface_nets/sn_grid.h"
#include <map>

namespace godot {

SurfaceNets::MeshData SurfaceNets::generate_mesh(const SNGrid *p_grid, const Vector3i &p_chunk_loc, const Vector3i &p_chunk_size) {
	MeshData data;

	Vector3i start_cell = p_chunk_loc * p_chunk_size - Vector3i(1, 1, 1);
	Vector3i end_cell = (p_chunk_loc + Vector3i(1, 1, 1)) * p_chunk_size;

	// Cell helper definitions
	const Vector3i corner_offsets[8] = {
		Vector3i(0, 0, 1), // c0
		Vector3i(1, 0, 1), // c1
		Vector3i(1, 0, 0), // c2
		Vector3i(0, 0, 0), // c3
		Vector3i(0, 1, 1), // c4
		Vector3i(1, 1, 1), // c5
		Vector3i(1, 1, 0), // c6
		Vector3i(0, 1, 0)  // c7
	};

	const int cell_edges[12][2] = {
		{0, 1}, {1, 2}, {2, 3}, {3, 0}, // Bottom face
		{4, 5}, {5, 6}, {6, 7}, {7, 4}, // Top face
		{0, 4}, {1, 5}, {2, 6}, {3, 7}  // Vertical edges
	};

	std::map<Vector3i, int> cell_to_index;
	int vert_count = 0;

	// 1. Scan and compute vertices for crossing cells
	for (int gy = start_cell.y; gy <= end_cell.y; ++gy) {
		for (int gz = start_cell.z; gz <= end_cell.z; ++gz) {
			for (int gx = start_cell.x; gx <= end_cell.x; ++gx) {
				Vector3i global_cell(gx, gy, gz);

				int8_t densities[8];
				bool has_solid = false;
				bool has_empty = false;
				for (int i = 0; i < 8; ++i) {
					densities[i] = p_grid->get_density(global_cell + corner_offsets[i]);
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
							Vector3 pA = Vector3(global_cell + corner_offsets[idxA]);
							Vector3 pB = Vector3(global_cell + corner_offsets[idxB]);
							sum_pts += pA.lerp(pB, t);
							count++;
						}
					}

					Vector3 v_pos;
					if (count > 0) {
						v_pos = sum_pts / static_cast<float>(count);
					} else {
						v_pos = Vector3(global_cell) + Vector3(0.5f, 0.5f, 0.5f);
					}

					data.vertices.push_back(v_pos);

					// Compute gradient normal using central differences
					float nx = static_cast<float>(p_grid->get_density(Vector3i(gx + 1, gy, gz)) - p_grid->get_density(Vector3i(gx - 1, gy, gz)));
					float ny = static_cast<float>(p_grid->get_density(Vector3i(gx, gy + 1, gz)) - p_grid->get_density(Vector3i(gx, gy - 1, gz)));
					float nz = static_cast<float>(p_grid->get_density(Vector3i(gx, gy, gz + 1)) - p_grid->get_density(Vector3i(gx, gy, gz - 1)));
					Vector3 norm(nx, ny, nz);
					if (norm.length_squared() > 0.001f) {
						norm.normalize();
					} else {
						norm = Vector3(0, 1, 0);
					}
					data.normals.push_back(norm);

					cell_to_index[global_cell] = vert_count++;
				}
			}
		}
	}

	if (data.vertices.is_empty()) {
		return data;
	}

	// Helper to add a quad (2 triangles) connecting 4 cells sharing a crossing edge
	auto add_quad = [&](const Vector3i &c1, const Vector3i &c2, const Vector3i &c3, const Vector3i &c4, const Vector3i &edge_start, const Vector3i &edge_dir) {
		if (cell_to_index.find(c1) == cell_to_index.end() ||
			cell_to_index.find(c2) == cell_to_index.end() ||
			cell_to_index.find(c3) == cell_to_index.end() ||
			cell_to_index.find(c4) == cell_to_index.end()) {
			return;
		}

		int idx1 = cell_to_index[c1];
		int idx2 = cell_to_index[c2];
		int idx3 = cell_to_index[c3];
		int idx4 = cell_to_index[c4];

		// Determine CCW winding using cell centers
		Vector3 cc1 = Vector3(c1) + Vector3(0.5f, 0.5f, 0.5f);
		Vector3 cc2 = Vector3(c2) + Vector3(0.5f, 0.5f, 0.5f);
		Vector3 cc3 = Vector3(c3) + Vector3(0.5f, 0.5f, 0.5f);

		Vector3 N = (cc2 - cc1).cross(cc3 - cc1);
		Vector3 V_edge = Vector3(edge_dir);

		bool a_solid = p_grid->get_density(edge_start) < 0;
		bool order_ok = false;

		if (a_solid) {
			order_ok = (N.dot(V_edge) > 0.0f);
		} else {
			order_ok = (N.dot(V_edge) < 0.0f);
		}

		if (order_ok) {
			data.indices.push_back(idx1);
			data.indices.push_back(idx3);
			data.indices.push_back(idx2);

			data.indices.push_back(idx1);
			data.indices.push_back(idx4);
			data.indices.push_back(idx3);
		} else {
			data.indices.push_back(idx1);
			data.indices.push_back(idx2);
			data.indices.push_back(idx3);

			data.indices.push_back(idx1);
			data.indices.push_back(idx3);
			data.indices.push_back(idx4);
		}
	};

	// 2. Scan internal edges of the chunk and generate indices
	int start_x = p_chunk_loc.x * p_chunk_size.x;
	int end_x = (p_chunk_loc.x + 1) * p_chunk_size.x;
	int start_y = p_chunk_loc.y * p_chunk_size.y;
	int end_y = (p_chunk_loc.y + 1) * p_chunk_size.y;
	int start_z = p_chunk_loc.z * p_chunk_size.z;
	int end_z = (p_chunk_loc.z + 1) * p_chunk_size.z;

	for (int gy = start_y; gy < end_y; ++gy) {
		for (int gz = start_z; gz < end_z; ++gz) {
			for (int gx = start_x; gx < end_x; ++gx) {
				Vector3i global_cell(gx, gy, gz);

				// X-edge: from (gx, gy, gz) to (gx+1, gy, gz)
				if ((p_grid->get_density(global_cell) < 0) != (p_grid->get_density(global_cell + Vector3i(1, 0, 0)) < 0)) {
					add_quad(
						Vector3i(gx, gy - 1, gz - 1),
						Vector3i(gx, gy, gz - 1),
						Vector3i(gx, gy, gz),
						Vector3i(gx, gy - 1, gz),
						global_cell,
						Vector3i(1, 0, 0)
					);
				}

				// Y-edge: from (gx, gy, gz) to (gx, gy+1, gz)
				if ((p_grid->get_density(global_cell) < 0) != (p_grid->get_density(global_cell + Vector3i(0, 1, 0)) < 0)) {
					add_quad(
						Vector3i(gx - 1, gy, gz - 1),
						Vector3i(gx, gy, gz - 1),
						Vector3i(gx, gy, gz),
						Vector3i(gx - 1, gy, gz),
						global_cell,
						Vector3i(0, 1, 0)
					);
				}

				// Z-edge: from (gx, gy, gz) to (gx, gy, gz+1)
				if ((p_grid->get_density(global_cell) < 0) != (p_grid->get_density(global_cell + Vector3i(0, 0, 1)) < 0)) {
					add_quad(
						Vector3i(gx - 1, gy - 1, gz),
						Vector3i(gx, gy - 1, gz),
						Vector3i(gx, gy, gz),
						Vector3i(gx - 1, gy, gz),
						global_cell,
						Vector3i(0, 0, 1)
					);
				}
			}
		}
	}

	return data;
}

} // namespace godot
