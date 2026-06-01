/**
 * @file mp_grid_serialization.cpp
 * @brief Handles saving and loading the Marching Prism grid configurations.
 * Module Path: src/terrain/marching_prism/mp_grid_serialization.cpp
 * Build Dependencies: godot-cpp, mp_grid.h
 */

#include "mp_grid.h"
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

// Saves the entire grid data to a binary file at the specified path.
void MPGrid::save_grid(const String &p_path) {
	Ref<FileAccess> file = FileAccess::open(p_path, FileAccess::WRITE);
	if (file.is_null()) {
		UtilityFunctions::print("MPGrid: Failed to write ", p_path);
		return;
	}

	file->store_32(0x50524953); // Magic: 'PRIS'
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

// Loads the entire grid data from a binary file at the specified path.
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
