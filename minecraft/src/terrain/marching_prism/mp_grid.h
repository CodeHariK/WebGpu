/**
 * @file mp_grid.h
 * @brief Manages the grid and chunk structure for the Marching Prism terrain generator.
 * Module Path: src/terrain/marching_prism/mp_grid.h
 * Build Dependencies: godot-cpp, mp.h
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
class SphereMesh;
class CylinderMesh;
class MPNode;
class MeshInstance3D;
class StandardMaterial3D;
class StaticBody3D;

struct MPChunk {
	int size_x = 0;
	int size_y = 0;
	int size_z = 0;
	int loc_x = 0;
	int loc_y = 0;
	int loc_z = 0;
	std::vector<uint8_t> corner_states;

	std::vector<MeshInstance3D *> cell_visuals;
	std::vector<StaticBody3D *> cell_colliders; // Tracks center of prism colliders
	std::vector<MeshInstance3D *> debug_visuals;

	inline int get_1d_index(int x, int y, int z) const {
		int nx = size_x + 1;
		int nz = size_z + 1;
		return x + (z * nx) + (y * nx * nz);
	}

	bool get_corner(int x, int y, int z) const {
		int nx = size_x + 1;
		int ny = size_y + 1;
		int nz = size_z + 1;
		if (x < 0 || x >= nx || y < 0 || y >= ny || z < 0 || z >= nz)
			return false;
		int index = get_1d_index(x, y, z);
		return (corner_states[index / 8] & (1 << (index % 8))) != 0;
	}

	void set_corner(int x, int y, int z, bool value) {
		int nx = size_x + 1;
		int ny = size_y + 1;
		int nz = size_z + 1;
		if (x < 0 || x >= nx || y < 0 || y >= ny || z < 0 || z >= nz)
			return;
		int index = get_1d_index(x, y, z);
		if (value)
			corner_states[index / 8] |= (1 << (index % 8));
		else
			corner_states[index / 8] &= ~(1 << (index % 8));
	}

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

class MPGrid : public Node3D {
	GDCLASS(MPGrid, Node3D)

private:
	Vector3i grid_size = Vector3i(1, 1, 1);
	Vector3i chunk_size = Vector3i(6, 6, 6);
	MPNode *mp_node = nullptr;
	bool show_debug_corners = false;
	std::vector<MPChunk> chunks;
	int total_mp_meshes = 0;
	int total_debug_corners = 0;
	int total_cells = 0;
	Node3D *debug_corners_container = nullptr;

	Ref<SphereMesh> _debug_corner_mesh; // Mesh instance used to draw debug corner spheres
	Ref<StandardMaterial3D> _debug_mat_blue; // Material for inactive debug corner spheres
	Ref<StandardMaterial3D> _debug_mat_red; // Material for active debug corner spheres

	Node3D *hover_root = nullptr;
	MeshInstance3D *hover_quads[3] = { nullptr, nullptr, nullptr };
	Ref<StandardMaterial3D> hover_mat_yellow;
	Ref<StandardMaterial3D> hover_mat_white;
	MeshInstance3D *hover_cylinder = nullptr; // Cylinder mesh representing the vertex click zone
	float hover_cylinder_alpha = 0.5f; // Opacity of the hover cylinder visual preview.
	MeshInstance3D *wireframe_instance = nullptr; // Renders all triangular prism edges in the grid.

	inline int _get_chunk_index(int x, int y, int z) const {
		return (y * grid_size.x * grid_size.z) + (z * grid_size.x) + x;
	}

	void _update_visual_at(int gx, int gy, int gz);
	// Updates the debug visualization (sphere) at a specific global corner coordinate.
	void _update_debug_at(int gx, int gy, int gz);
	void _clear_children();
	bool _is_boundary_corner(int gx, int gy, int gz, bool &r_required_state) const;
	void _initialize_boundaries(MPChunk &p_chunk) const;
	// Spawns sphere debug visuals at the corners of a chunk for visualization/debugging.
	int _spawn_debug_spheres(const MPChunk &p_chunk, const Ref<SphereMesh> &p_sphere_mesh);
	int _spawn_marching_prisms(const MPChunk &p_chunk, MPNode *p_mp_node);
	void _initialize_hover_previews();
	// Gets the world position of a specific local corner coordinate in a chunk.
	Vector3 _get_corner_world_pos(const MPChunk &p_chunk, int lx, int ly, int lz) const;
	// Builds/rebuilds the wireframe mesh instance representing the edges of all prisms.
	void _build_grid_wireframe();

protected:
	static void _bind_methods();

public:
	MPGrid();
	~MPGrid() override;

	void initialize_grid(int p_chunks_x, int p_chunks_y, int p_chunks_z, int p_chunk_size_x, int p_chunk_size_y, int p_chunk_size_z, bool p_refresh = true);
	// Refreshes the grid by spawning marching prisms and debug spheres.
	void refresh_grid();
	void modify_corner(const Vector3i &p_grid_pos, bool p_active);
	bool is_corner_active(const Vector3i &p_grid_pos) const;
	void save_grid(const String &p_path);
	void load_grid(const String &p_path);

	// Updates the hover preview visual (snapped face quad and/or cylinder).
	void update_hover_preview(const Vector3 &p_corner_pos, const Vector3 &p_hit_normal, Camera3D *p_camera, bool p_is_cell = false);
	void hide_hover_preview();

	void _ready() override;

	void set_grid_size(const Vector3i &p_size) { grid_size = p_size; }
	Vector3i get_grid_size() const { return grid_size; }
	void set_chunk_size(const Vector3i &p_size) { chunk_size = p_size; }
	Vector3i get_chunk_size() const { return chunk_size; }
	int get_total_mp_meshes() const { return total_mp_meshes; }
	int get_total_debug_corners() const { return total_debug_corners; }
	int get_total_cells() const { return total_cells; }
	void set_mp_node(MPNode *p_node) { mp_node = p_node; }
	MPNode *get_mp_node() const { return mp_node; }
	void set_show_debug_corners(bool p_show);
	bool get_show_debug_corners() const { return show_debug_corners; }
	void set_debug_corners_visible(bool p_visible) {
		if (debug_corners_container)
			debug_corners_container->set_visible(p_visible);
	}
	bool is_debug_corners_visible() const {
		if (debug_corners_container)
			return debug_corners_container->is_visible();
		return false;
	}
	void set_corner_collision_enabled(bool p_enabled);
	// Enables or disables physics collision for marching prism cell/center colliders.
	void set_cell_collision_enabled(bool p_enabled);

	// Sets the opacity (alpha) of the hover cylinder visual preview.
	void set_hover_cylinder_alpha(float p_alpha);
	// Gets the opacity (alpha) of the hover cylinder visual preview.
	float get_hover_cylinder_alpha() const;
};

} // namespace godot

#endif // MP_GRID_H
