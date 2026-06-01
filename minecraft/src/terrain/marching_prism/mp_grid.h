/**
 * @file mp_grid.h
 * @brief Handles chunked storage and mesh generation for Marching Prisms terrain.
 * @module src/terrain/marching_prism/mp_grid.h
 * @dependencies godot-cpp, terrain/marching_prism/mp.h
 */

#ifndef MP_GRID_H
#define MP_GRID_H

#include "terrain/marching_prism/mp.h"
#include <cstdint>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/variant/vector3i.hpp>
#include <vector>

namespace godot {

class Camera3D;
class BoxMesh;
class MPNode;
class MeshInstance3D;
class StandardMaterial3D;

/**
 * @struct MPChunk
 * @brief Represents a single 3D chunk of points for the Marching Prisms grid.
 */
struct MPChunk {
	// Horizontal size in x.
	int size_x = 0;
	// Vertical size in y.
	int size_y = 0;
	// Depth size in z.
	int size_z = 0;
	// Chunk coordinate x.
	int loc_x = 0;
	// Chunk coordinate y.
	int loc_y = 0;
	// Chunk coordinate z.
	int loc_z = 0;
	// Packed boolean vertex states.
	std::vector<uint8_t> corner_states;

	// Visual mesh instances for each cell.
	std::vector<MeshInstance3D *> cell_visuals;
	// Debug corner visual indicator instances.
	std::vector<MeshInstance3D *> debug_visuals;

	/**
	 * @brief Computes 1D index from 3D coordinates.
	 */
	inline int get_1d_index(int x, int y, int z) const {
		int nx = size_x + 1;
		int nz = size_z + 1;
		return x + (z * nx) + (y * nx * nz);
	}

	/**
	 * @brief Gets the value of a corner.
	 */
	bool get_corner(int x, int y, int z) const {
		int nx = size_x + 1;
		int ny = size_y + 1;
		int nz = size_z + 1;
		if (x < 0 || x >= nx || y < 0 || y >= ny || z < 0 || z >= nz)
			return false;
		int index = get_1d_index(x, y, z);
		return (corner_states[index / 8] & (1 << (index % 8))) != 0;
	}

	/**
	 * @brief Sets the value of a corner.
	 */
	void set_corner(int x, int y, int z, bool value) {
		int nx = size_x + 1;
		int ny = size_y + 1;
		int nz = size_z + 1;
		if (x < 0 || x >= nx || y < 0 || y >= ny || z < 0 || z >= nz)
			return;
		int index = get_1d_index(x, y, z);
		if (value) {
			corner_states[index / 8] |= (1 << (index % 8));
		} else {
			corner_states[index / 8] &= ~(1 << (index % 8));
		}
	}

	/**
	 * @brief Checks if any neighboring corner is active.
	 */
	bool has_active_neighbor(int x, int y, int z) const {
		int nx = size_x + 1;
		int ny = size_y + 1;
		int nz = size_z + 1;
		if (x > 0 && get_corner(x - 1, y, z))
			return true;
		if (x < nx - 1 && get_corner(x + 1, y, z))
			return true;
		if (y > 0 && get_corner(x, y - 1, z))
			return true;
		if (y < ny - 1 && get_corner(x, y + 1, z))
			return true;
		if (z > 0 && get_corner(x, y, z - 1))
			return true;
		if (z < nz - 1 && get_corner(x, y, z + 1))
			return true;
		return false;
	}

	/**
	 * @brief Checks if any neighboring corner is inactive.
	 */
	bool has_inactive_neighbor(int x, int y, int z) const {
		int nx = size_x + 1;
		int ny = size_y + 1;
		int nz = size_z + 1;
		if (x > 0 && !get_corner(x - 1, y, z))
			return true;
		if (x < nx - 1 && !get_corner(x + 1, y, z))
			return true;
		if (y > 0 && !get_corner(x, y - 1, z))
			return true;
		if (y < ny - 1 && !get_corner(x, y + 1, z))
			return true;
		if (z > 0 && !get_corner(x, y, z - 1))
			return true;
		if (z < nz - 1 && !get_corner(x, y, z + 1))
			return true;
		return false;
	}
};

/**
 * @class MPGrid
 * @brief Node for Marching Prisms terrain representation.
 */
class MPGrid : public Node3D {
	GDCLASS(MPGrid, Node3D)

private:
	// Grid dimensions in chunks.
	Vector3i grid_size = Vector3i(1, 1, 1);
	// Single chunk dimensions in voxels.
	Vector3i chunk_size = Vector3i(6, 6, 6);
	// Reference to MPNode containing mesh catalog.
	MPNode *mp_node = nullptr;
	// Flags whether debug corner shapes are visible.
	bool show_debug_corners = false;
	// List of all chunks in the grid.
	std::vector<MPChunk> chunks;
	// Total count of meshes spawned.
	int total_mp_meshes = 0;
	// Total count of active debug corner meshes.
	int total_debug_corners = 0;
	// Total count of marching cells.
	int total_cells = 0;
	// Parent node for debug spheres/cubes.
	Node3D *debug_corners_container = nullptr;

	// Shared debug mesh for corners.
	Ref<BoxMesh> _debug_box_mesh;
	// Material for inactive corners.
	Ref<StandardMaterial3D> _debug_mat_blue;
	// Material for active corners.
	Ref<StandardMaterial3D> _debug_mat_red;

	Node3D *hover_root = nullptr;
	MeshInstance3D *hover_quads[3] = { nullptr, nullptr, nullptr };
	Ref<StandardMaterial3D> hover_mat_yellow;
	Ref<StandardMaterial3D> hover_mat_white;

	// Retrieves internal 1D chunk list index.
	inline int _get_chunk_index(int x, int y, int z) const {
		return (y * grid_size.x * grid_size.z) + (z * grid_size.x) + x;
	}

	// Updates visual mesh instance at global voxel coord.
	void _update_visual_at(int gx, int gy, int gz);
	// Updates debug corner mesh instance at global voxel coord.
	void _update_debug_at(int gx, int gy, int gz);

	// Cleans up all child mesh instances.
	void _clear_children();
	// Determines if global coordinate lies on a map boundary.
	bool _is_boundary_corner(int gx, int gy, int gz, bool &r_required_state) const;
	// Populates chunk boundary values.
	void _initialize_boundaries(MPChunk &p_chunk) const;
	// Spawns debug visual markers for a chunk.
	int _spawn_debug_cubes(const MPChunk &p_chunk, const Ref<BoxMesh> &p_box_mesh);
	// Spawns Marching Prisms mesh instances for a chunk.
	int _spawn_marching_prisms(const MPChunk &p_chunk, MPNode *p_mp_node);

	void _initialize_hover_previews();

protected:
	// Binds GDScript methods.
	static void _bind_methods();

public:
	MPGrid();
	~MPGrid() override;

	// Sets up chunk list and geometry sizes.
	void initialize_grid(int p_chunks_x, int p_chunks_y, int p_chunks_z, int p_chunk_size_x, int p_chunk_size_y, int p_chunk_size_z, bool p_refresh = true);
	// Rebuilds all chunk meshes.
	void refresh_grid();
	// Changes corner state and schedules visual updates.
	void modify_corner(const Vector3i &p_grid_pos, bool p_active);
	// Queries if global coordinate corner is set active.
	bool is_corner_active(const Vector3i &p_grid_pos) const;
	// Writes grid state to file.
	void save_grid(const String &p_path);
	// Reads grid state from file.
	void load_grid(const String &p_path);

	void update_hover_preview(const Vector3 &p_corner_pos, const Vector3 &p_hit_normal, Camera3D *p_camera);
	void hide_hover_preview();

	// Ready callback.
	void _ready() override;

	// Sets grid size in chunks.
	void set_grid_size(const Vector3i &p_size) { grid_size = p_size; }
	// Gets grid size in chunks.
	Vector3i get_grid_size() const { return grid_size; }

	// Sets chunk dimensions in voxels.
	void set_chunk_size(const Vector3i &p_size) { chunk_size = p_size; }
	// Gets chunk dimensions in voxels.
	Vector3i get_chunk_size() const { return chunk_size; }

	// Gets total meshes spawned.
	int get_total_mp_meshes() const { return total_mp_meshes; }
	// Gets total debug corners active.
	int get_total_debug_corners() const { return total_debug_corners; }
	// Gets total cells.
	int get_total_cells() const { return total_cells; }

	// Sets associated MPNode.
	void set_mp_node(MPNode *p_node) { mp_node = p_node; }
	// Gets associated MPNode.
	MPNode *get_mp_node() const { return mp_node; }

	// Sets debug corners visibility state.
	void set_show_debug_corners(bool p_show);
	// Gets debug corners visibility state.
	bool get_show_debug_corners() const { return show_debug_corners; }

	void set_debug_corners_visible(bool p_visible) {
		if (debug_corners_container) {
			debug_corners_container->set_visible(p_visible);
		}
	}
	bool is_debug_corners_visible() const {
		if (debug_corners_container) {
			return debug_corners_container->is_visible();
		}
		return false;
	}
	void set_corner_collision_enabled(bool p_enabled);
};

} // namespace godot

#endif // MP_GRID_H