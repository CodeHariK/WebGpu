#ifndef SN_GRID_H
#define SN_GRID_H

#include <cstdint>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/vector3i.hpp>
#include <vector>

namespace godot {

class MeshInstance3D;
class StaticBody3D;
class Material;

struct SNChunk {
	int size_x = 0;
	int size_y = 0;
	int size_z = 0;
	int loc_x = 0;
	int loc_y = 0;
	int loc_z = 0;
	std::vector<int8_t> corner_densities; // 8-bit signed density/SDF values (-128 to 127)

	MeshInstance3D *visual_node = nullptr;
	StaticBody3D *collision_body = nullptr;

	int8_t get_corner(int x, int y, int z) const {
		int nx = size_x + 1;
		int nz = size_z + 1;
		int index = (y * nx * nz) + (z * nx) + x;
		return corner_densities[index];
	}

	void set_corner(int x, int y, int z, int8_t value) {
		int nx = size_x + 1;
		int nz = size_z + 1;
		int index = (y * nx * nz) + (z * nx) + x;
		corner_densities[index] = value;
	}

	bool is_corner_active(int x, int y, int z) const {
		return get_corner(x, y, z) < 0; // Negative values are inside/solid
	}

	PackedByteArray serialize_rle() const;
	void deserialize_rle(const PackedByteArray &p_data);
};

class SNGrid : public Node3D {
	GDCLASS(SNGrid, Node3D)

private:
	Vector3i grid_size = Vector3i(1, 1, 1);
	Vector3i chunk_size = Vector3i(16, 16, 16);
	std::vector<SNChunk> chunks;
	Ref<Material> terrain_material;

	int _get_chunk_index(int x, int y, int z) const {
		return (y * grid_size.x * grid_size.z) + (z * grid_size.x) + x;
	}

	bool _is_boundary_corner(int gx, int gy, int gz, int8_t &r_required_density) const;
	void _initialize_boundaries(SNChunk &p_chunk) const;

	void _update_chunk_mesh(int p_chunk_idx);
	void _clear_chunk_visuals(SNChunk &p_chunk);

protected:
	static void _bind_methods();

public:
	SNGrid();
	~SNGrid() override;

	void _ready() override;

	void initialize_grid(int p_chunks_x, int p_chunks_y, int p_chunks_z, int p_chunk_size_x, int p_chunk_size_y, int p_chunk_size_z, bool p_refresh = true);
	void refresh_grid();

	void modify_density(const Vector3i &p_grid_pos, int8_t p_density);
	int8_t get_density(const Vector3i &p_grid_pos) const;
	bool is_solid(const Vector3i &p_grid_pos) const;

	void save_grid(const String &p_path);
	void load_grid(const String &p_path);

	void set_grid_size(const Vector3i &p_size);
	Vector3i get_grid_size() const {
		return grid_size;
	}

	void set_chunk_size(const Vector3i &p_size);
	Vector3i get_chunk_size() const {
		return chunk_size;
	}

	void set_terrain_material(const Ref<Material> &p_material);
	Ref<Material> get_terrain_material() const;

	void generate_test_sdf();
};

} // namespace godot

#endif // SN_GRID_H
