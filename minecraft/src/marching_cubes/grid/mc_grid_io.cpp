// MCGrid: save / load the corner grid.
// File layout (version 1): magic 'MCTR', version, grid_size xyz, chunk_size xyz, chunk count,
// then per chunk: byte length + RLE bytes. Everything is validated and read into temporaries
// first; the grid is only changed once the whole file is known to be good.
#include "godot_cpp/classes/file_access.hpp"
#include "godot_cpp/variant/packed_byte_array.hpp"
#include "marching_cubes/grid/mc_grid.h"
#include <godot_cpp/variant/utility_functions.hpp>
#include <vector>

namespace godot {

static const uint32_t MCT_MAGIC = 0x4D435452;
static const uint32_t MCT_VERSION = 1;
static const int32_t MCT_MAX_AXIS = 1024; // generous; stops a corrupt header allocating gigabytes

void MCGrid::save_grid(const String &p_path) {
	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::WRITE);
	if (f.is_null()) {
		UtilityFunctions::print("MCGrid: Failed to open file for writing: ", p_path);
		return;
	}

	// 1. Header
	f->store_32(MCT_MAGIC);
	f->store_32(MCT_VERSION);

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
	if (f->get_32() != MCT_MAGIC) {
		UtilityFunctions::push_error("MCGrid: not a terrain file (bad magic): ", p_path);
		return;
	}
	uint32_t version = f->get_32();
	if (version != MCT_VERSION) {
		UtilityFunctions::push_error("MCGrid: unsupported terrain file version ", version, " in ", p_path);
		return;
	}

	// 2. Metadata: every axis 1..MCT_MAX_AXIS, and the chunk count must match
	Vector3i file_grid(
			static_cast<int32_t>(f->get_32()), static_cast<int32_t>(f->get_32()), static_cast<int32_t>(f->get_32())
	);
	Vector3i file_chunk(
			static_cast<int32_t>(f->get_32()), static_cast<int32_t>(f->get_32()), static_cast<int32_t>(f->get_32())
	);
	for (int axis = 0; axis < 3; axis++) {
		if (file_grid[axis] < 1 || file_grid[axis] > MCT_MAX_AXIS || file_chunk[axis] < 1 ||
			file_chunk[axis] > MCT_MAX_AXIS) {
			UtilityFunctions::push_error("MCGrid: corrupt terrain file (bad sizes): ", p_path);
			return;
		}
	}
	uint64_t expected_chunks = static_cast<uint64_t>(file_grid.x) * file_grid.y * file_grid.z;
	uint32_t num_chunks = f->get_32();
	if (num_chunks != expected_chunks) {
		UtilityFunctions::push_error("MCGrid: corrupt terrain file (chunk count mismatch): ", p_path);
		return;
	}

	// 3. Chunk data into temporaries, each length checked against what is left in the file
	std::vector<PackedByteArray> chunk_data(num_chunks);
	for (uint32_t i = 0; i < num_chunks; i++) {
		uint32_t data_size = f->get_32();
		if (f->get_position() + data_size > f->get_length()) {
			UtilityFunctions::push_error("MCGrid: corrupt terrain file (truncated chunk data): ", p_path);
			return;
		}
		chunk_data[i] = f->get_buffer(data_size);
	}

	// 4. Apply: resize only if needed (that also clears placed objects), then decode
	if (grid_size != file_grid || chunk_size != file_chunk) {
		initialize_grid(file_grid.x, file_grid.y, file_grid.z, file_chunk.x, file_chunk.y, file_chunk.z, false);
	}
	for (uint32_t i = 0; i < num_chunks; i++) {
		chunks[i].deserialize_rle(chunk_data[i]);
	}

	UtilityFunctions::print("MCGrid: Loaded state from ", p_path);
	refresh_grid();
}

} // namespace godot
