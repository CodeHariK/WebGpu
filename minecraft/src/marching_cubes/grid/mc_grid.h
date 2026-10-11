// MCGrid: the editable corner grid, split into chunks, and everything spawned for it.
//   mc_grid.cpp                setup, properties, corner edits, corner/cell queries
//   mc_grid_cells.cpp          tile meshes for the cells (spawn, update one, clear)
//   mc_grid_debug_corners.cpp  debug cubes on the corners (+ their picking colliders)
//   mc_grid_hover.cpp          hover preview quads
//   mc_grid_objects.cpp        placed objects (with mc_spatial)
//   mc_grid_io.cpp             save / load
#ifndef MC_GRID_H
#define MC_GRID_H

#include "marching_cubes/core/mc_chunk.h"
#include "marching_cubes/core/mc_grid_dims.h"
#include "marching_cubes/grid/mc_spatial.h"
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

class MCGrid : public Node3D {
	GDCLASS(MCGrid,
			Node3D)

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

	int _get_chunk_index(
			int x,
			int y,
			int z
	) const {
		return (y * grid_size.x * grid_size.z) + (z * grid_size.x) + x;
	}

	void _update_visual_at(
			int gx,
			int gy,
			int gz
	);
	void _update_debug_at(
			int gx,
			int gy,
			int gz
	);

	// Dimensions + all global <-> chunk-local conversions (bounds-checked): core/mc_grid_dims.h
	MCGridDims _dims() const { return MCGridDims{ grid_size, chunk_size }; }

	// Does any of the 6 neighbours of this global corner (inside the grid) have state p_state?
	// Looks across chunk borders, unlike Chunk::has_active_neighbor.
	bool _has_neighbor_with_state(
			const Vector3i &p_corner,
			bool p_state
	) const;

	// Placed objects live under their own container, so refresh_grid() never frees them.
	Node3D *placed_objects_container = nullptr;
	Ref<StandardMaterial3D> placed_object_material; // shared by every placed object
	void _clear_placed_objects();

	void _clear_children();
	bool _is_boundary_corner(
			int gx,
			int gy,
			int gz,
			bool &r_required_state
	) const;
	void _initialize_boundaries(Chunk &p_chunk) const;
	int _spawn_debug_cubes(Chunk &p_chunk);
	int _spawn_marching_cubes(
			const Chunk &p_chunk,
			MCNode *p_mc_node
	);

	void _initialize_hover_previews();

protected:
	static void _bind_methods();

public:
	MCGrid();
	~MCGrid() override;

	void initialize_grid(
			int p_chunks_x,
			int p_chunks_y,
			int p_chunks_z,
			int p_chunk_size_x,
			int p_chunk_size_y,
			int p_chunk_size_z,
			bool p_refresh = true
	);
	void refresh_grid();
	void modify_corner(
			const Vector3i &p_grid_pos,
			bool p_active
	);
	bool is_corner_active(const Vector3i &p_grid_pos) const;
	bool is_area_blocked_by_grid(
			const Vector3i &p_dual_grid_pos,
			const Vector3i &p_size
	) const;
	bool is_area_blocked_by_objects(const AABB &p_aabb) const;
	void add_placed_object(
			const Vector3i &p_dual_grid_pos,
			const Vector3i &p_size
	);
	void remove_placed_object(Node *p_node);
	void detach_placed_object(Node *p_node);
	const PlacedObject *get_placed_object(Node *p_node) const;

	uint8_t get_cell_hash_at_global_coord(const Vector3i &p_pos) const;
	void save_grid(const String &p_path);
	void load_grid(const String &p_path);

	void update_hover_preview(
			const Vector3 &p_corner_pos,
			const Vector3 &p_hit_normal,
			Camera3D *p_camera
	);
	void hide_hover_preview();
	void _ready() override;

	void set_grid_size(const Vector3i &p_size);
	Vector3i get_grid_size() const { return grid_size; }

	void set_chunk_size(const Vector3i &p_size);
	Vector3i get_chunk_size() const { return chunk_size; }

	void set_debug_corners_visible(bool p_visible);
	bool is_debug_corners_visible() const;
	void set_corner_collision_enabled(bool p_enabled);

	int get_total_mc_meshes() const { return total_mc_meshes; }
	int get_total_debug_corners() const { return total_debug_corners; }
	int get_total_cells() const { return total_cells; }

	void set_mc_node(MCNode *p_node);
	MCNode *get_mc_node() const;

	void set_show_debug_corners(bool p_show);
	bool get_show_debug_corners() const;
};

} // namespace godot

#endif // MC_GRID_H
