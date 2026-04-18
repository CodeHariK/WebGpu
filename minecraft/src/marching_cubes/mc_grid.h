#ifndef MC_GRID_H
#define MC_GRID_H

#include "marching_cubes/mc_spatial.h"
#include "utils/encoding/rle.h"
#include <cstdint>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/vector3i.hpp>
#include <vector>

namespace godot {

class Camera3D;
class BoxMesh;
class MCNode;
class MeshInstance3D;
class StandardMaterial3D;

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

	bool has_active_neighbor(int x, int y, int z) const {
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

	bool has_inactive_neighbor(int x, int y, int z) const {
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

	std::vector<MeshInstance3D*> cell_visuals;
	std::vector<MeshInstance3D*> debug_visuals;

	PackedByteArray serialize_rle() const;
	void deserialize_rle(const PackedByteArray &p_data);
};

class MCGrid : public Node3D {
	GDCLASS(MCGrid, Node3D)

private:
	Vector3i grid_size = Vector3i(1, 1, 1);
	Vector3i chunk_size = Vector3i(16, 16, 16);
	MCNode *mc_node = nullptr;
	bool show_debug_corners = false;
	std::vector<Chunk> chunks;
	MCSpatial spatial;
	int total_mc_meshes = 0;
	int total_debug_corners = 0;
	int total_cells = 0;
	Node3D *debug_corners_container = nullptr;

	Ref<BoxMesh> _debug_box_mesh;
	Ref<StandardMaterial3D> _debug_mat_blue;
	Ref<StandardMaterial3D> _debug_mat_red;
	Node3D *hover_root = nullptr;
	MeshInstance3D *hover_quads[3] = { nullptr, nullptr, nullptr };
	Ref<StandardMaterial3D> hover_mat_yellow;
	Ref<StandardMaterial3D> hover_mat_white;

	int _get_chunk_index(int x, int y, int z) const {
		return (y * grid_size.x * grid_size.z) + (z * grid_size.x) + x;
	}

	void _update_visual_at(int gx, int gy, int gz);
	void _update_debug_at(int gx, int gy, int gz);

	void _clear_children();
	bool _is_boundary_corner(int gx, int gy, int gz, bool &r_required_state) const;
	void _initialize_boundaries(Chunk &p_chunk) const;
	int _spawn_debug_cubes(const Chunk &p_chunk, const Ref<BoxMesh> &p_box_mesh);
	int _spawn_marching_cubes(const Chunk &p_chunk, MCNode *p_mc_node);

	void _initialize_hover_previews();

protected:
	static void _bind_methods();

public:
	MCGrid();
	~MCGrid() override;

	void initialize_grid(int p_chunks_x, int p_chunks_y, int p_chunks_z, int p_chunk_size_x, int p_chunk_size_y, int p_chunk_size_z, bool p_refresh = true);
	void refresh_grid();
	void modify_corner(const Vector3i &p_grid_pos, bool p_active);
	bool is_corner_active(const Vector3i &p_grid_pos) const;
	bool is_area_blocked_by_grid(const Vector3i &p_dual_grid_pos, const Vector3i &p_size) const;
	bool is_area_blocked_by_objects(const AABB &p_aabb) const;
	void add_placed_object(const Vector3i &p_dual_grid_pos, const Vector3i &p_size);
	void remove_placed_object(Node *p_node);
	void detach_placed_object(Node *p_node);
	const PlacedObject *get_placed_object(Node *p_node) const;

	uint8_t get_cell_hash_at_global_coord(const Vector3i &p_pos) const;
	void save_grid(const String &p_path);
	void load_grid(const String &p_path);

	void update_hover_preview(const Vector3 &p_corner_pos, const Vector3 &p_hit_normal, Camera3D *p_camera);
	void hide_hover_preview();
	void _ready() override;

	void set_grid_size(const Vector3i &p_size);
	Vector3i get_grid_size() const {
		return grid_size;
	}

	void set_chunk_size(const Vector3i &p_size);
	Vector3i get_chunk_size() const {
		return chunk_size;
	}

	void set_debug_corners_visible(bool p_visible);
	bool is_debug_corners_visible() const;
	void set_corner_collision_enabled(bool p_enabled);

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
	bool get_show_debug_corners() const;
};

} // namespace godot

#endif // MC_GRID_H
