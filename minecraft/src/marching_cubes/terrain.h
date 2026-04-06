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
		if (corner_states.empty()) return false;
		return (corner_states[static_cast<size_t>(p_index / 8)] & (1 << (p_index % 8))) != 0;
	}

	void set_corner_bit(int p_index, bool p_state) {
		if (p_state) {
			corner_states[static_cast<size_t>(p_index / 8)] |= (1 << (p_index % 8));
		} else {
			corner_states[static_cast<size_t>(p_index / 8)] &= ~(1 << (p_index % 8));
		}
	}
};

class MCTerrain : public Node3D {
	GDCLASS(MCTerrain, Node3D)

private:
	Vector3i terrain_size = Vector3i(1, 1, 1);
	Vector3i chunk_size = Vector3i(16, 16, 16);
	NodePath mc_node_path;
	bool use_marching_cubes = false;
	Ref<FastNoiseLite> noise;
	float noise_threshold = 0.0f;
	bool trigger_generation = false;
	std::vector<Chunk> chunks;

	void _clear_children();
	void _sample_chunk_noise(Chunk &p_chunk, const Ref<FastNoiseLite> &p_noise, float p_threshold);
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
	void _ready() override;

	void set_terrain_size(const Vector3i &p_size);
	Vector3i get_terrain_size() const { return terrain_size; }

	void set_chunk_size(const Vector3i &p_size);
	Vector3i get_chunk_size() const { return chunk_size; }

	void set_noise(const Ref<FastNoiseLite> &p_noise);
	Ref<FastNoiseLite> get_noise() const;

	void set_noise_threshold(float p_threshold);
	float get_noise_threshold() const { return noise_threshold; }

	void set_trigger_generation(bool p_trigger);
	bool get_trigger_generation() const { return false; }

	void set_use_marching_cubes(bool p_use);
	bool get_use_marching_cubes() const { return use_marching_cubes; }

	void set_mc_node_path(const NodePath &p_path);
	NodePath get_mc_node_path() const { return mc_node_path; }
};

} // namespace godot

#endif // MC_TERRAIN_H
