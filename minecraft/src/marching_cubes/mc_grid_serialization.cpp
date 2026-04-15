#include "godot_cpp/classes/file_access.hpp"
#include "godot_cpp/variant/packed_byte_array.hpp"

#include "mc_grid.h"

namespace godot {

PackedByteArray Chunk::serialize_rle() const {
	int num_corners = (size_x + 1) * (size_y + 1) * (size_z + 1);
	return RLE::encode_bit_rle(num_corners, [this](int i) {
		return get_corner_bit(i);
	});
}

void Chunk::deserialize_rle(const PackedByteArray &p_data) {
	int num_corners = (size_x + 1) * (size_y + 1) * (size_z + 1);
	int num_bytes = (num_corners + 7) / 8;
	corner_states.assign(static_cast<size_t>(num_bytes), 0);

	RLE::decode_bit_rle(p_data, num_corners, [this](int i, bool state) {
		set_corner_bit(i, state);
	});
}

void MCGrid::save_grid(const String &p_path) {
	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::WRITE);
	if (f.is_null()) {
		UtilityFunctions::print("MCGrid: Failed to open file for writing: ", p_path);
		return;
	}

	// 1. Header
	f->store_32(0x4D435452); // Magic: 'MCTR'
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
	for (const Chunk &chunk : chunks) {
		PackedByteArray rle_data = chunk.serialize_rle();
		f->store_32(static_cast<uint32_t>(rle_data.size()));
		f->store_buffer(rle_data);
	}

	UtilityFunctions::print("MCGrid: Saved state to ", p_path);
}

void MCGrid::load_grid(const String &p_path) {
	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::READ);
	if (f.is_null()) {
		UtilityFunctions::print("MCGrid: Failed to open file for reading: ", p_path);
		return;
	}

	// 1. Header
	uint32_t magic = f->get_32();
	if (magic != 0x4D435452) {
		UtilityFunctions::print("MCGrid: Invalid magic number in file: ", p_path);
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
		initialize_grid(tx, ty, tz, cx, cy, cz);
	}

	// 3. Chunk Data
	uint32_t num_chunks = f->get_32();
	if (num_chunks != chunks.size()) {
		UtilityFunctions::print("MCGrid: Chunk count mismatch in file.");
		return;
	}

	for (uint32_t i = 0; i < num_chunks; i++) {
		uint32_t data_size = f->get_32();
		PackedByteArray arr = f->get_buffer(data_size);
		chunks[i].deserialize_rle(arr);
	}

	UtilityFunctions::print("MCGrid: Loaded state from ", p_path);
	refresh_grid();
}
} //namespace godot
