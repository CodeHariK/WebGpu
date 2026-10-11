// Grid dimensions and every global <-> chunk-local conversion, all bounds-checked.
// Corners run 0..grid * chunk on each axis; cells run 0..grid * chunk - 1.
#ifndef MC_GRID_DIMS_H
#define MC_GRID_DIMS_H

#include <godot_cpp/core/math.hpp>
#include <godot_cpp/variant/vector3i.hpp>

namespace godot {

struct MCGridDims {
	Vector3i grid; // chunks per axis
	Vector3i chunk; // cells per chunk per axis

	Vector3i max_corner() const { return grid * chunk; }

	int chunk_index(
			int p_cx,
			int p_cy,
			int p_cz
	) const {
		return (p_cy * grid.x * grid.z) + (p_cz * grid.x) + p_cx;
	}

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
				MIN(p_corner.x / chunk.x, grid.x - 1), MIN(p_corner.y / chunk.y, grid.y - 1),
				MIN(p_corner.z / chunk.z, grid.z - 1)
		);
		r_chunk_index = chunk_index(c.x, c.y, c.z);
		r_local = p_corner - c * chunk;
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
		Vector3i c(p_cell.x / chunk.x, p_cell.y / chunk.y, p_cell.z / chunk.z);
		r_chunk_index = chunk_index(c.x, c.y, c.z);
		r_local = p_cell - c * chunk;
		return true;
	}

	// Does the chunk at p_chunk_loc own its local corner? (Each shared corner has one owner.)
	bool owns_corner(
			const Vector3i &p_chunk_loc,
			const Vector3i &p_local
	) const {
		bool own_x = p_local.x < chunk.x || p_chunk_loc.x == grid.x - 1;
		bool own_y = p_local.y < chunk.y || p_chunk_loc.y == grid.y - 1;
		bool own_z = p_local.z < chunk.z || p_chunk_loc.z == grid.z - 1;
		return own_x && own_y && own_z;
	}
};

} // namespace godot

#endif // MC_GRID_DIMS_H
