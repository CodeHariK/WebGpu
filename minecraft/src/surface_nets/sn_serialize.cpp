#include "surface_nets/sn_grid.h"
#include <godot_cpp/classes/file_access.hpp>

namespace godot {

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

	ClassDB::bind_method(D_METHOD("set_terrain_noise", "noise"), &SNGrid::set_terrain_noise);
	ClassDB::bind_method(D_METHOD("get_terrain_noise"), &SNGrid::get_terrain_noise);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "terrain_noise", PROPERTY_HINT_RESOURCE_TYPE, "Noise"), "set_terrain_noise", "get_terrain_noise");

	ClassDB::bind_method(D_METHOD("set_cell_center", "enabled"), &SNGrid::set_cell_center);
	ClassDB::bind_method(D_METHOD("is_cell_center"), &SNGrid::is_cell_center);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "cell_center"), "set_cell_center", "is_cell_center");

	ClassDB::bind_method(D_METHOD("set_smooth_normal", "enabled"), &SNGrid::set_smooth_normal);
	ClassDB::bind_method(D_METHOD("is_smooth_normal"), &SNGrid::is_smooth_normal);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "smooth_normal"), "set_smooth_normal", "is_smooth_normal");

	ClassDB::bind_method(D_METHOD("generate_test_sdf"), &SNGrid::generate_test_sdf);
	ClassDB::bind_method(D_METHOD("generate_noise_sdf"), &SNGrid::generate_noise_sdf);

	ClassDB::bind_method(D_METHOD("save_grid", "path"), &SNGrid::save_grid);
	ClassDB::bind_method(D_METHOD("load_grid", "path"), &SNGrid::load_grid);
}

} //namespace godot