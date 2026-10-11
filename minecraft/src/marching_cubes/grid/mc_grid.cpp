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

void MCGrid::_bind_methods() {
	ClassDB::bind_method(
			D_METHOD(
					"initialize_grid", "chunks_x", "chunks_y", "chunks_z", "chunk_size_x", "chunk_size_y",
					"chunk_size_z", "refresh"
			),
			&MCGrid::initialize_grid, DEFVAL(true)
	);
	ClassDB::bind_method(D_METHOD("refresh_grid"), &MCGrid::refresh_grid);
	ClassDB::bind_method(D_METHOD("modify_corner", "p_grid_pos", "p_active"), &MCGrid::modify_corner);

	ClassDB::bind_method(D_METHOD("set_grid_size", "size"), &MCGrid::set_grid_size);
	ClassDB::bind_method(D_METHOD("get_grid_size"), &MCGrid::get_grid_size);
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3I, "grid_size"), "set_grid_size", "get_grid_size");

	ClassDB::bind_method(D_METHOD("set_chunk_size", "size"), &MCGrid::set_chunk_size);
	ClassDB::bind_method(D_METHOD("get_chunk_size"), &MCGrid::get_chunk_size);
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3I, "chunk_size"), "set_chunk_size", "get_chunk_size");

	ClassDB::bind_method(D_METHOD("set_show_debug_corners", "show"), &MCGrid::set_show_debug_corners);
	ClassDB::bind_method(D_METHOD("get_show_debug_corners"), &MCGrid::get_show_debug_corners);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "show_debug_corners"), "set_show_debug_corners", "get_show_debug_corners");

	ClassDB::bind_method(D_METHOD("set_mc_node", "node"), &MCGrid::set_mc_node);
	ClassDB::bind_method(D_METHOD("get_mc_node"), &MCGrid::get_mc_node);

	ClassDB::bind_method(D_METHOD("save_grid", "path"), &MCGrid::save_grid);
	ClassDB::bind_method(D_METHOD("load_grid", "path"), &MCGrid::load_grid);
	ClassDB::bind_method(D_METHOD("cells_to_json"), &MCGrid::cells_to_json);
}

MCGrid::MCGrid() {}

MCGrid::~MCGrid() {}

void MCGrid::set_grid_size(const Vector3i &p_size) {
	if (grid_size == p_size) {
		return;
	}
	grid_size = p_size;
	// At scene load (not in the tree yet) and in the editor just store the value: MCManager builds
	// the grid once in initialize_all(). Rebuild only when changed on a live grid.
	if (is_inside_tree() && !chunks.empty() && !Engine::get_singleton()->is_editor_hint()) {
		initialize_grid(grid_size.x, grid_size.y, grid_size.z, chunk_size.x, chunk_size.y, chunk_size.z);
	}
}

void MCGrid::set_chunk_size(const Vector3i &p_size) {
	if (chunk_size == p_size) {
		return;
	}
	chunk_size = p_size;
	if (is_inside_tree() && !chunks.empty() && !Engine::get_singleton()->is_editor_hint()) {
		initialize_grid(grid_size.x, grid_size.y, grid_size.z, chunk_size.x, chunk_size.y, chunk_size.z);
	}
}

void MCGrid::set_mc_node(MCNode *p_node) { mc_node = p_node; }

MCNode *MCGrid::get_mc_node() const { return mc_node; }

void MCGrid::_ready() {
	if (Engine::get_singleton()->is_editor_hint()) {
		return;
	}
	// Generation is now triggered by MCManager to ensure correct order
	UtilityFunctions::print("MCGrid: _ready() called, waiting for MCManager...");
}

void MCGrid::initialize_grid(
		int p_chunks_x,
		int p_chunks_y,
		int p_chunks_z,
		int p_chunk_size_x,
		int p_chunk_size_y,
		int p_chunk_size_z,
		bool p_refresh
) {
	if (p_chunks_x < 1 || p_chunks_y < 1 || p_chunks_z < 1 || p_chunk_size_x < 1 || p_chunk_size_y < 1 ||
		p_chunk_size_z < 1) {
		UtilityFunctions::push_error("MCGrid: grid and chunk sizes must be at least 1 on every axis.");
		return;
	}
	grid_size = Vector3i(p_chunks_x, p_chunks_y, p_chunks_z);
	chunk_size = Vector3i(p_chunk_size_x, p_chunk_size_y, p_chunk_size_z);

	chunks.clear();
	chunks.resize(static_cast<size_t>(grid_size.x) * grid_size.y * grid_size.z);

	int num_corners = (chunk_size.x + 1) * (chunk_size.y + 1) * (chunk_size.z + 1);
	int num_bytes = (num_corners + 7) / 8;

	UtilityFunctions::print(
			"Initializing grid: chunks(", p_chunks_x, ", ", p_chunks_y, ", ", p_chunks_z, ") chunk_size(",
			p_chunk_size_x, ", ", p_chunk_size_y, ", ", p_chunk_size_z, ")"
	);

	for (int y = 0; y < grid_size.y; y++) {
		for (int z = 0; z < grid_size.z; z++) {
			for (int x = 0; x < grid_size.x; x++) {
				int index = _get_chunk_index(x, y, z);
				Chunk &c = chunks[static_cast<size_t>(index)];
				c.size_x = chunk_size.x;
				c.size_y = chunk_size.y;
				c.size_z = chunk_size.z;
				c.loc_x = x;
				c.loc_y = y;
				c.loc_z = z;
				c.corner_states.assign(static_cast<size_t>(num_bytes), 0); // Initialize all bits to 0

				int num_cells = chunk_size.x * chunk_size.y * chunk_size.z;
				c.cell_visuals.assign(static_cast<size_t>(num_cells), nullptr);

				c.debug_visuals.assign(static_cast<size_t>(num_corners), nullptr);
			}
		}
	}

	for (Chunk &chunk : chunks) {
		_initialize_boundaries(chunk);
	}

	spatial.clear();
	_clear_placed_objects(); // their grid positions mean nothing in the new grid

	if (p_refresh) {
		refresh_grid();
	}
}

void MCGrid::refresh_grid() {
	_clear_children();

	int spawn_count = 0;

	for (Chunk &chunk : chunks) {
		if (mc_node) {
			spawn_count += _spawn_marching_cubes(chunk, mc_node);
		}

		if (!debug_corners_container) {
			debug_corners_container = memnew(Node3D);
			debug_corners_container->set_name("DebugCorners");
			add_child(debug_corners_container);
		}
		spawn_count += _spawn_debug_cubes(chunk);
	}

	if (debug_corners_container) {
		debug_corners_container->set_visible(show_debug_corners);
	}
	UtilityFunctions::print("MCGrid Refresh: ", spawn_count, " visual nodes spawned.");
}

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

bool MCGrid::_is_boundary_corner(
		int gx,
		int gy,
		int gz,
		bool &r_required_state
) const {
	int max_gx = grid_size.x * chunk_size.x;
	int max_gy = grid_size.y * chunk_size.y;
	int max_gz = grid_size.z * chunk_size.z;

	// Bottommost (Y=0) is always 1 (solid)
	// Topmost (Y=max) is always 0 (empty)
	// X and Z horizontal boundaries are always 0 (empty)
	if (gy == 0 || gy == max_gy || gx == 0 || gx == max_gx || gz == 0 || gz == max_gz) {
		r_required_state = (gy == 0);
		return true;
	}

	return false;
}

void MCGrid::modify_corner(
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
	int start_cx = (gx == 0) ? 0 : (gx - 1) / chunk_size.x;
	int end_cx = (gx >= grid_size.x * chunk_size.x) ? grid_size.x - 1 : gx / chunk_size.x;
	int start_cy = (gy == 0) ? 0 : (gy - 1) / chunk_size.y;
	int end_cy = (gy >= grid_size.y * chunk_size.y) ? grid_size.y - 1 : gy / chunk_size.y;
	int start_cz = (gz == 0) ? 0 : (gz - 1) / chunk_size.z;
	int end_cz = (gz >= grid_size.z * chunk_size.z) ? grid_size.z - 1 : gz / chunk_size.z;

	for (int cy = start_cy; cy <= end_cy; cy++) {
		for (int cz = start_cz; cz <= end_cz; cz++) {
			for (int cx = start_cx; cx <= end_cx; cx++) {
				int idx = _get_chunk_index(cx, cy, cz);
				Chunk &chunk = chunks[idx];
				chunk.set_corner(
						gx - (cx * chunk_size.x), gy - (cy * chunk_size.y), gz - (cz * chunk_size.z), p_active
				);
				actual_modified = true;
			}
		}
	}

	if (actual_modified) {
		// Update the 8 affected cells
		for (int cy = gy - 1; cy <= gy; cy++) {
			for (int cz = gz - 1; cz <= gz; cz++) {
				for (int cx = gx - 1; cx <= gx; cx++) {
					_update_visual_at(cx, cy, cz);
				}
			}
		}

		// Update debug corners (itself + 6 neighbors)
		_update_debug_at(gx, gy, gz);
		_update_debug_at(gx - 1, gy, gz);
		_update_debug_at(gx + 1, gy, gz);
		_update_debug_at(gx, gy - 1, gz);
		_update_debug_at(gx, gy + 1, gz);
		_update_debug_at(gx, gy, gz - 1);
		_update_debug_at(gx, gy, gz + 1);
	}
}

uint8_t MCGrid::get_cell_hash_at_global_coord(const Vector3i &p_pos) const {
	int chunk_index = 0;
	Vector3i local;
	if (!_dims().cell_to_local(p_pos, chunk_index, local)) {
		return 0;
	}
	return chunks[static_cast<size_t>(chunk_index)].get_cell_hash(local.x, local.y, local.z);
}

bool MCGrid::is_corner_active(const Vector3i &p_grid_pos) const {
	int chunk_index = 0;
	Vector3i local;
	if (!_dims().corner_to_local(p_grid_pos, chunk_index, local)) {
		return false; // outside the grid counts as empty
	}
	return chunks[static_cast<size_t>(chunk_index)].get_corner(local.x, local.y, local.z);
}

} // namespace godot
