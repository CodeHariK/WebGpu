/**
 * @file mp_grid.cpp
 * @brief Implementation of chunked Marching Prisms terrain grid.
 * @module src/terrain/marching_prism/mp_grid.cpp
 * @dependencies mp_grid.h, godot classes
 */

#include "mp_grid.h"
#include "game_manager/game_constants.h"
#include "mp.h"
#include <godot_cpp/classes/box_mesh.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/static_body3d.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

// Binds GDScript methods.
void MPGrid::_bind_methods() {
	ClassDB::bind_method(D_METHOD("initialize_grid", "chunks_x", "chunks_y", "chunks_z", "chunk_size_x", "chunk_size_y", "chunk_size_z"), &MPGrid::initialize_grid);
	ClassDB::bind_method(D_METHOD("refresh_grid"), &MPGrid::refresh_grid);
	ClassDB::bind_method(D_METHOD("modify_corner", "p_grid_pos", "p_active"), &MPGrid::modify_corner);

	ClassDB::bind_method(D_METHOD("set_grid_size", "size"), &MPGrid::set_grid_size);
	ClassDB::bind_method(D_METHOD("get_grid_size"), &MPGrid::get_grid_size);

	ClassDB::bind_method(D_METHOD("set_chunk_size", "size"), &MPGrid::set_chunk_size);
	ClassDB::bind_method(D_METHOD("get_chunk_size"), &MPGrid::get_chunk_size);

	ClassDB::bind_method(D_METHOD("set_show_debug_corners", "show"), &MPGrid::set_show_debug_corners);
	ClassDB::bind_method(D_METHOD("get_show_debug_corners"), &MPGrid::get_show_debug_corners);

	ClassDB::bind_method(D_METHOD("set_mp_node", "node"), &MPGrid::set_mp_node);
	ClassDB::bind_method(D_METHOD("get_mp_node"), &MPGrid::get_mp_node);

	ClassDB::bind_method(D_METHOD("save_grid", "path"), &MPGrid::save_grid);
	ClassDB::bind_method(D_METHOD("load_grid", "path"), &MPGrid::load_grid);

	ClassDB::bind_method(D_METHOD("get_total_mp_meshes"), &MPGrid::get_total_mp_meshes);
	ClassDB::bind_method(D_METHOD("get_total_debug_corners"), &MPGrid::get_total_debug_corners);
	ClassDB::bind_method(D_METHOD("get_total_cells"), &MPGrid::get_total_cells);
}

// Constructor.
MPGrid::MPGrid() {
	grid_size = Vector3i(1, 1, 1);
	chunk_size = Vector3i(6, 6, 6);
}

// Destructor.
MPGrid::~MPGrid() {}

// Ready callback.
void MPGrid::_ready() {
	if (Engine::get_singleton()->is_editor_hint()) {
		return;
	}
	UtilityFunctions::print("MPGrid: _ready() called.");
}

// Initializes the grid structures.
void MPGrid::initialize_grid(int p_chunks_x, int p_chunks_y, int p_chunks_z, int p_chunk_size_x, int p_chunk_size_y, int p_chunk_size_z, bool p_refresh) {
	grid_size = Vector3i(p_chunks_x, p_chunks_y, p_chunks_z);
	chunk_size = Vector3i(p_chunk_size_x, p_chunk_size_y, p_chunk_size_z);

	_clear_children();
	chunks.clear();

	int total_chunks = grid_size.x * grid_size.y * grid_size.z;
	chunks.resize(total_chunks);

	for (int y = 0; y < grid_size.y; y++) {
		for (int z = 0; z < grid_size.z; z++) {
			for (int x = 0; x < grid_size.x; x++) {
				int idx = _get_chunk_index(x, y, z);
				MPChunk &chunk = chunks[idx];
				chunk.size_x = chunk_size.x;
				chunk.size_y = chunk_size.y;
				chunk.size_z = chunk_size.z;
				chunk.loc_x = x;
				chunk.loc_y = y;
				chunk.loc_z = z;

				int num_corners = (chunk.size_x + 1) * (chunk.size_y + 1) * (chunk.size_z + 1);
				chunk.corner_states.assign((num_corners + 7) / 8, 0);
				chunk.cell_visuals.assign(chunk.size_x * 2 * chunk.size_y * chunk.size_z, nullptr);
				chunk.debug_visuals.assign(num_corners, nullptr);

				_initialize_boundaries(chunk);
			}
		}
	}

	if (p_refresh) {
		refresh_grid();
	}
}

// Rebuilds all visual components.
void MPGrid::refresh_grid() {
	_clear_children();
	total_mp_meshes = 0;
	total_cells = 0;

	if (_debug_box_mesh.is_null()) {
		_debug_box_mesh.instantiate();
		_debug_box_mesh->set_size(Vector3(0.1, 0.1, 0.1));
	}

	for (const MPChunk &chunk : chunks) {
		_spawn_marching_prisms(chunk, mp_node);
		_spawn_debug_cubes(chunk, _debug_box_mesh);
	}

	if (debug_corners_container) {
		debug_corners_container->set_visible(show_debug_corners);
	}
}

// Helper: loops over chunk cells and spawns meshes based on config.
int MPGrid::_spawn_marching_prisms(const MPChunk &p_chunk, MPNode *p_mp_node) {
	if (!p_mp_node)
		return 0;
	int count = 0;

	for (int y = 0; y < p_chunk.size_y; y++) {
		for (int z = 0; z < p_chunk.size_z; z++) {
			for (int cx = 0; cx < p_chunk.size_x * 2; cx++) {
				int vx = cx / 2;
				int v0_x, v0_z, v1_x, v1_z, v2_x, v2_z;

				if (z % 2 == 0) {
					if (cx % 2 == 0) {
						v0_x = vx;
						v0_z = z;
						v1_x = vx + 1;
						v1_z = z;
						v2_x = vx;
						v2_z = z + 1;
					} else {
						v0_x = vx + 1;
						v0_z = z;
						v1_x = vx;
						v1_z = z + 1;
						v2_x = vx + 1;
						v2_z = z + 1;
					}
				} else {
					if (cx % 2 == 0) {
						v0_x = vx;
						v0_z = z;
						v1_x = vx + 1;
						v1_z = z;
						v2_x = vx + 1;
						v2_z = z + 1;
					} else {
						v0_x = vx;
						v0_z = z;
						v1_x = vx;
						v1_z = z + 1;
						v2_x = vx + 1;
						v2_z = z + 1;
					}
				}

				uint8_t config_idx = 0;
				if (p_chunk.get_corner(v0_x, y, v0_z))
					config_idx |= (1 << 0);
				if (p_chunk.get_corner(v1_x, y, v1_z))
					config_idx |= (1 << 1);
				if (p_chunk.get_corner(v2_x, y, v2_z))
					config_idx |= (1 << 2);
				if (p_chunk.get_corner(v0_x, y + 1, v0_z))
					config_idx |= (1 << 3);
				if (p_chunk.get_corner(v1_x, y + 1, v1_z))
					config_idx |= (1 << 4);
				if (p_chunk.get_corner(v2_x, y + 1, v2_z))
					config_idx |= (1 << 5);

				total_cells++;
				if (config_idx == 0 || config_idx == 63)
					continue;

				PrismMeshConfig conf = p_mp_node->get_mesh_config(config_idx);
				if (conf.mesh.is_null())
					continue;

				float stagger = (z % 2 != 0) ? 0.5f : 0.0f;
				float physical_x = (vx + stagger);
				float physical_z = (z * 0.866025f);
				float physical_y = y * 1.0f;

				Vector3 world_pos = Vector3(
						static_cast<float>(p_chunk.loc_x * p_chunk.size_x) + physical_x,
						static_cast<float>(p_chunk.loc_y * p_chunk.size_y) + physical_y,
						static_cast<float>(p_chunk.loc_z * p_chunk.size_z) * 0.866025f + physical_z);

				MeshInstance3D *mi = memnew(MeshInstance3D);
				mi->set_mesh(conf.mesh);
				Transform3D cell_t;
				cell_t.origin = world_pos;
				mi->set_transform(cell_t * conf.transform);
				mi->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
				add_child(mi);

				if (is_inside_tree()) {
					mi->set_owner(get_owner() ? get_owner() : this);
				}

				int cell_idx = (y * p_chunk.size_x * 2 * p_chunk.size_z) + (z * p_chunk.size_x * 2) + cx;
				const_cast<MPChunk &>(p_chunk).cell_visuals[cell_idx] = mi;
				count++;
				total_mp_meshes++;
			}
		}
	}
	return count;
}

// Clears all visual nodes.
void MPGrid::_clear_children() {
	TypedArray<Node> children = get_children();
	for (int i = 0; i < children.size(); i++) {
		Node *child = Object::cast_to<Node>(children[i]);
		if (child && child != debug_corners_container) {
			remove_child(child);
			child->queue_free();
		}
	}

	if (debug_corners_container) {
		TypedArray<Node> debug_children = debug_corners_container->get_children();
		for (int i = 0; i < debug_children.size(); i++) {
			Node *child = Object::cast_to<Node>(debug_children[i]);
			if (child) {
				debug_corners_container->remove_child(child);
				child->queue_free();
			}
		}
	}

	for (MPChunk &chunk : chunks) {
		std::fill(chunk.cell_visuals.begin(), chunk.cell_visuals.end(), nullptr);
		std::fill(chunk.debug_visuals.begin(), chunk.debug_visuals.end(), nullptr);
	}

	total_mp_meshes = 0;
	total_debug_corners = 0;
	total_cells = 0;
}

// Updates the visual meshes for cells containing a given corner coordinate.
void MPGrid::_update_visual_at(int gx, int gy, int gz) {
	int cx = (gx < 0) ? -1 : gx / chunk_size.x;
	int cy = (gy < 0) ? -1 : gy / chunk_size.y;
	int cz = (gz < 0) ? -1 : gz / chunk_size.z;

	if (cx < 0 || cx >= grid_size.x || cy < 0 || cy >= grid_size.y || cz < 0 || cz >= grid_size.z) {
		return;
	}

	int lx = gx - (cx * chunk_size.x);
	int ly = gy - (cy * chunk_size.y);
	int lz = gz - (cz * chunk_size.z);

	MPChunk &chunk = chunks[_get_chunk_index(cx, cy, cz)];

	// In marching prisms, a corner affects multiple cells. Re-build chunk cells.
	// Since cell meshes are fast, we can just spawn/update cells around this.
	// For simplicity and correctness, rebuild cells in the chunk at y and z, z-1.
	for (int y = ly - 1; y <= ly; y++) {
		if (y < 0 || y >= chunk.size_y)
			continue;
		for (int z = lz - 1; z <= lz; z++) {
			if (z < 0 || z >= chunk.size_z)
				continue;
			for (int cx_l = lx * 2 - 2; cx_l <= lx * 2 + 2; cx_l++) {
				if (cx_l < 0 || cx_l >= chunk.size_x * 2)
					continue;

				int cell_idx = (y * chunk.size_x * 2 * chunk.size_z) + (z * chunk.size_x * 2) + cx_l;
				if (chunk.cell_visuals[cell_idx]) {
					chunk.cell_visuals[cell_idx]->queue_free();
					chunk.cell_visuals[cell_idx] = nullptr;
					total_mp_meshes--;
				}

				// Stagger formulas:
				int vx = cx_l / 2;
				int v0_x, v0_z, v1_x, v1_z, v2_x, v2_z;
				if (z % 2 == 0) {
					if (cx_l % 2 == 0) {
						v0_x = vx;
						v0_z = z;
						v1_x = vx + 1;
						v1_z = z;
						v2_x = vx;
						v2_z = z + 1;
					} else {
						v0_x = vx + 1;
						v0_z = z;
						v1_x = vx;
						v1_z = z + 1;
						v2_x = vx + 1;
						v2_z = z + 1;
					}
				} else {
					if (cx_l % 2 == 0) {
						v0_x = vx;
						v0_z = z;
						v1_x = vx + 1;
						v1_z = z;
						v2_x = vx + 1;
						v2_z = z + 1;
					} else {
						v0_x = vx;
						v0_z = z;
						v1_x = vx;
						v1_z = z + 1;
						v2_x = vx + 1;
						v2_z = z + 1;
					}
				}

				uint8_t config_idx = 0;
				if (chunk.get_corner(v0_x, y, v0_z))
					config_idx |= (1 << 0);
				if (chunk.get_corner(v1_x, y, v1_z))
					config_idx |= (1 << 1);
				if (chunk.get_corner(v2_x, y, v2_z))
					config_idx |= (1 << 2);
				if (chunk.get_corner(v0_x, y + 1, v0_z))
					config_idx |= (1 << 3);
				if (chunk.get_corner(v1_x, y + 1, v1_z))
					config_idx |= (1 << 4);
				if (chunk.get_corner(v2_x, y + 1, v2_z))
					config_idx |= (1 << 5);

				if (config_idx == 0 || config_idx == 63 || !mp_node)
					continue;

				PrismMeshConfig conf = mp_node->get_mesh_config(config_idx);
				if (conf.mesh.is_null())
					continue;

				float stagger = (z % 2 != 0) ? 0.5f : 0.0f;
				float physical_x = (vx + stagger);
				float physical_z = (z * 0.866025f);
				float physical_y = y * 1.0f;

				Vector3 world_pos = Vector3(
						static_cast<float>(cx * chunk_size.x) + physical_x,
						static_cast<float>(cy * chunk_size.y) + physical_y,
						static_cast<float>(cz * chunk_size.z) * 0.866025f + physical_z);

				MeshInstance3D *mi = memnew(MeshInstance3D);
				mi->set_mesh(conf.mesh);
				Transform3D cell_t;
				cell_t.origin = world_pos;
				mi->set_transform(cell_t * conf.transform);
				mi->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
				add_child(mi);

				if (is_inside_tree()) {
					mi->set_owner(get_owner() ? get_owner() : this);
				}

				chunk.cell_visuals[cell_idx] = mi;
				total_mp_meshes++;
			}
		}
	}
}

// Sets initial solid/empty states on chunk boundaries.
void MPGrid::_initialize_boundaries(MPChunk &p_chunk) const {
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
				p_chunk.set_corner(lx, ly, lz, false);
			}
		}
	}
}

// Identifies if voxel coordinates touch a map edge.
bool MPGrid::_is_boundary_corner(int gx, int gy, int gz, bool &r_required_state) const {
	int max_gx = grid_size.x * chunk_size.x;
	int max_gy = grid_size.y * chunk_size.y;
	int max_gz = grid_size.z * chunk_size.z;

	if (gy == 0 || gy == max_gy || gx == 0 || gx == max_gx || gz == 0 || gz == max_gz) {
		r_required_state = (gy == 0);
		return true;
	}
	return false;
}

// Modifies a single corner state and updates affected cell visuals.
void MPGrid::modify_corner(const Vector3i &p_grid_pos, bool p_active) {
	int gx = p_grid_pos.x;
	int gy = p_grid_pos.y;
	int gz = p_grid_pos.z;

	bool required_state = false;
	if (_is_boundary_corner(gx, gy, gz, required_state)) {
		return;
	}

	bool actual_modified = false;
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
				MPChunk &chunk = chunks[idx];
				chunk.set_corner(gx - (cx * chunk_size.x), gy - (cy * chunk_size.y), gz - (cz * chunk_size.z), p_active);
				actual_modified = true;
			}
		}
	}

	if (actual_modified) {
		for (int cy = gy - 1; cy <= gy; cy++) {
			for (int cz = gz - 1; cz <= gz; cz++) {
				for (int cx = gx - 1; cx <= gx; cx++) {
					_update_visual_at(cx, cy, cz);
				}
			}
		}

		_update_debug_at(gx, gy, gz);
		_update_debug_at(gx - 1, gy, gz);
		_update_debug_at(gx + 1, gy, gz);
		_update_debug_at(gx, gy - 1, gz);
		_update_debug_at(gx, gy + 1, gz);
		_update_debug_at(gx, gy, gz - 1);
		_update_debug_at(gx, gy, gz + 1);
	}
}

// Queries if a corner is active in the world.
bool MPGrid::is_corner_active(const Vector3i &p_grid_pos) const {
	int gx = p_grid_pos.x;
	int gy = p_grid_pos.y;
	int gz = p_grid_pos.z;

	int cx = (gx < 0) ? -1 : gx / chunk_size.x;
	int cy = (gy < 0) ? -1 : gy / chunk_size.y;
	int cz = (gz < 0) ? -1 : gz / chunk_size.z;

	if (cx < 0 || cx >= grid_size.x || cy < 0 || cy >= grid_size.y || cz < 0 || cz >= grid_size.z) {
		return false;
	}

	int idx = _get_chunk_index(cx, cy, cz);
	return chunks[idx].get_corner(gx - (cx * chunk_size.x), gy - (cy * chunk_size.y), gz - (cz * chunk_size.z));
}

// Configures visibility of debug helper cubes.
void MPGrid::set_show_debug_corners(bool p_show) {
	if (show_debug_corners == p_show)
		return;
	show_debug_corners = p_show;
	set_debug_corners_visible(show_debug_corners);
}

void MPGrid::set_corner_collision_enabled(bool p_enabled) {
	if (!debug_corners_container)
		return;
	uint32_t layer = p_enabled ? toLayer(LAYER_CORNERS) : 0;
	TypedArray<Node> children = debug_corners_container->get_children();
	for (int i = 0; i < children.size(); i++) {
		Node *child = Object::cast_to<Node>(children[i]);
		if (!child)
			continue;
		TypedArray<Node> sub_children = child->get_children();
		for (int j = 0; j < sub_children.size(); j++) {
			StaticBody3D *sb = Object::cast_to<StaticBody3D>(sub_children[j]);
			if (sb) {
				sb->set_collision_layer(layer);
			}
		}
	}
}

// Persists the grid corner states to a file.
void MPGrid::save_grid(const String &p_path) {
	Ref<FileAccess> file = FileAccess::open(p_path, FileAccess::WRITE);
	if (file.is_null()) {
		UtilityFunctions::print("MPGrid: Failed to write ", p_path);
		return;
	}

	file->store_32(0x50524953); // Magic 'PRIS'
	file->store_32(grid_size.x);
	file->store_32(grid_size.y);
	file->store_32(grid_size.z);
	file->store_32(chunk_size.x);
	file->store_32(chunk_size.y);
	file->store_32(chunk_size.z);

	for (const MPChunk &chunk : chunks) {
		file->store_32(chunk.corner_states.size());
		for (uint8_t byte : chunk.corner_states) {
			file->store_8(byte);
		}
	}
}

// Loads grid corner states from a file.
void MPGrid::load_grid(const String &p_path) {
	Ref<FileAccess> file = FileAccess::open(p_path, FileAccess::READ);
	if (file.is_null()) {
		UtilityFunctions::print("MPGrid: Failed to read ", p_path);
		return;
	}

	uint32_t magic = file->get_32();
	if (magic != 0x50524953) {
		UtilityFunctions::print("MPGrid: Invalid magic number");
		return;
	}

	int gx = file->get_32();
	int gy = file->get_32();
	int gz = file->get_32();
	int cx = file->get_32();
	int cy = file->get_32();
	int cz = file->get_32();

	initialize_grid(gx, gy, gz, cx, cy, cz, false);

	for (MPChunk &chunk : chunks) {
		uint32_t len = file->get_32();
		chunk.corner_states.resize(len);
		for (uint32_t i = 0; i < len; i++) {
			chunk.corner_states[i] = file->get_8();
		}
	}

	refresh_grid();
}

} // namespace godot
