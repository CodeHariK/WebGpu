
#ifndef PRISM_GRID_3D_H
#define PRISM_GRID_3D_H

#include <cmath>
#include <memory>
#include <unordered_map>

namespace godot {

// A simple 3D Vector struct for our hash map keys
// (If you are doing this in Godot GDExtension, replace this with Godot's Vector3i)
struct Vector3Int {
	int x, y, z;
	bool operator==(const Vector3Int &o) const { return x == o.x && y == o.y && z == o.z; }
};

// Custom hash function for the 3D Vector
struct Vector3IntHash {
	std::size_t operator()(const Vector3Int &k) const {
		return ((std::hash<int>()(k.x) ^ (std::hash<int>()(k.y) << 1)) >> 1) ^ (std::hash<int>()(k.z) << 1);
	}
};

class PrismChunk3D {
private:
	// Visual size is 6. Memory size is 7 (Overlap)
	static const int CHUNK_SIZE = 6;
	static const int MEM_SIZE = 7;

	std::vector<uint8_t> bit_mask;

	// Fast 3D to 1D index mapping
	inline int get_1d_index(int x, int y, int z) const {
		return x + (z * MEM_SIZE) + (y * MEM_SIZE * MEM_SIZE);
	}

public:
	PrismChunk3D() {
		int required_bytes = ((MEM_SIZE * MEM_SIZE * MEM_SIZE) + 7) / 8;
		bit_mask.assign(required_bytes, 0);
	}

	// Set vertex (Local coordinates 0 to 6)
	void set_vertex(int x, int y, int z, bool is_solid) {
		if (x < 0 || x >= MEM_SIZE || y < 0 || y >= MEM_SIZE || z < 0 || z >= MEM_SIZE)
			return;

		int index = get_1d_index(x, y, z);
		int byte_index = index / 8;
		int bit_offset = index % 8;

		if (is_solid)
			bit_mask[byte_index] |= (1 << bit_offset);
		else
			bit_mask[byte_index] &= ~(1 << bit_offset);
	}

	// Get vertex
	bool get_vertex(int x, int y, int z) const {
		if (x < 0 || x >= MEM_SIZE || y < 0 || y >= MEM_SIZE || z < 0 || z >= MEM_SIZE)
			return false;

		int index = get_1d_index(x, y, z);
		return (bit_mask[index / 8] & (1 << (index % 8))) != 0;
	}
};

class PrismGrid3D {
private:
	static const int CHUNK_SIZE = 6;

	// Dictionary mapping 3D chunk coordinates to actual Chunk objects
	std::unordered_map<Vector3Int, std::shared_ptr<PrismChunk3D>, Vector3IntHash> chunks;

	// Helper: Safely initialize a chunk if it doesn't exist, then set the vertex
	void _safe_set(int cx, int cy, int cz, int lx, int ly, int lz, bool is_solid) {
		Vector3Int key = { cx, cy, cz };

		if (chunks.find(key) == chunks.end()) {
			chunks[key] = std::make_shared<PrismChunk3D>();
		}

		chunks[key]->set_vertex(lx, ly, lz, is_solid);
	}

public:
	PrismGrid3D() = default;

	// --- MODIFY THE WORLD ---
	void set_global_vertex(int world_x, int world_y, int world_z, bool is_solid) {
		// 1. Calculate World to Chunk Coordinates
		// (std::floor correctly handles negative world coordinates)
		int cx = (int)std::floor((float)world_x / CHUNK_SIZE);
		int cy = (int)std::floor((float)world_y / CHUNK_SIZE);
		int cz = (int)std::floor((float)world_z / CHUNK_SIZE);

		// 2. Calculate Local Coordinates inside that chunk (0 to 5)
		int lx = world_x - (cx * CHUNK_SIZE);
		int ly = world_y - (cy * CHUNK_SIZE);
		int lz = world_z - (cz * CHUNK_SIZE);

		// 3. Update the Primary Chunk
		_safe_set(cx, cy, cz, lx, ly, lz, is_solid);

		// 4. THE 3D OVERLAP RULE
		// If a vertex lies on the left/bottom/back boundary (0),
		// we must duplicate it to the right/top/front boundary (6) of neighbor chunks.

		// Shared Faces (2 chunks share this vertex)
		if (lx == 0)
			_safe_set(cx - 1, cy, cz, CHUNK_SIZE, ly, lz, is_solid);
		if (ly == 0)
			_safe_set(cx, cy - 1, cz, lx, CHUNK_SIZE, lz, is_solid);
		if (lz == 0)
			_safe_set(cx, cy, cz - 1, lx, ly, CHUNK_SIZE, is_solid);

		// Shared Edges (4 chunks share this vertex)
		if (lx == 0 && ly == 0)
			_safe_set(cx - 1, cy - 1, cz, CHUNK_SIZE, CHUNK_SIZE, lz, is_solid);
		if (lx == 0 && lz == 0)
			_safe_set(cx - 1, cy, cz - 1, CHUNK_SIZE, ly, CHUNK_SIZE, is_solid);
		if (ly == 0 && lz == 0)
			_safe_set(cx, cy - 1, cz - 1, lx, CHUNK_SIZE, CHUNK_SIZE, is_solid);

		// Shared Corner (8 chunks share this vertex)
		if (lx == 0 && ly == 0 && lz == 0) {
			_safe_set(cx - 1, cy - 1, cz - 1, CHUNK_SIZE, CHUNK_SIZE, CHUNK_SIZE, is_solid);
		}
	}

	// --- READ THE WORLD ---
	bool get_global_vertex(int world_x, int world_y, int world_z) const {
		int cx = (int)std::floor((float)world_x / CHUNK_SIZE);
		int cy = (int)std::floor((float)world_y / CHUNK_SIZE);
		int cz = (int)std::floor((float)world_z / CHUNK_SIZE);

		Vector3Int key = { cx, cy, cz };

		// If the chunk hasn't been generated yet, treat it as empty air
		auto it = chunks.find(key);
		if (it == chunks.end()) {
			return false;
		}

		int lx = world_x - (cx * CHUNK_SIZE);
		int ly = world_y - (cy * CHUNK_SIZE);
		int lz = world_z - (cz * CHUNK_SIZE);

		return it->second->get_vertex(lx, ly, lz);
	}

	// --- ACCESS CHUNK FOR MESH GENERATION ---
	std::shared_ptr<PrismChunk3D> get_chunk(int cx, int cy, int cz) {
		Vector3Int key = { cx, cy, cz };
		if (chunks.find(key) != chunks.end()) {
			return chunks[key];
		}
		return nullptr;
	}
};

} //namespace godot

#endif