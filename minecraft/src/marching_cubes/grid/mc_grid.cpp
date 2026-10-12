// MCGrid: setup, properties, corner edits and corner/cell queries.
#include "marching_cubes/grid/mc_grid.h"
#include "marching_cubes/library/mc_library.h"
#include <godot_cpp/classes/box_mesh.hpp>
#include <godot_cpp/classes/box_shape3d.hpp>
#include <godot_cpp/classes/collision_shape3d.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/geometry_instance3d.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/scroll_container.hpp>
#include <godot_cpp/classes/shader_material.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/static_body3d.hpp>
#include <godot_cpp/classes/v_box_container.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

// Inspector properties and the methods scripts / MCManager call.
void MCGrid::_bind_methods() {
	ClassDB::bind_method(
			D_METHOD(
					"initialize_grid", "chunks_x", "chunks_y", "chunks_z", "chunk_dims_x", "chunk_dims_y",
					"chunk_dims_z", "refresh"
			),
			&MCGrid::initialize_grid, DEFVAL(true)
	);
	ClassDB::bind_method(D_METHOD("refresh_grid"), &MCGrid::refresh_grid);
	ClassDB::bind_method(D_METHOD("flush_dirty_chunks"), &MCGrid::flush_dirty_chunks);
	ClassDB::bind_method(D_METHOD("set_corner", "p_grid_pos", "p_active"), &MCGrid::set_corner);

	ClassDB::bind_method(D_METHOD("set_grid_dims", "size"), &MCGrid::set_grid_dims);
	ClassDB::bind_method(D_METHOD("get_grid_dims"), &MCGrid::get_grid_dims);
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3I, "grid_dims"), "set_grid_dims", "get_grid_dims");

	ClassDB::bind_method(D_METHOD("set_chunk_dims", "size"), &MCGrid::set_chunk_dims);
	ClassDB::bind_method(D_METHOD("get_chunk_dims"), &MCGrid::get_chunk_dims);
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3I, "chunk_dims"), "set_chunk_dims", "get_chunk_dims");

	ClassDB::bind_method(D_METHOD("set_show_debug_corners", "show"), &MCGrid::set_show_debug_corners);
	ClassDB::bind_method(D_METHOD("get_show_debug_corners"), &MCGrid::get_show_debug_corners);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "show_debug_corners"), "set_show_debug_corners", "get_show_debug_corners");

	ClassDB::bind_method(D_METHOD("set_tile_library", "node"), &MCGrid::set_tile_library);
	ClassDB::bind_method(D_METHOD("get_tile_library"), &MCGrid::get_tile_library);

	ClassDB::bind_method(D_METHOD("save_grid", "path"), &MCGrid::save_grid);
	ClassDB::bind_method(D_METHOD("load_grid", "path"), &MCGrid::load_grid);
	ClassDB::bind_method(D_METHOD("cells_to_json"), &MCGrid::cells_to_json);
}

MCGrid::MCGrid() {}

MCGrid::~MCGrid() {}

// Chunks per axis (each at least 1); used by the next initialize_grid.
void MCGrid::set_grid_dims(const Vector3i &p_size) {
	if (grid_dims == p_size) {
		return;
	}
	grid_dims = p_size;
	// At scene load (not in the tree yet) and in the editor just store the value: MCManager builds
	// the grid once in initialize_all(). Rebuild only when changed on a live grid.
	if (is_inside_tree() && !chunks.empty() && !Engine::get_singleton()->is_editor_hint()) {
		initialize_grid(grid_dims.x, grid_dims.y, grid_dims.z, chunk_dims.x, chunk_dims.y, chunk_dims.z);
	}
}

// Cells per chunk per axis (each at least 1); used by the next initialize_grid.
void MCGrid::set_chunk_dims(const Vector3i &p_size) {
	if (chunk_dims == p_size) {
		return;
	}
	chunk_dims = p_size;
	if (is_inside_tree() && !chunks.empty() && !Engine::get_singleton()->is_editor_hint()) {
		initialize_grid(grid_dims.x, grid_dims.y, grid_dims.z, chunk_dims.x, chunk_dims.y, chunk_dims.z);
	}
}

// The tile library; the cached tile arrays are dropped because the meshes change.
void MCGrid::set_tile_library(MCTileLibrary *p_node) {
	tile_library = p_node;
	surface_cache.clear(); // other tile meshes
}

// The tile library that supplies the tiles.
MCTileLibrary *MCGrid::get_tile_library() const { return tile_library; }

// Nothing is built here: MCManager calls initialize_grid once the library is loaded.
void MCGrid::_ready() {
	if (Engine::get_singleton()->is_editor_hint()) {
		return;
	}
	// Generation is now triggered by MCManager to ensure correct order
	UtilityFunctions::print("MCGrid: _ready() called, waiting for MCManager...");
}

// Allocate the chunks (all corners empty, floor solid), drop placed objects, optionally build.
void MCGrid::initialize_grid(
		int p_chunks_x,
		int p_chunks_y,
		int p_chunks_z,
		int p_chunk_dims_x,
		int p_chunk_dims_y,
		int p_chunk_dims_z,
		bool p_refresh
) {
	if (p_chunks_x < 1 || p_chunks_y < 1 || p_chunks_z < 1 || p_chunk_dims_x < 1 || p_chunk_dims_y < 1 ||
		p_chunk_dims_z < 1) {
		UtilityFunctions::push_error("MCGrid: grid and chunk sizes must be at least 1 on every axis.");
		return;
	}
	grid_dims = Vector3i(p_chunks_x, p_chunks_y, p_chunks_z);
	chunk_dims = Vector3i(p_chunk_dims_x, p_chunk_dims_y, p_chunk_dims_z);

	chunks.clear();
	chunks.resize(static_cast<size_t>(grid_dims.x) * grid_dims.y * grid_dims.z);

	int num_corners = (chunk_dims.x + 1) * (chunk_dims.y + 1) * (chunk_dims.z + 1);
	int num_bytes = (num_corners + 7) / 8;

	UtilityFunctions::print(
			"Initializing grid: chunks(", p_chunks_x, ", ", p_chunks_y, ", ", p_chunks_z, ") chunk_dims(",
			p_chunk_dims_x, ", ", p_chunk_dims_y, ", ", p_chunk_dims_z, ")"
	);

	for (int y = 0; y < grid_dims.y; y++) {
		for (int z = 0; z < grid_dims.z; z++) {
			for (int x = 0; x < grid_dims.x; x++) {
				int index = _get_chunk_index(x, y, z);
				Chunk &c = chunks[static_cast<size_t>(index)];
				c.size_x = chunk_dims.x;
				c.size_y = chunk_dims.y;
				c.size_z = chunk_dims.z;
				c.loc_x = x;
				c.loc_y = y;
				c.loc_z = z;
				c.corner_bits.assign(static_cast<size_t>(num_bytes), 0); // Initialize all bits to 0

				c.debug_visuals.assign(static_cast<size_t>(num_corners), nullptr);
			}
		}
	}

	for (Chunk &chunk : chunks) {
		_initialize_boundaries(chunk);
	}

	placed_list.clear();
	_clear_placed_objects(); // their grid positions mean nothing in the new grid

	if (p_refresh) {
		refresh_grid();
	}
}

// Free everything spawned and rebuild every chunk mesh and debug cube.
void MCGrid::refresh_grid() {
	_free_spawned_nodes();
	surface_cache.clear(); // the library may have been reloaded
	Vector3i cells = _dims().max_corner();
	total_cells = cells.x * cells.y * cells.z;

	int spawn_count = 0;

	for (Chunk &chunk : chunks) {
		_build_chunk_mesh(chunk);
		spawn_count += chunk.mesh_node ? 1 : 0;

		if (!debug_corners_container) {
			debug_corners_container = memnew(Node3D);
			debug_corners_container->set_name("DebugCorners");
			add_child(debug_corners_container);
		}
		spawn_count += _spawn_debug_corners(chunk);
	}

	if (debug_corners_container) {
		debug_corners_container->set_visible(show_debug_corners);
	}
	UtilityFunctions::print("MCGrid Refresh: ", spawn_count, " visual nodes spawned.");
}

// Set the chunk's outer-shell corners: floor solid, sides and top empty.
void MCGrid::_initialize_boundaries(Chunk &p_chunk) const {
	int nx = p_chunk.size_x + 1;
	int ny = p_chunk.size_y + 1;
	int nz = p_chunk.size_z + 1;

	for (int ly = 0; ly < ny; ly++) {
		for (int lz = 0; lz < nz; lz++) {
			for (int lx = 0; lx < nx; lx++) {
				int gx = (p_chunk.loc_x * p_chunk.size_x) + lx;
				int gy = (p_chunk.loc_y * p_chunk.size_y) + ly;
				int gz = (p_chunk.loc_z * p_chunk.size_z) + lz;

				bool required_state = false;
				if (_is_boundary_corner(gx, gy, gz, required_state)) {
					p_chunk.set_corner(lx, ly, lz, required_state);
					continue;
				}

				// Standard internal corners are now always empty (false) by default
				p_chunk.set_corner(lx, ly, lz, false);
			}
		}
	}
}

// Is the corner on the grid's outer shell, and which state must it keep?
bool MCGrid::_is_boundary_corner(
		int gx,
		int gy,
		int gz,
		bool &r_required_state
) const {
	int max_gx = grid_dims.x * chunk_dims.x;
	int max_gy = grid_dims.y * chunk_dims.y;
	int max_gz = grid_dims.z * chunk_dims.z;

	// Bottommost (Y=0) is always 1 (solid)
	// Topmost (Y=max) is always 0 (empty)
	// X and Z horizontal boundaries are always 0 (empty)
	if (gy == 0 || gy == max_gy || gx == 0 || gx == max_gx || gz == 0 || gz == max_gz) {
		r_required_state = (gy == 0);
		return true;
	}

	return false;
}

// Turn a corner on / off in every chunk that stores it, mark the 8 cells around it dirty
// (rebuilt at the end of the frame) and update the nearby debug cubes.
void MCGrid::set_corner(
		const Vector3i &p_grid_pos,
		bool p_active
) {
	int gx = p_grid_pos.x;
	int gy = p_grid_pos.y;
	int gz = p_grid_pos.z;

	// Reject anything outside the grid (e.g. one below the floor when clicking a corner's underside):
	// the chunk maths below would turn it into a negative index.
	if (!_dims().contains_corner(p_grid_pos)) {
		return;
	}

	// Reject modifications to boundaries
	bool required_state = false;
	if (_is_boundary_corner(gx, gy, gz, required_state)) {
		UtilityFunctions::print("MCGrid: Cannot modify boundary corner at ", p_grid_pos);
		return;
	}

	bool actual_modified = false;

	// Update the logic states in all shared chunks
	int start_cx = (gx == 0) ? 0 : (gx - 1) / chunk_dims.x;
	int end_cx = (gx >= grid_dims.x * chunk_dims.x) ? grid_dims.x - 1 : gx / chunk_dims.x;
	int start_cy = (gy == 0) ? 0 : (gy - 1) / chunk_dims.y;
	int end_cy = (gy >= grid_dims.y * chunk_dims.y) ? grid_dims.y - 1 : gy / chunk_dims.y;
	int start_cz = (gz == 0) ? 0 : (gz - 1) / chunk_dims.z;
	int end_cz = (gz >= grid_dims.z * chunk_dims.z) ? grid_dims.z - 1 : gz / chunk_dims.z;

	for (int cy = start_cy; cy <= end_cy; cy++) {
		for (int cz = start_cz; cz <= end_cz; cz++) {
			for (int cx = start_cx; cx <= end_cx; cx++) {
				int idx = _get_chunk_index(cx, cy, cz);
				Chunk &chunk = chunks[idx];
				chunk.set_corner(
						gx - (cx * chunk_dims.x), gy - (cy * chunk_dims.y), gz - (cz * chunk_dims.z), p_active
				);
				actual_modified = true;
			}
		}
	}

	if (actual_modified) {
		// The 8 cells around the corner changed: rebuild the chunk(s) they belong to, once each.
		for (int cy = gy - 1; cy <= gy; cy++) {
			for (int cz = gz - 1; cz <= gz; cz++) {
				for (int cx = gx - 1; cx <= gx; cx++) {
					_mark_cell_dirty(cx, cy, cz);
				}
			}
		}
		_queue_rebuild();

		// Update debug corners (itself + 6 neighbors)
		_update_debug_corner(gx, gy, gz);
		_update_debug_corner(gx - 1, gy, gz);
		_update_debug_corner(gx + 1, gy, gz);
		_update_debug_corner(gx, gy - 1, gz);
		_update_debug_corner(gx, gy + 1, gz);
		_update_debug_corner(gx, gy, gz - 1);
		_update_debug_corner(gx, gy, gz + 1);
	}
}

// Hash of the cell at p_pos (grid coordinates), 0 outside the grid.
uint8_t MCGrid::get_cell_hash(const Vector3i &p_pos) const {
	int chunk_index = 0;
	Vector3i local;
	if (!_dims().cell_to_local(p_pos, chunk_index, local)) {
		return 0;
	}
	return chunks[static_cast<size_t>(chunk_index)].get_cell_hash(local.x, local.y, local.z);
}

// Is the global corner solid? Outside the grid counts as empty.
bool MCGrid::is_corner_active(const Vector3i &p_grid_pos) const {
	int chunk_index = 0;
	Vector3i local;
	if (!_dims().corner_to_local(p_grid_pos, chunk_index, local)) {
		return false; // outside the grid counts as empty
	}
	return chunks[static_cast<size_t>(chunk_index)].get_corner(local.x, local.y, local.z);
}

} // namespace godot
