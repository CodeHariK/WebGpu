#include "marching_cubes/terrain.h"
#include "marching_cubes/mc.h"
#include <godot_cpp/classes/box_mesh.hpp>
#include <godot_cpp/classes/box_shape3d.hpp>
#include <godot_cpp/classes/collision_shape3d.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/fast_noise_lite.hpp>
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

void MCTerrain::_bind_methods() {
	ClassDB::bind_method(D_METHOD("initialize_terrain", "chunks_x", "chunks_y", "chunks_z", "chunk_size_x", "chunk_size_y", "chunk_size_z"), &MCTerrain::initialize_terrain);
	ClassDB::bind_method(D_METHOD("initialize_terrain_with_noise", "chunks_x", "chunks_y", "chunks_z", "chunk_size_x", "chunk_size_y", "chunk_size_z", "threshold"), &MCTerrain::initialize_terrain_with_noise, DEFVAL(0.0));
	ClassDB::bind_method(D_METHOD("generate_with_noise"), &MCTerrain::generate_with_noise);
	ClassDB::bind_method(D_METHOD("refresh_terrain"), &MCTerrain::refresh_terrain);
	ClassDB::bind_method(D_METHOD("modify_corner", "p_grid_pos", "p_active"), &MCTerrain::modify_corner);

	ClassDB::bind_method(D_METHOD("set_terrain_size", "size"), &MCTerrain::set_terrain_size);
	ClassDB::bind_method(D_METHOD("get_terrain_size"), &MCTerrain::get_terrain_size);
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3I, "terrain_size"), "set_terrain_size", "get_terrain_size");

	ClassDB::bind_method(D_METHOD("set_chunk_size", "size"), &MCTerrain::set_chunk_size);
	ClassDB::bind_method(D_METHOD("get_chunk_size"), &MCTerrain::get_chunk_size);
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3I, "chunk_size"), "set_chunk_size", "get_chunk_size");

	ClassDB::bind_method(D_METHOD("set_noise", "noise"), &MCTerrain::set_noise);
	ClassDB::bind_method(D_METHOD("get_noise"), &MCTerrain::get_noise);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "noise", PROPERTY_HINT_RESOURCE_TYPE, "FastNoiseLite"), "set_noise", "get_noise");

	ClassDB::bind_method(D_METHOD("set_noise_threshold", "threshold"), &MCTerrain::set_noise_threshold);
	ClassDB::bind_method(D_METHOD("get_noise_threshold"), &MCTerrain::get_noise_threshold);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "noise_threshold"), "set_noise_threshold", "get_noise_threshold");

	ClassDB::bind_method(D_METHOD("set_show_debug_corners", "show"), &MCTerrain::set_show_debug_corners);
	ClassDB::bind_method(D_METHOD("get_show_debug_corners"), &MCTerrain::get_show_debug_corners);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "show_debug_corners"), "set_show_debug_corners", "get_show_debug_corners");

	ClassDB::bind_method(D_METHOD("set_mc_node", "node"), &MCTerrain::set_mc_node);
	ClassDB::bind_method(D_METHOD("get_mc_node"), &MCTerrain::get_mc_node);

	ClassDB::bind_method(D_METHOD("save_terrain", "path"), &MCTerrain::save_terrain);
	ClassDB::bind_method(D_METHOD("load_terrain", "path"), &MCTerrain::load_terrain);
}

MCTerrain::MCTerrain() {
	noise.instantiate();
	noise->connect("changed", Callable(this, "_on_noise_changed"));
}

MCTerrain::~MCTerrain() {
	if (noise.is_valid()) {
		noise->disconnect("changed", Callable(this, "_on_noise_changed"));
	}
}

void MCTerrain::set_terrain_size(const Vector3i &p_size) {
	if (terrain_size == p_size) {
		return;
	}
	terrain_size = p_size;
	generate_with_noise();
}

void MCTerrain::set_chunk_size(const Vector3i &p_size) {
	if (chunk_size == p_size) {
		return;
	}
	chunk_size = p_size;
	generate_with_noise();
}

void MCTerrain::set_noise(const Ref<FastNoiseLite> &p_noise) {
	if (noise == p_noise) {
		return;
	}

	if (noise.is_valid()) {
		noise->disconnect("changed", Callable(this, "_on_noise_changed"));
	}

	noise = p_noise;

	if (noise.is_valid()) {
		noise->connect("changed", Callable(this, "_on_noise_changed"));
	}

	generate_with_noise();
}

Ref<FastNoiseLite> MCTerrain::get_noise() const {
	return noise;
}

void MCTerrain::set_mc_node(MCNode *p_node) {
	mc_node = p_node;
}

MCNode *MCTerrain::get_mc_node() const {
	return mc_node;
}

void MCTerrain::set_noise_threshold(float p_threshold) {
	if (Math::is_equal_approx(noise_threshold, p_threshold)) {
		return;
	}
	noise_threshold = p_threshold;
	generate_with_noise();
}

void MCTerrain::initialize_terrain(int p_chunks_x, int p_chunks_y, int p_chunks_z, int p_chunk_size_x, int p_chunk_size_y, int p_chunk_size_z) {
	terrain_size = Vector3i(p_chunks_x, p_chunks_y, p_chunks_z);
	chunk_size = Vector3i(p_chunk_size_x, p_chunk_size_y, p_chunk_size_z);

	chunks.clear();
	chunks.resize(static_cast<size_t>(terrain_size.x) * terrain_size.y * terrain_size.z);

	int num_corners = (chunk_size.x + 1) * (chunk_size.y + 1) * (chunk_size.z + 1);
	int num_bytes = (num_corners + 7) / 8;

	UtilityFunctions::print("Initializing terrain: chunks(", p_chunks_x, ", ", p_chunks_y, ", ", p_chunks_z, ") chunk_size(", p_chunk_size_x, ", ", p_chunk_size_y, ", ", p_chunk_size_z, ")");

	for (int z = 0; z < terrain_size.z; z++) {
		for (int y = 0; y < terrain_size.y; y++) {
			for (int x = 0; x < terrain_size.x; x++) {
				int index = x + (y * terrain_size.x) + (z * terrain_size.x * terrain_size.y);
				Chunk &c = chunks[static_cast<size_t>(index)];
				c.size_x = chunk_size.x;
				c.size_y = chunk_size.y;
				c.size_z = chunk_size.z;
				c.loc_x = x;
				c.loc_y = y;
				c.loc_z = z;
				c.corner_states.assign(static_cast<size_t>(num_bytes), 0); // Initialize all bits to 0
			}
		}
	}
}

void MCTerrain::initialize_terrain_with_noise(int p_chunks_x, int p_chunks_y, int p_chunks_z, int p_chunk_size_x, int p_chunk_size_y, int p_chunk_size_z, float p_threshold) {
	initialize_terrain(p_chunks_x, p_chunks_y, p_chunks_z, p_chunk_size_x, p_chunk_size_y, p_chunk_size_z);

	if (noise.is_null()) {
		UtilityFunctions::print("MCTerrain: Noise resource is null, using default.");
		noise.instantiate();
	}

	for (Chunk &chunk : chunks) {
		_sample_chunk_noise(chunk, noise, p_threshold);
	}
	refresh_terrain();
}

void MCTerrain::generate_with_noise() {
	if (Engine::get_singleton()->is_editor_hint() || !is_inside_tree()) {
		return;
	}
	initialize_terrain_with_noise(terrain_size.x, terrain_size.y, terrain_size.z, chunk_size.x, chunk_size.y, chunk_size.z, noise_threshold);
}

void MCTerrain::refresh_terrain() {
	_clear_children();

	Ref<BoxMesh> box_mesh;
	box_mesh.instantiate();
	box_mesh->set_size(Vector3(0.1, 0.1, 0.1));

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
		spawn_count += _spawn_debug_cubes(chunk, box_mesh);
	}

	if (debug_corners_container) {
		debug_corners_container->set_visible(show_debug_corners);
	}
	UtilityFunctions::print("Terrain Refresh: ", spawn_count, " visual nodes spawned.");
}

void MCTerrain::modify_corner(const Vector3i &p_grid_pos, bool p_active) {
	int gx = p_grid_pos.x;
	int gy = p_grid_pos.y;
	int gz = p_grid_pos.z;

	// Reject modifications to boundaries
	bool required_state = false;
	if (_is_boundary_corner(gx, gy, gz, required_state)) {
		UtilityFunctions::print("MCTerrain: Cannot modify boundary corner at ", p_grid_pos);
		return;
	}

	bool modified = false;
	for (Chunk &chunk : chunks) {
		int start_x = chunk.loc_x * chunk.size_x;
		int end_x = start_x + chunk.size_x;
		int start_y = chunk.loc_y * chunk.size_y;
		int end_y = start_y + chunk.size_y;
		int start_z = chunk.loc_z * chunk.size_z;
		int end_z = start_z + chunk.size_z;

		if (gx >= start_x && gx <= end_x &&
				gy >= start_y && gy <= end_y &&
				gz >= start_z && gz <= end_z) {
			chunk.set_corner(gx - start_x, gy - start_y, gz - start_z, p_active);
			modified = true;
		}
	}

	if (modified) {
		UtilityFunctions::print("Voxel Modified at ", p_grid_pos, " to ", p_active);
		refresh_terrain();
	}
}

int MCTerrain::_spawn_marching_cubes(const Chunk &p_chunk, MCNode *p_mc_node) {
	int count = 0;

	for (int ly = 0; ly < p_chunk.size_y; ly++) {
		for (int lz = 0; lz < p_chunk.size_z; lz++) {
			for (int lx = 0; lx < p_chunk.size_x; lx++) {
				uint8_t hash = p_chunk.get_cell_hash(lx, ly, lz);
				total_cells++;

				if (hash == 0) {
					continue;
				}

				MeshConfig conf = p_mc_node->get_mesh_config(hash);
				if (conf.mesh.is_null()) {
					continue;
				}

				// Spawn at center of the 8 corners (cell center)
				Vector3 world_pos = Vector3(
						static_cast<float>((p_chunk.loc_x * p_chunk.size_x) + lx) + 0.5f,
						static_cast<float>((p_chunk.loc_y * p_chunk.size_y) + ly) + 0.5f,
						static_cast<float>((p_chunk.loc_z * p_chunk.size_z) + lz) + 0.5f);

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
				count++;
				total_mc_meshes++;
			}
		}
	}
	return count;
}

// generate_with_noise moved up

uint8_t MCTerrain::get_cell_hash_at_global_coord(const Vector3i &p_pos) const {
	int gx = p_pos.x;
	int gy = p_pos.y;
	int gz = p_pos.z;

	for (const Chunk &chunk : chunks) {
		int start_x = chunk.loc_x * chunk.size_x;
		int end_x = start_x + chunk.size_x;
		int start_y = chunk.loc_y * chunk.size_y;
		int end_y = start_y + chunk.size_y;
		int start_z = chunk.loc_z * chunk.size_z;
		int end_z = start_z + chunk.size_z;

		if (gx >= start_x && gx < end_x &&
				gy >= start_y && gy < end_y &&
				gz >= start_z && gz < end_z) {
			return chunk.get_cell_hash(gx - start_x, gy - start_y, gz - start_z);
		}
	}
	return 0;
}

void MCTerrain::save_terrain(const String &p_path) {
	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::WRITE);
	if (f.is_null()) {
		UtilityFunctions::print("MCTerrain: Failed to open file for writing: ", p_path);
		return;
	}

	// 1. Header
	f->store_32(0x4D435452); // Magic: 'MCTR'
	f->store_32(1); // Version

	// 2. Metadata
	f->store_32(terrain_size.x);
	f->store_32(terrain_size.y);
	f->store_32(terrain_size.z);
	f->store_32(chunk_size.x);
	f->store_32(chunk_size.y);
	f->store_32(chunk_size.z);

	// 3. Chunk Data
	f->store_32(static_cast<uint32_t>(chunks.size()));
	for (const Chunk &chunk : chunks) {
		f->store_32(static_cast<uint32_t>(chunk.corner_states.size()));
		PackedByteArray arr;
		arr.resize(static_cast<int64_t>(chunk.corner_states.size()));
		for (size_t i = 0; i < chunk.corner_states.size(); i++) {
			arr[static_cast<int>(i)] = chunk.corner_states[i];
		}
		f->store_buffer(arr);
	}

	UtilityFunctions::print("MCTerrain: Saved state to ", p_path);
}

void MCTerrain::load_terrain(const String &p_path) {
	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::READ);
	if (f.is_null()) {
		UtilityFunctions::print("MCTerrain: Failed to open file for reading: ", p_path);
		return;
	}

	// 1. Header
	uint32_t magic = f->get_32();
	if (magic != 0x4D435452) {
		UtilityFunctions::print("MCTerrain: Invalid magic number in file: ", p_path);
		return;
	}
	uint32_t version = f->get_32();

	// 2. Metadata
	int32_t tx = f->get_32();
	int32_t ty = f->get_32();
	int32_t tz = f->get_32();
	int32_t cx = f->get_32();
	int32_t cy = f->get_32();
	int32_t cz = f->get_32();

	// If size changed, re-initialize
	if (terrain_size != Vector3i(tx, ty, tz) || chunk_size != Vector3i(cx, cy, cz)) {
		initialize_terrain(tx, ty, tz, cx, cy, cz);
	}

	// 3. Chunk Data
	uint32_t num_chunks = f->get_32();
	if (num_chunks != chunks.size()) {
		UtilityFunctions::print("MCTerrain: Chunk count mismatch in file.");
		return;
	}

	for (uint32_t i = 0; i < num_chunks; i++) {
		uint32_t data_size = f->get_32();
		PackedByteArray arr = f->get_buffer(data_size);

		Chunk &chunk = chunks[i];
		chunk.corner_states.resize(data_size);
		for (uint32_t j = 0; j < data_size; j++) {
			chunk.corner_states[j] = arr[j];
		}
	}

	UtilityFunctions::print("MCTerrain: Loaded state from ", p_path);
	refresh_terrain();
}

void MCTerrain::_ready() {
	if (Engine::get_singleton()->is_editor_hint()) {
		return;
	}
	// Generation is now triggered by MCManager to ensure correct order
	UtilityFunctions::print("MCTerrain: _ready() called, waiting for MCManager...");
}

void MCTerrain::_clear_children() {
	TypedArray<Node> children = get_children();
	for (int i = 0; i < children.size(); i++) {
		Node *child = Object::cast_to<Node>(children[i]);
		if (child && child != debug_corners_container) {
			child->queue_free();
		}
	}

	if (debug_corners_container) {
		TypedArray<Node> debug_children = debug_corners_container->get_children();
		for (int i = 0; i < debug_children.size(); i++) {
			Node *child = Object::cast_to<Node>(debug_children[i]);
			if (child) {
				child->queue_free();
			}
		}
	}

	total_mc_meshes = 0;
	total_debug_corners = 0;
	total_cells = 0;
}

void MCTerrain::_on_noise_changed() {
	generate_with_noise();
}

void MCTerrain::_sample_chunk_noise(Chunk &p_chunk, const Ref<FastNoiseLite> &p_noise, float p_threshold) const {
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

bool MCTerrain::_is_boundary_corner(int gx, int gy, int gz, bool &r_required_state) const {
	int max_gx = terrain_size.x * chunk_size.x;
	int max_gy = terrain_size.y * chunk_size.y;
	int max_gz = terrain_size.z * chunk_size.z;

	// Bottommost (Y=0) is always 1 (solid)
	if (gy == 0) {
		r_required_state = true;
		return true;
	}

	// Topmost (Y=max) is always 0 (empty)
	if (gy == max_gy) {
		r_required_state = false;
		return true;
	}

	// X and Z horizontal boundaries are always 0 (empty)
	if (gx == 0 || gx == max_gx || gz == 0 || gz == max_gz) {
		r_required_state = false;
		return true;
	}

	return false;
}

} // namespace godot
