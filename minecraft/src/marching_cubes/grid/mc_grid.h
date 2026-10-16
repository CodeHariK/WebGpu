// MCGrid: the editable corner grid, split into chunks, and everything spawned for it.
//   mc_grid.cpp                setup, properties, corner edits, corner/cell queries
//   mc_grid_cells.cpp          the tiles of each chunk merged into one mesh + collider
//   mc_grid_levels.cpp         each cell's level (L / M / H, core/mc_level_plan.h) and Randomize
//   mc_grid_debug_corners.cpp  debug cubes on the corners (+ their picking colliders)
//   mc_grid_hover.cpp          hover preview quads
//   mc_grid_objects.cpp        placed objects (with mc_spatial)
//   mc_grid_io.cpp             save / load
#ifndef MC_GRID_H
#define MC_GRID_H

#include "marching_cubes/core/mc_chunk.h"
#include "marching_cubes/core/mc_grid_dims.h"
#include "marching_cubes/core/mc_level_plan.h"
#include "marching_cubes/grid/mc_chunk_builder.h"
#include "marching_cubes/grid/mc_placed_objects.h"
#include <cstdint>
#include <godot_cpp/classes/mesh.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/vector3i.hpp>
#include <vector>

namespace godot {

class Camera3D;
class BoxMesh;
class MCTileLibrary;
class Mesh;
class MeshInstance3D;
class StandardMaterial3D;

class MCGrid : public Node3D {
	GDCLASS(MCGrid,
			Node3D)

private:
	Vector3i grid_dims = Vector3i(1, 1, 1);
	Vector3i chunk_dims = Vector3i(16, 16, 16);
	MCTileLibrary *tile_library = nullptr;
	bool show_debug_corners = false;
	bool solid_floor = true; // floor corners (y = 0) locked solid; off: editable (random on Randomize)
	std::vector<uint8_t> cell_levels; // per cell (mc::CellBlock order): which level's tile it draws
	std::vector<uint8_t> roof_m; // per cell: roof regions Randomize made M (kept through edits)
	mc::LevelPlan last_plan; // the last Randomize
	mc::LevelPin pin; // one library tile fixed in the grid (Randomize plans around it)
	Ref<Mesh> pin_mesh; // the pinned tile's own mesh, drawn at its cell
	std::vector<Chunk> chunks;
	MCPlacedObjects placed_list; // boxes placed in object mode
	int total_tiles_drawn = 0; // tiles drawn (cells with a mesh)
	int total_chunk_meshes = 0; // merged chunk meshes = draw objects for the tiles
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

	// Index into chunks[] of the chunk at chunk coordinates (x, y, z).
	int _get_chunk_index(
			int x,
			int y,
			int z
	) const {
		return (y * grid_dims.x * grid_dims.z) + (z * grid_dims.x) + x;
	}

	// Mark the chunk owning this cell for a rebuild (after a corner edit).
	void _mark_cell_dirty(
			int gx,
			int gy,
			int gz
	);
	bool rebuild_queued = false; // a deferred rebuild of the dirty chunks is pending
	MCSurfaceCache surface_cache; // tile mesh arrays, read once (cleared when the library changes)
	// Ask for flush_dirty_chunks at the end of the frame (once, however many edits).
	void _queue_rebuild();
	// The grid's cells as a level block (cells per axis).
	mc::CellBlock _cell_block() const;
	// The level of a cell (mc::LEVEL_L / M / H); M outside the grid or before the first update.
	uint8_t _level_at(const Vector3i &p_cell) const;
	// Recompute every cell's level from the corners; cells whose level changed are marked dirty.
	void _update_levels();
	// The pin, while its cell still has the pinned hash (an edit there drops it); else nullptr.
	const mc::LevelPin *_live_pin() const;
	// Write one global corner into every chunk that stores it (no boundary check, no rebuild).
	void _write_corner(
			const Vector3i &p_grid_pos,
			bool p_active
	);
	// Merge every tile of the chunk into one mesh + one collider (replaces the old ones).
	void _build_chunk_mesh(Chunk &p_chunk);
	// Free the chunk's merged mesh node (and its collider) and reset its counters.
	void _free_chunk_mesh(Chunk &p_chunk);
	// Recreate or recolour the debug cube of one global corner after an edit.
	void _update_debug_corner(
			int gx,
			int gy,
			int gz
	);

	// Dimensions + all global <-> chunk-local conversions (bounds-checked): core/mc_grid_dims.h
	MCGridDims _dims() const { return MCGridDims{ grid_dims, chunk_dims }; }

	// Does any of the 6 neighbours of this global corner (inside the grid) have state p_state?
	// Looks across chunk borders, unlike Chunk::has_active_neighbor.
	bool _has_neighbor_with_state(
			const Vector3i &p_corner,
			bool p_state
	) const;

	// Placed objects live under their own container, so refresh_grid() never frees them.
	Node3D *placed_objects_container = nullptr;
	Ref<StandardMaterial3D> placed_object_material; // shared by every placed object
	// Free every placed object and empty the list (the grid was re-initialised).
	void _clear_placed_objects();

	// Free every chunk mesh and debug cube (placed objects and hover quads are kept).
	void _free_spawned_nodes();
	// Is this corner on the grid's outer shell? r_required_state is the state it must keep
	// (bottom layer solid, sides and top empty), so edits there are refused.
	bool _is_boundary_corner(
			int gx,
			int gy,
			int gz,
			bool &r_required_state
	) const;
	// Set the outer-shell corners of a chunk to their required state.
	void _initialize_boundaries(Chunk &p_chunk) const;
	// Create the debug cubes (with picking colliders) for a chunk's corners; returns how many.
	int _spawn_debug_corners(Chunk &p_chunk);

	// Create the three hover quads (one per axis) used to highlight the corner under the mouse.
	void _initialize_hover_previews();

protected:
	// Inspector properties and the methods scripts / MCManager call.
	static void _bind_methods();

public:
	MCGrid();
	~MCGrid() override;

	// Allocate a grid of p_chunks_* chunks of p_chunk_dims_* cells, all corners empty except the
	// solid floor; drops placed objects. p_refresh builds the meshes right away.
	void initialize_grid(
			int p_chunks_x,
			int p_chunks_y,
			int p_chunks_z,
			int p_chunk_dims_x,
			int p_chunk_dims_y,
			int p_chunk_dims_z,
			bool p_refresh = true
	);
	// Rebuild every chunk mesh and debug cube from the corner data.
	void refresh_grid();
	// Rebuild the chunks changed by edits now. Edits queue this for the end of the frame, so many
	// edits in one frame (a brush, a generator) rebuild each chunk once.
	void flush_dirty_chunks();
	// Turn one global corner on or off (outer-shell corners are refused) and queue a rebuild of the
	// chunks whose cells changed.
	void set_corner(
			const Vector3i &p_grid_pos,
			bool p_active
	);
	// Is this global corner solid? Outside the grid counts as empty.
	bool is_corner_active(const Vector3i &p_grid_pos) const;
	// Would a p_size box with its lower corner at cell p_cell overlap solid terrain?
	bool is_area_blocked_by_grid(
			const Vector3i &p_cell,
			const Vector3i &p_size
	) const;
	// Does p_aabb (grid space) overlap any placed object?
	bool is_area_blocked_by_objects(const AABB &p_aabb) const;
	// Place a p_size box at cell p_cell (no checks: call the is_area_blocked_* functions first).
	void add_placed_object(
			const Vector3i &p_cell,
			const Vector3i &p_size
	);
	// Remove a placed object and free its node.
	void remove_placed_object(Node *p_node);
	// Take a placed object out of the list but keep its node (drag start).
	void detach_placed_object(Node *p_node);
	// The placed object whose node is p_node, or nullptr.
	const PlacedObject *get_placed_object(Node *p_node) const;

	// The 8-bit corner hash of the cell at p_pos (0 outside the grid).
	uint8_t get_cell_hash(const Vector3i &p_pos) const;
	// The level of the cell at p_pos: 'l', 'm' or 'h' (as a String for scripts; "" outside).
	String get_cell_level(const Vector3i &p_pos) const;
	// New random corners following the level rules (L bottom layer, H on column tops and beside H,
	// L -> M -> H up a column), the best of p_tries layouts for p_seed; rebuilds the grid. Returns
	// a summary: "seed 3: 120 cells, L 40 M 50 H 30, 0 missing".
	String randomize_levels(
			int p_seed,
			int p_tries = 300
	);
	// Pin one tile (hash, level 'l' / 'm' / 'h', its mesh): its cell is the middle column, in the
	// bottom layer for L, one up for M, the top layer for H. Randomize keeps it and plans around it.
	void set_pin(
			int p_hash,
			const String &p_level,
			const Ref<Mesh> &p_mesh
	);
	// Remove the pin (the cell keeps its corners until the next Randomize).
	void clear_pin();
	// The pinned cell, or (-1, -1, -1) when nothing is pinned.
	Vector3i get_pin_cell() const;
	// The last Randomize as data: hashes / levels (PackedByteArray, x fastest, then y, then z;
	// 0 L, 1 M, 2 H), size, gaps, column_breaks, bottom_solved.
	Dictionary get_last_plan() const;
	// Write the corner grid to a .mct file (format in mc_grid_io.cpp).
	void save_grid(const String &p_path);
	// Read a .mct file; the grid only changes once the whole file has been validated.
	void load_grid(const String &p_path);
	// Every non-empty cell as JSON text, for the Blender add-on's playground ("Paste from Godot").
	String cells_to_json() const;

	// Show the hover quads on the face of the corner under the mouse (p_corner_pos, grid space).
	void update_hover_preview(
			const Vector3 &p_corner_pos,
			const Vector3 &p_hit_normal,
			Camera3D *p_camera
	);
	// Hide the hover quads.
	void hide_hover_preview();
	// Nothing is built here: MCManager calls initialize_grid once the tile library is loaded.
	void _ready() override;

	// Chunks per axis (Inspector; takes effect at the next initialize_grid).
	void set_grid_dims(const Vector3i &p_size);
	// Chunks per axis.
	Vector3i get_grid_dims() const { return grid_dims; }

	// Cells per chunk per axis (Inspector; takes effect at the next initialize_grid).
	void set_chunk_dims(const Vector3i &p_size);
	// Cells per chunk per axis.
	Vector3i get_chunk_dims() const { return chunk_dims; }

	// Floor corners locked solid (default) or editable / random (needed for L ground tiles).
	void set_solid_floor(bool p_solid);
	bool get_solid_floor() const { return solid_floor; }

	// Show or hide the debug corner cubes.
	void set_debug_corners_visible(bool p_visible);
	// Are the debug corner cubes shown?
	bool is_debug_corners_visible() const;
	// Turn the debug cubes' picking colliders on or off.
	void set_corner_collision_enabled(bool p_enabled);

	// Stats for the editor panel.
	int get_total_tiles_drawn() const { return total_tiles_drawn; }
	// Merged chunk meshes (one draw object each).
	int get_total_chunk_meshes() const { return total_chunk_meshes; }
	// Debug corner cubes spawned.
	int get_total_debug_corners() const { return total_debug_corners; }
	// Cells in the whole grid.
	int get_total_cells() const { return total_cells; }

	// The tile library that supplies a mesh for every cell hash.
	void set_tile_library(MCTileLibrary *p_node);
	// The tile library in use.
	MCTileLibrary *get_tile_library() const;

	// Inspector property behind set_debug_corners_visible.
	void set_show_debug_corners(bool p_show);
	// Inspector property: debug corner cubes shown?
	bool get_show_debug_corners() const;
};

} // namespace godot

#endif // MC_GRID_H
