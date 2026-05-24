#include "surface_nets/sn_grid.h"
#include "surface_nets/surface_nets.h"
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/static_body3d.hpp>
#include <godot_cpp/classes/collision_shape3d.hpp>
#include <godot_cpp/classes/concave_polygon_shape3d.hpp>
#include <godot_cpp/classes/array_mesh.hpp>
#include <godot_cpp/classes/material.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <algorithm>

namespace godot {

// --- SNChunk Implementation ---

PackedByteArray SNChunk::serialize_rle() const {
	PackedByteArray output;
	if (corner_densities.empty()) {
		return output;
	}

	int8_t current_val = corner_densities[0];
	uint32_t run_length = 1;

	for (size_t i = 1; i < corner_densities.size(); ++i) {
		if (corner_densities[i] == current_val && run_length < 255) {
			run_length++;
		} else {
			output.push_back(static_cast<uint8_t>(run_length));
			output.push_back(static_cast<uint8_t>(current_val));
			current_val = corner_densities[i];
			run_length = 1;
		}
	}
	output.push_back(static_cast<uint8_t>(run_length));
	output.push_back(static_cast<uint8_t>(current_val));

	return output;
}

void SNChunk::deserialize_rle(const PackedByteArray &p_data) {
	int num_corners = (size_x + 1) * (size_y + 1) * (size_z + 1);
	corner_densities.clear();
	corner_densities.reserve(num_corners);

	for (int i = 0; i < p_data.size(); i += 2) {
		if (i + 1 >= p_data.size()) {
			break;
		}
		uint8_t run_length = p_data[i];
		int8_t val = static_cast<int8_t>(p_data[i + 1]);
		for (int r = 0; r < run_length; ++r) {
			if (corner_densities.size() < static_cast<size_t>(num_corners)) {
				corner_densities.push_back(val);
			}
		}
	}

	if (corner_densities.size() < static_cast<size_t>(num_corners)) {
		corner_densities.resize(num_corners, 127); // Default to empty air (127)
	}
}

// --- SNGrid Implementation ---

void SNGrid::_bind_methods() {
	ClassDB::bind_method(D_METHOD("initialize_grid", "chunks_x", "chunks_y", "chunks_z", "chunk_size_x", "chunk_size_y", "chunk_size_z"), &SNGrid::initialize_grid);
	ClassDB::bind_method(D_METHOD("refresh_grid"), &SNGrid::refresh_grid);
	ClassDB::bind_method(D_METHOD("modify_density", "p_grid_pos", "p_density"), &SNGrid::modify_density);
	ClassDB::bind_method(D_METHOD("get_density", "p_grid_pos"), &SNGrid::get_density);
	ClassDB::bind_method(D_METHOD("is_solid", "p_grid_pos"), &SNGrid::is_solid);

	ClassDB::bind_method(D_METHOD("set_grid_size", "size"), &SNGrid::set_grid_size);
	ClassDB::bind_method(D_METHOD("get_grid_size"), &SNGrid::get_grid_size);
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3I, "grid_size"), "set_grid_size", "get_grid_size");

	ClassDB::bind_method(D_METHOD("set_chunk_size", "size"), &SNGrid::set_chunk_size);
	ClassDB::bind_method(D_METHOD("get_chunk_size"), &SNGrid::get_chunk_size);
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3I, "chunk_size"), "set_chunk_size", "get_chunk_size");

	ClassDB::bind_method(D_METHOD("set_terrain_material", "material"), &SNGrid::set_terrain_material);
	ClassDB::bind_method(D_METHOD("get_terrain_material"), &SNGrid::get_terrain_material);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "terrain_material", PROPERTY_HINT_RESOURCE_TYPE, "Material"), "set_terrain_material", "get_terrain_material");

	ClassDB::bind_method(D_METHOD("generate_test_sdf"), &SNGrid::generate_test_sdf);

	ClassDB::bind_method(D_METHOD("save_grid", "path"), &SNGrid::save_grid);
	ClassDB::bind_method(D_METHOD("load_grid", "path"), &SNGrid::load_grid);
}

SNGrid::SNGrid() {
}

SNGrid::~SNGrid() {
}

void SNGrid::_clear_chunk_visuals(SNChunk &p_chunk) {
	if (p_chunk.visual_node) {
		p_chunk.visual_node->queue_free();
		p_chunk.visual_node = nullptr;
	}
	if (p_chunk.collision_body) {
		p_chunk.collision_body->queue_free();
		p_chunk.collision_body = nullptr;
	}
}

void SNGrid::_ready() {
	if (Engine::get_singleton()->is_editor_hint()) {
		return;
	}
	// By default, initialize with a test SDF scene
	initialize_grid(2, 1, 1, 16, 16, 16, false);
	generate_test_sdf();
}

void SNGrid::set_grid_size(const Vector3i &p_size) {
	if (grid_size == p_size) {
		return;
	}
	grid_size = p_size;
	initialize_grid(grid_size.x, grid_size.y, grid_size.z, chunk_size.x, chunk_size.y, chunk_size.z);
}

void SNGrid::set_chunk_size(const Vector3i &p_size) {
	if (chunk_size == p_size) {
		return;
	}
	chunk_size = p_size;
	initialize_grid(grid_size.x, grid_size.y, grid_size.z, chunk_size.x, chunk_size.y, chunk_size.z);
}

void SNGrid::set_terrain_material(const Ref<Material> &p_material) {
	terrain_material = p_material;
	if (is_inside_tree()) {
		refresh_grid();
	}
}

Ref<Material> SNGrid::get_terrain_material() const {
	return terrain_material;
}

void SNGrid::initialize_grid(int p_chunks_x, int p_chunks_y, int p_chunks_z, int p_chunk_size_x, int p_chunk_size_y, int p_chunk_size_z, bool p_refresh) {
	for (SNChunk &chunk : chunks) {
		_clear_chunk_visuals(chunk);
	}

	grid_size = Vector3i(p_chunks_x, p_chunks_y, p_chunks_z);
	chunk_size = Vector3i(p_chunk_size_x, p_chunk_size_y, p_chunk_size_z);

	chunks.clear();
	chunks.resize(static_cast<size_t>(grid_size.x) * grid_size.y * grid_size.z);

	int num_corners = (chunk_size.x + 1) * (chunk_size.y + 1) * (chunk_size.z + 1);

	UtilityFunctions::print("SNGrid: Initializing: chunks(", p_chunks_x, ", ", p_chunks_y, ", ", p_chunks_z, ") chunk_size(", p_chunk_size_x, ", ", p_chunk_size_y, ", ", p_chunk_size_z, ")");

	for (int y = 0; y < grid_size.y; y++) {
		for (int z = 0; z < grid_size.z; z++) {
			for (int x = 0; x < grid_size.x; x++) {
				int index = _get_chunk_index(x, y, z);
				SNChunk &c = chunks[static_cast<size_t>(index)];
				c.size_x = chunk_size.x;
				c.size_y = chunk_size.y;
				c.size_z = chunk_size.z;
				c.loc_x = x;
				c.loc_y = y;
				c.loc_z = z;
				c.corner_densities.assign(static_cast<size_t>(num_corners), 127); // Default to empty air (127)
			}
		}
	}

	for (SNChunk &chunk : chunks) {
		_initialize_boundaries(chunk);
	}

	if (p_refresh) {
		refresh_grid();
	}
}

void SNGrid::refresh_grid() {
	for (size_t i = 0; i < chunks.size(); ++i) {
		_update_chunk_mesh(static_cast<int>(i));
	}
}

bool SNGrid::_is_boundary_corner(int gx, int gy, int gz, int8_t &r_required_density) const {
	int max_gx = grid_size.x * chunk_size.x;
	int max_gy = grid_size.y * chunk_size.y;
	int max_gz = grid_size.z * chunk_size.z;

	// Bottommost (Y=0) is always solid (density = -128, representing max solidity)
	// Topmost (Y=max) is always empty (density = 127, representing max air/outside)
	// X and Z horizontal boundaries are always empty (density = 127)
	if (gy == 0 || gy == max_gy || gx == 0 || gx == max_gx || gz == 0 || gz == max_gz) {
		r_required_density = (gy == 0) ? -128 : 127;
		return true;
	}

	return false;
}

void SNGrid::_initialize_boundaries(SNChunk &p_chunk) const {
	int nx = p_chunk.size_x + 1;
	int ny = p_chunk.size_y + 1;
	int nz = p_chunk.size_z + 1;

	for (int ly = 0; ly < ny; ly++) {
		for (int lz = 0; lz < nz; lz++) {
			for (int lx = 0; lx < nx; lx++) {
				int gx = (p_chunk.loc_x * p_chunk.size_x) + lx;
				int gy = (p_chunk.loc_y * p_chunk.size_y) + ly;
				int gz = (p_chunk.loc_z * p_chunk.size_z) + lz;

				int8_t required_density = 127;
				if (_is_boundary_corner(gx, gy, gz, required_density)) {
					p_chunk.set_corner(lx, ly, lz, required_density);
				} else {
					p_chunk.set_corner(lx, ly, lz, 127); // Empty by default
				}
			}
		}
	}
}

void SNGrid::generate_test_sdf() {
	int max_x = grid_size.x * chunk_size.x;
	int max_y = grid_size.y * chunk_size.y;
	int max_z = grid_size.z * chunk_size.z;

	// Center coordinates for sphere (left half) and box (right half)
	Vector3 sphere_center(static_cast<float>(max_x) * 0.3f, static_cast<float>(max_y) * 0.5f, static_cast<float>(max_z) * 0.5f);
	float sphere_radius = static_cast<float>(max_y) * 0.35f;

	Vector3 box_center(static_cast<float>(max_x) * 0.7f, static_cast<float>(max_y) * 0.5f, static_cast<float>(max_z) * 0.5f);
	Vector3 box_half_size(static_cast<float>(max_y) * 0.25f, static_cast<float>(max_y) * 0.25f, static_cast<float>(max_y) * 0.25f);

	for (SNChunk &chunk : chunks) {
		int nx = chunk.size_x + 1;
		int ny = chunk.size_y + 1;
		int nz = chunk.size_z + 1;

		for (int ly = 0; ly < ny; ly++) {
			for (int lz = 0; lz < nz; lz++) {
				for (int lx = 0; lx < nx; lx++) {
					int gx = (chunk.loc_x * chunk.size_x) + lx;
					int gy = (chunk.loc_y * chunk.size_y) + ly;
					int gz = (chunk.loc_z * chunk.size_z) + lz;

					// Check boundary first
					int8_t req_density = 127;
					if (_is_boundary_corner(gx, gy, gz, req_density)) {
						chunk.set_corner(lx, ly, lz, req_density);
						continue;
					}

					Vector3 p(static_cast<float>(gx), static_cast<float>(gy), static_cast<float>(gz));

					// Sphere SDF
					float d_sphere = p.distance_to(sphere_center) - sphere_radius;

					// Box SDF (Chebyshev distance approximation)
					float dx = std::abs(p.x - box_center.x) - box_half_size.x;
					float dy = std::abs(p.y - box_center.y) - box_half_size.y;
					float dz = std::abs(p.z - box_center.z) - box_half_size.z;
					float d_box = std::max(dx, std::max(dy, dz));

					// Union
					float d = std::min(d_sphere, d_box);

					// Scale and clamp to int8_t
					float scaled = d * 16.0f;
					int8_t density = static_cast<int8_t>(std::clamp(scaled, -128.0f, 127.0f));

					chunk.set_corner(lx, ly, lz, density);
				}
			}
		}
	}

	refresh_grid();
}

void SNGrid::_update_chunk_mesh(int p_chunk_idx) {
	SNChunk &chunk = chunks[static_cast<size_t>(p_chunk_idx)];
	_clear_chunk_visuals(chunk);

	Vector3i chunk_loc(chunk.loc_x, chunk.loc_y, chunk.loc_z);

	SurfaceNets::MeshData data = SurfaceNets::generate_mesh(this, chunk_loc, chunk_size);
	if (data.vertices.is_empty() || data.indices.is_empty()) {
		return;
	}

	// Assemble and Spawn Godot Mesh
	Array arrays;
	arrays.resize(Mesh::ARRAY_MAX);
	arrays[Mesh::ARRAY_VERTEX] = data.vertices;
	arrays[Mesh::ARRAY_NORMAL] = data.normals;
	arrays[Mesh::ARRAY_INDEX] = data.indices;

	Ref<ArrayMesh> am;
	am.instantiate();
	am->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, arrays);

	MeshInstance3D *mi = memnew(MeshInstance3D);
	mi->set_mesh(am);
	if (terrain_material.is_valid()) {
		mi->set_material_override(terrain_material);
	}
	add_child(mi);
	if (is_inside_tree()) {
		mi->set_owner(get_owner() ? get_owner() : this);
	}
	chunk.visual_node = mi;

	// Generate Concave Collision Shape
	StaticBody3D *sb = memnew(StaticBody3D);
	CollisionShape3D *cs = memnew(CollisionShape3D);
	Ref<ConcavePolygonShape3D> shape;
	shape.instantiate();

	PackedVector3Array faces;
	faces.resize(data.indices.size());
	for (int i = 0; i < data.indices.size(); ++i) {
		faces[i] = data.vertices[data.indices[i]];
	}
	shape->set_faces(faces);
	cs->set_shape(shape);
	sb->add_child(cs);

	sb->set_collision_layer(16); // Terrain layer
	sb->set_collision_mask(0);

	add_child(sb);
	if (is_inside_tree()) {
		sb->set_owner(get_owner() ? get_owner() : this);
		cs->set_owner(get_owner() ? get_owner() : this);
	}
	chunk.collision_body = sb;
}

void SNGrid::modify_density(const Vector3i &p_grid_pos, int8_t p_density) {
	int gx = p_grid_pos.x;
	int gy = p_grid_pos.y;
	int gz = p_grid_pos.z;

	int8_t required_density = 127;
	if (_is_boundary_corner(gx, gy, gz, required_density)) {
		UtilityFunctions::print("SNGrid: Cannot modify boundary corner at ", p_grid_pos);
		return;
	}

	bool actual_modified = false;

	// Shared chunk ranges
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
				SNChunk &chunk = chunks[static_cast<size_t>(idx)];
				chunk.set_corner(gx - (cx * chunk_size.x), gy - (cy * chunk_size.y), gz - (cz * chunk_size.z), p_density);
				actual_modified = true;
			}
		}
	}

	if (actual_modified) {
		for (int cy = start_cy; cy <= end_cy; cy++) {
			for (int cz = start_cz; cz <= end_cz; cz++) {
				for (int cx = start_cx; cx <= end_cx; cx++) {
					int idx = _get_chunk_index(cx, cy, cz);
					_update_chunk_mesh(idx);
				}
			}
		}
	}
}

int8_t SNGrid::get_density(const Vector3i &p_grid_pos) const {
	int cx = (p_grid_pos.x >= grid_size.x * chunk_size.x) ? grid_size.x - 1 : p_grid_pos.x / chunk_size.x;
	int cy = (p_grid_pos.y >= grid_size.y * chunk_size.y) ? grid_size.y - 1 : p_grid_pos.y / chunk_size.y;
	int cz = (p_grid_pos.z >= grid_size.z * chunk_size.z) ? grid_size.z - 1 : p_grid_pos.z / chunk_size.z;

	if (cx < 0 || cx >= grid_size.x || cy < 0 || cy >= grid_size.y || cz < 0 || cz >= grid_size.z) {
		return 127; // Outside the grid boundaries is considered air
	}

	int idx = _get_chunk_index(cx, cy, cz);
	return chunks[static_cast<size_t>(idx)].get_corner(p_grid_pos.x - (cx * chunk_size.x), p_grid_pos.y - (cy * chunk_size.y), p_grid_pos.z - (cz * chunk_size.z));
}

bool SNGrid::is_solid(const Vector3i &p_grid_pos) const {
	return get_density(p_grid_pos) < 0; // Negative values are inside/solid
}

void SNGrid::save_grid(const String &p_path) {
	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::WRITE);
	if (f.is_null()) {
		UtilityFunctions::print("SNGrid: Failed to open file for writing: ", p_path);
		return;
	}

	// 1. Header
	f->store_32(0x534E5452); // Magic: 'SNTR'
	f->store_32(1); // Version

	// 2. Metadata
	f->store_32(grid_size.x);
	f->store_32(grid_size.y);
	f->store_32(grid_size.z);
	f->store_32(chunk_size.x);
	f->store_32(chunk_size.y);
	f->store_32(chunk_size.z);

	// 3. Chunk Data
	f->store_32(static_cast<uint32_t>(chunks.size()));
	for (const SNChunk &chunk : chunks) {
		PackedByteArray rle_data = chunk.serialize_rle();
		f->store_32(static_cast<uint32_t>(rle_data.size()));
		f->store_buffer(rle_data);
	}

	UtilityFunctions::print("SNGrid: Saved state to ", p_path);
}

void SNGrid::load_grid(const String &p_path) {
	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::READ);
	if (f.is_null()) {
		UtilityFunctions::print("SNGrid: Failed to open file for reading: ", p_path);
		return;
	}

	// 1. Header
	uint32_t magic = f->get_32();
	if (magic != 0x534E5452) {
		UtilityFunctions::print("SNGrid: Invalid magic number in file: ", p_path);
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
	if (grid_size != Vector3i(tx, ty, tz) || chunk_size != Vector3i(cx, cy, cz)) {
		initialize_grid(tx, ty, tz, cx, cy, cz, false);
	}

	// 3. Chunk Data
	uint32_t num_chunks = f->get_32();
	if (num_chunks != chunks.size()) {
		UtilityFunctions::print("SNGrid: Chunk count mismatch in file.");
		return;
	}

	for (uint32_t i = 0; i < num_chunks; i++) {
		uint32_t data_size = f->get_32();
		PackedByteArray arr = f->get_buffer(data_size);
		chunks[i].deserialize_rle(arr);
	}

	UtilityFunctions::print("SNGrid: Loaded state from ", p_path);
	refresh_grid();
}

} // namespace godot
