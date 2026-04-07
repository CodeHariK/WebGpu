#ifndef MC_TERRAIN_H
#define MC_TERRAIN_H

#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/variant/vector3i.hpp>
#include <cstdint>
#include <vector>

namespace godot {

class FastNoiseLite;
class BoxMesh;
class MCNode;

struct Chunk {
	int size_x = 0;
	int size_y = 0;
	int size_z = 0;
	int loc_x = 0;
	int loc_y = 0;
	int loc_z = 0;
	std::vector<uint8_t> corner_states; // Bit-packed states

	bool get_corner_bit(int p_index) const {
		if (corner_states.empty()) {
			return false;
		}
		return (corner_states[static_cast<size_t>(p_index / 8)] & (1 << (p_index % 8))) != 0;
	}

	void set_corner_bit(int p_index, bool p_state) {
		if (p_state) {
			corner_states[static_cast<size_t>(p_index / 8)] |= (1 << (p_index % 8));
		} else {
			corner_states[static_cast<size_t>(p_index / 8)] &= ~(1 << (p_index % 8));
		}
	}

	bool get_corner(int x, int y, int z) const {
		int nx = size_x + 1;
		int nz = size_z + 1;
		int index = (y * nx * nz) + (z * nx) + x;
		return get_corner_bit(index);
	}

	void set_corner(int x, int y, int z, bool value) {
		int nx = size_x + 1;
		int nz = size_z + 1;
		int index = (y * nx * nz) + (z * nx) + x;
		set_corner_bit(index, value);
	}

	uint8_t get_cell_hash(int x, int y, int z) const {
		uint8_t hash = 0;
		// Mapping matches MCNode standards:
		// c0: x, y, z+1
		if (get_corner(x, y, z + 1)) {
			hash |= (1 << 7);
		}
		// c1: x+1, y, z+1
		if (get_corner(x + 1, y, z + 1)) {
			hash |= (1 << 6);
		}
		// c2: x+1, y, z
		if (get_corner(x + 1, y, z)) {
			hash |= (1 << 5);
		}
		// c3: x, y, z
		if (get_corner(x, y, z)) {
			hash |= (1 << 4);
		}
		// c4: x, y+1, z+1
		if (get_corner(x, y + 1, z + 1)) {
			hash |= (1 << 3);
		}
		// c5: x+1, y+1, z+1
		if (get_corner(x + 1, y + 1, z + 1)) {
			hash |= (1 << 2);
		}
		// c6: x+1, y+1, z
		if (get_corner(x + 1, y + 1, z)) {
			hash |= (1 << 1);
		}
		// c7: x, y+1, z
		if (get_corner(x, y + 1, z)) {
			hash |= (1 << 0);
		}
		return hash;
	}

	bool has_active_neighbor(int x, int y, int z) const {
		if (x > 0 && get_corner(x - 1, y, z)) return true;
		if (x < size_x && get_corner(x + 1, y, z)) return true;
		if (y > 0 && get_corner(x, y - 1, z)) return true;
		if (y < size_y && get_corner(x, y + 1, z)) return true;
		if (z > 0 && get_corner(x, y, z - 1)) return true;
		if (z < size_z && get_corner(x, y, z + 1)) return true;
		return false;
	}

	bool has_inactive_neighbor(int x, int y, int z) const {
		if (x > 0 && !get_corner(x - 1, y, z)) return true;
		if (x < size_x && !get_corner(x + 1, y, z)) return true;
		if (y > 0 && !get_corner(x, y - 1, z)) return true;
		if (y < size_y && !get_corner(x, y + 1, z)) return true;
		if (z > 0 && !get_corner(x, y, z - 1)) return true;
		if (z < size_z && !get_corner(x, y, z + 1)) return true;
		return false;
	}
};

class MCTerrain : public Node3D {
	GDCLASS(MCTerrain, Node3D)

private:
	Vector3i terrain_size = Vector3i(1, 1, 1);
	Vector3i chunk_size = Vector3i(16, 16, 16);
	MCNode *mc_node = nullptr;
	Ref<FastNoiseLite> noise;
	float noise_threshold = 0.0f;
	bool show_debug_corners = false;
	std::vector<Chunk> chunks;
	int total_mc_meshes = 0;
	int total_debug_corners = 0;
	int total_cells = 0;
	Node3D *debug_corners_container = nullptr;

	void _clear_children();
	bool _is_boundary_corner(int gx, int gy, int gz, bool &r_required_state) const;
	void _sample_chunk_noise(Chunk &p_chunk, const Ref<FastNoiseLite> &p_noise, float p_threshold) const;
	int _spawn_debug_cubes(const Chunk &p_chunk, const Ref<BoxMesh> &p_box_mesh);
	int _spawn_marching_cubes(const Chunk &p_chunk, MCNode *p_mc_node);
	void _on_noise_changed();

protected:
	static void _bind_methods();

public:
	MCTerrain();
	~MCTerrain() override;

	void initialize_terrain(int p_chunks_x, int p_chunks_y, int p_chunks_z, int p_chunk_size_x, int p_chunk_size_y, int p_chunk_size_z);
	void initialize_terrain_with_noise(int p_chunks_x, int p_chunks_y, int p_chunks_z, int p_chunk_size_x, int p_chunk_size_y, int p_chunk_size_z, float p_threshold = 0.0);
	void generate_with_noise();
	void refresh_terrain();
	void modify_corner(const Vector3i &p_grid_pos, bool p_active);
	void _ready() override;

	void set_terrain_size(const Vector3i &p_size);
	Vector3i get_terrain_size() const {
		return terrain_size;
	}

	void set_chunk_size(const Vector3i &p_size);
	Vector3i get_chunk_size() const {
		return chunk_size;
	}

	void set_noise(const Ref<FastNoiseLite> &p_noise);
	Ref<FastNoiseLite> get_noise() const;

	void set_noise_threshold(float p_threshold);
	float get_noise_threshold() const {
		return noise_threshold;
	}

	void set_debug_corners_visible(bool p_visible);
	bool is_debug_corners_visible() const;


	void set_trigger_test_grid(bool p_trigger);
	bool get_trigger_test_grid() const {
		return false;
	}

	int get_total_mc_meshes() const {
		return total_mc_meshes;
	}
	int get_total_debug_corners() const {
		return total_debug_corners;
	}
	int get_total_cells() const {
		return total_cells;
	}


	void set_mc_node(MCNode *p_node);
	MCNode *get_mc_node() const;

	void set_show_debug_corners(bool p_show);
	bool get_show_debug_corners() const {
		return show_debug_corners;
	}

	void spawn_test_grid();
};

} // namespace godot

#endif // MC_TERRAIN_H
