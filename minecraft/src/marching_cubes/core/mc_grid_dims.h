// Grid dimensions and every global <-> chunk-local conversion, all bounds-checked.
// Corners run 0..grid_dims * chunk_dims on each axis; cells run 0..that - 1.
#ifndef MC_GRID_DIMS_H
#define MC_GRID_DIMS_H

#include <godot_cpp/core/math.hpp>
#include <godot_cpp/variant/vector3i.hpp>

namespace godot {

struct MCGridDims {
	Vector3i grid_dims; // chunks per axis
	Vector3i chunk_dims; // cells per chunk per axis

	// Highest corner index per axis; also the number of cells per axis.
	Vector3i max_corner() const { return grid_dims * chunk_dims; }

	// Index into MCGrid::chunks of the chunk at chunk coordinates (p_cx, p_cy, p_cz).
	int chunk_index(
			int p_cx,
			int p_cy,
			int p_cz
	) const {
		return (p_cy * grid_dims.x * grid_dims.z) + (p_cz * grid_dims.x) + p_cx;
	}

	// Is this global corner inside the grid (0..max_corner on every axis)?
	bool contains_corner(const Vector3i &p_corner) const {
		Vector3i max = max_corner();
		return p_corner.x >= 0 && p_corner.y >= 0 && p_corner.z >= 0 && p_corner.x <= max.x && p_corner.y <= max.y &&
				p_corner.z <= max.z;
	}

	// Global corner -> the chunk that OWNS it + its local corner. False outside the grid.
	// A corner on a shared chunk face belongs to the higher chunk (local 0), except on the
	// grid's far face, where only the last chunk has it (local = size).
	bool corner_to_local(
			const Vector3i &p_corner,
			int &r_chunk_index,
			Vector3i &r_local
	) const {
		if (!contains_corner(p_corner)) {
			return false;
		}
		// Never negative here, so integer division is floor division.
		Vector3i c(
				MIN(p_corner.x / chunk_dims.x, grid_dims.x - 1), MIN(p_corner.y / chunk_dims.y, grid_dims.y - 1),
				MIN(p_corner.z / chunk_dims.z, grid_dims.z - 1)
		);
		r_chunk_index = chunk_index(c.x, c.y, c.z);
		r_local = p_corner - c * chunk_dims;
		return true;
	}

	// Global cell -> its chunk + local cell. False outside the grid.
	bool cell_to_local(
			const Vector3i &p_cell,
			int &r_chunk_index,
			Vector3i &r_local
	) const {
		Vector3i max = max_corner();
		if (p_cell.x < 0 || p_cell.y < 0 || p_cell.z < 0 || p_cell.x >= max.x || p_cell.y >= max.y ||
			p_cell.z >= max.z) {
			return false;
		}
		Vector3i c(p_cell.x / chunk_dims.x, p_cell.y / chunk_dims.y, p_cell.z / chunk_dims.z);
		r_chunk_index = chunk_index(c.x, c.y, c.z);
		r_local = p_cell - c * chunk_dims;
		return true;
	}

	// Does the chunk at p_chunk_loc own its local corner? (Each shared corner has one owner.)
	bool owns_corner(
			const Vector3i &p_chunk_loc,
			const Vector3i &p_local
	) const {
		bool own_x = p_local.x < chunk_dims.x || p_chunk_loc.x == grid_dims.x - 1;
		bool own_y = p_local.y < chunk_dims.y || p_chunk_loc.y == grid_dims.y - 1;
		bool own_z = p_local.z < chunk_dims.z || p_chunk_loc.z == grid_dims.z - 1;
		return own_x && own_y && own_z;
	}
};

} // namespace godot

#endif // MC_GRID_DIMS_H
