// One chunk of the corner grid: on/off corner bits (bit-packed), the cell hash lookup, its merged
// tile mesh and the debug-corner node pointers. Chunks store (size + 1) corners per axis, so a corner
// on a shared face exists in both neighbouring chunks (MCGrid keeps both copies in sync).
#ifndef MC_CHUNK_H
#define MC_CHUNK_H

#include "utils/encoding/rle.h"
#include <cstdint>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <vector>

namespace godot {

class MeshInstance3D;

struct Chunk {
	int size_x = 0;
	int size_y = 0;
	int size_z = 0;
	int loc_x = 0;
	int loc_y = 0;
	int loc_z = 0;
	std::vector<uint8_t> corner_bits; // Bit-packed states

	// Corner p_index (y, z, x order) solid?
	bool get_corner_bit(int p_index) const {
		if (corner_bits.empty()) {
			return false;
		}
		return (corner_bits[static_cast<size_t>(p_index / 8)] & (1 << (p_index % 8))) != 0;
	}

	// Set corner p_index (y, z, x order) solid or empty.
	void set_corner_bit(
			int p_index,
			bool p_state
	) {
		if (p_state) {
			corner_bits[static_cast<size_t>(p_index / 8)] |= (1 << (p_index % 8));
		} else {
			corner_bits[static_cast<size_t>(p_index / 8)] &= ~(1 << (p_index % 8));
		}
	}

	// Corner at chunk-local (x, y, z), 0..size on each axis.
	bool get_corner(
			int x,
			int y,
			int z
	) const {
		int nx = size_x + 1;
		int nz = size_z + 1;
		int index = (y * nx * nz) + (z * nx) + x;
		return get_corner_bit(index);
	}

	// Set the corner at chunk-local (x, y, z).
	void set_corner(
			int x,
			int y,
			int z,
			bool value
	) {
		int nx = size_x + 1;
		int nz = size_z + 1;
		int index = (y * nx * nz) + (z * nx) + x;
		set_corner_bit(index, value);
	}

	// 8-bit hash of the cell at chunk-local (x, y, z): bit i = corner Ci solid (core/mc_corners.h).
	uint8_t get_cell_hash(
			int x,
			int y,
			int z
	) const {
		uint8_t hash = 0;
		// c0: x, y, z+1 (LSB)
		if (get_corner(x, y, z + 1)) {
			hash |= (1 << 0);
		}
		// c1: x+1, y, z+1
		if (get_corner(x + 1, y, z + 1)) {
			hash |= (1 << 1);
		}
		// c2: x+1, y, z
		if (get_corner(x + 1, y, z)) {
			hash |= (1 << 2);
		}
		// c3: x, y, z
		if (get_corner(x, y, z)) {
			hash |= (1 << 3);
		}
		// c4: x, y+1, z+1
		if (get_corner(x, y + 1, z + 1)) {
			hash |= (1 << 4);
		}
		// c5: x+1, y+1, z+1
		if (get_corner(x + 1, y + 1, z + 1)) {
			hash |= (1 << 5);
		}
		// c6: x+1, y+1, z
		if (get_corner(x + 1, y + 1, z)) {
			hash |= (1 << 6);
		}
		// c7: x, y+1, z (MSB)
		if (get_corner(x, y + 1, z)) {
			hash |= (1 << 7);
		}
		return hash;
	}

	// Is any of the 6 neighbours of this corner (inside the chunk) solid?
	bool has_active_neighbor(
			int x,
			int y,
			int z
	) const {
		if (x > 0 && get_corner(x - 1, y, z))
			return true;
		if (x < size_x && get_corner(x + 1, y, z))
			return true;
		if (y > 0 && get_corner(x, y - 1, z))
			return true;
		if (y < size_y && get_corner(x, y + 1, z))
			return true;
		if (z > 0 && get_corner(x, y, z - 1))
			return true;
		if (z < size_z && get_corner(x, y, z + 1))
			return true;
		return false;
	}

	// Is any of the 6 neighbours of this corner (inside the chunk) empty?
	bool has_inactive_neighbor(
			int x,
			int y,
			int z
	) const {
		if (x > 0 && !get_corner(x - 1, y, z))
			return true;
		if (x < size_x && !get_corner(x + 1, y, z))
			return true;
		if (y > 0 && !get_corner(x, y - 1, z))
			return true;
		if (y < size_y && !get_corner(x, y + 1, z))
			return true;
		if (z > 0 && !get_corner(x, y, z - 1))
			return true;
		if (z < size_z && !get_corner(x, y, z + 1))
			return true;
		return false;
	}

	// Every tile of the chunk merged into one mesh (one surface per material) plus one trimesh
	// collider (a StaticBody3D child). Rebuilt as a whole after an edit (MCGrid::_build_chunk_mesh).
	MeshInstance3D *mesh_node = nullptr;
	int tile_count = 0; // tiles drawn in mesh_node
	bool dirty = false; // an edit changed a cell: rebuild mesh_node

	std::vector<MeshInstance3D *> debug_visuals;

	// Corner bits as bit run-length encoding (utils/encoding/rle.h), for save files.
	PackedByteArray serialize_rle() const;
	// Replace the corner bits with the decoded p_data.
	void deserialize_rle(const PackedByteArray &p_data);
};

} // namespace godot

#endif // MC_CHUNK_H
