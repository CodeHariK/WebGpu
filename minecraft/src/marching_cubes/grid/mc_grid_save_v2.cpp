// MCGrid: save format version 2 (.mct), plan in docs/TerrainStorage.md §8.
//
//   header   magic 'MCTR', version 2, mode (0 corner, 1 heightmap), lock_rim, decor_seed,
//            grid_dims xyz, chunk_dims xyz
//   blocks   count, then each block's byte size (so a reader can seek to any block), then the blocks
//   block    layer count, then per layer: id, encoding, raw size, stored size, bytes
//
// Blocks: corner mode = one per chunk (layer CORNERS: the chunk's RLE corner bits); heightmap mode =
// one per chunk column (x, z): the corner columns it owns (x in [cx * sx, cx * sx + sx), the last
// block of a row / column also the grid's far edge), with layers HEIGHTS and WATER (a byte per
// column each). Every layer is zstd-compressed when that makes it smaller (encoding 1, else 0 raw).
// A reader skips layer ids it doesn't know, so new layers (ground material, objects) don't break
// old readers. Everything is read and checked into temporaries first; the grid only changes once the
// whole file is good (as in version 1, mc_grid_io.cpp, which still loads).
#include "godot_cpp/classes/file_access.hpp"
#include "godot_cpp/variant/packed_byte_array.hpp"
#include "marching_cubes/grid/mc_grid.h"
#include <godot_cpp/variant/utility_functions.hpp>
#include <algorithm>
#include <vector>

namespace godot {

namespace {
const uint32_t MCT_MAGIC = 0x4D435452;
const uint32_t VERSION_2 = 2;
const int32_t MAX_AXIS = 1024; // a corrupt header can't allocate gigabytes
const uint8_t MODE_CORNER = 0;
const uint8_t MODE_HEIGHT = 1;
const uint8_t LAYER_CORNERS = 1; // corner mode: the chunk's RLE corner bits
const uint8_t LAYER_HEIGHTS = 2; // heightmap mode: a byte per owned corner column
const uint8_t LAYER_WATER = 3; // heightmap mode: water level per owned corner column
const uint8_t RAW = 0;
const uint8_t ZSTD = 1;

// One layer as stored: zstd if that's smaller.
void append_layer(
		PackedByteArray &r_block,
		uint8_t p_id,
		const PackedByteArray &p_raw
) {
	PackedByteArray packed = p_raw.compress(FileAccess::COMPRESSION_ZSTD);
	bool zstd = !packed.is_empty() && packed.size() < p_raw.size();
	const PackedByteArray &stored = zstd ? packed : p_raw;
	int64_t at = r_block.size();
	r_block.resize(at + 10);
	r_block.encode_u8(at, p_id);
	r_block.encode_u8(at + 1, zstd ? ZSTD : RAW);
	r_block.encode_u32(at + 2, static_cast<uint32_t>(p_raw.size()));
	r_block.encode_u32(at + 6, static_cast<uint32_t>(stored.size()));
	r_block.append_array(stored);
}

// The layers of one block: id -> raw bytes (unknown ids are kept but never read). False = corrupt.
bool read_layers(
		const PackedByteArray &p_block,
		std::vector<std::pair<uint8_t, PackedByteArray>> &r_layers
) {
	if (p_block.size() < 1) {
		return false;
	}
	int count = p_block.decode_u8(0);
	int64_t at = 1;
	for (int i = 0; i < count; i++) {
		if (at + 10 > p_block.size()) {
			return false;
		}
		uint8_t id = p_block.decode_u8(at);
		uint8_t encoding = p_block.decode_u8(at + 1);
		uint32_t raw_size = p_block.decode_u32(at + 2);
		uint32_t stored_size = p_block.decode_u32(at + 6);
		at += 10;
		if (at + stored_size > p_block.size() || raw_size > (64u << 20)) {
			return false;
		}
		PackedByteArray stored = p_block.slice(at, at + stored_size);
		at += stored_size;
		PackedByteArray raw = encoding == ZSTD ? stored.decompress(raw_size, FileAccess::COMPRESSION_ZSTD) : stored;
		if (static_cast<uint32_t>(raw.size()) != raw_size) {
			return false;
		}
		r_layers.emplace_back(id, raw);
	}
	return true;
}
} // namespace

// The corner columns a chunk column (block) owns: [r_from, r_to) on x and z.
void MCGrid::_block_columns(
		int p_cx,
		int p_cz,
		Vector2i &r_from,
		Vector2i &r_to
) const {
	Vector2i columns = _column_counts();
	r_from = Vector2i(p_cx * chunk_dims.x, p_cz * chunk_dims.z);
	r_to = Vector2i(p_cx == grid_dims.x - 1 ? columns.x : r_from.x + chunk_dims.x,
			p_cz == grid_dims.z - 1 ? columns.y : r_from.y + chunk_dims.z);
}

void MCGrid::_save_v2(const Ref<FileAccess> &p_file) const {
	std::vector<PackedByteArray> blocks;
	if (heightmap_mode && column_heights.size() == water_levels.size() && !column_heights.empty()) {
		for (int cz = 0; cz < grid_dims.z; cz++) {
			for (int cx = 0; cx < grid_dims.x; cx++) {
				Vector2i from, to;
				_block_columns(cx, cz, from, to);
				PackedByteArray heights, water;
				for (int z = from.y; z < to.y; z++) {
					for (int x = from.x; x < to.x; x++) {
						size_t i = static_cast<size_t>(_column_index(x, z));
						heights.push_back(column_heights[i]);
						water.push_back(water_levels[i]);
					}
				}
				PackedByteArray block;
				block.push_back(2); // layers
				append_layer(block, LAYER_HEIGHTS, heights);
				append_layer(block, LAYER_WATER, water);
				blocks.push_back(block);
			}
		}
	} else {
		for (const Chunk &chunk : chunks) {
			PackedByteArray block;
			block.push_back(1);
			append_layer(block, LAYER_CORNERS, chunk.serialize_rle());
			blocks.push_back(block);
		}
	}
	p_file->store_32(MCT_MAGIC);
	p_file->store_32(VERSION_2);
	p_file->store_8(heightmap_mode ? MODE_HEIGHT : MODE_CORNER);
	p_file->store_8(lock_rim ? 1 : 0);
	p_file->store_32(static_cast<uint32_t>(decor_seed));
	for (int axis = 0; axis < 3; axis++) {
		p_file->store_32(static_cast<uint32_t>(grid_dims[axis]));
	}
	for (int axis = 0; axis < 3; axis++) {
		p_file->store_32(static_cast<uint32_t>(chunk_dims[axis]));
	}
	p_file->store_32(static_cast<uint32_t>(blocks.size()));
	for (const PackedByteArray &block : blocks) {
		p_file->store_32(static_cast<uint32_t>(block.size()));
	}
	for (const PackedByteArray &block : blocks) {
		p_file->store_buffer(block);
	}
}

bool MCGrid::_load_v2(
		const Ref<FileAccess> &p_file,
		const String &p_path
) {
	// 1. Header (magic and version already read)
	uint8_t mode = p_file->get_8();
	bool file_lock_rim = p_file->get_8() != 0;
	int file_seed = static_cast<int32_t>(p_file->get_32());
	Vector3i file_grid, file_chunk;
	for (int axis = 0; axis < 3; axis++) {
		file_grid[axis] = static_cast<int32_t>(p_file->get_32());
	}
	for (int axis = 0; axis < 3; axis++) {
		file_chunk[axis] = static_cast<int32_t>(p_file->get_32());
	}
	for (int axis = 0; axis < 3; axis++) {
		if (file_grid[axis] < 1 || file_grid[axis] > MAX_AXIS || file_chunk[axis] < 1 || file_chunk[axis] > MAX_AXIS) {
			UtilityFunctions::push_error("MCGrid: corrupt terrain file (bad sizes): ", p_path);
			return false;
		}
	}
	if (mode != MODE_CORNER && mode != MODE_HEIGHT) {
		UtilityFunctions::push_error("MCGrid: corrupt terrain file (unknown mode ", mode, "): ", p_path);
		return false;
	}
	uint64_t expected = mode == MODE_HEIGHT ? static_cast<uint64_t>(file_grid.x) * file_grid.z
											: static_cast<uint64_t>(file_grid.x) * file_grid.y * file_grid.z;
	uint32_t count = p_file->get_32();
	if (count != expected) {
		UtilityFunctions::push_error("MCGrid: corrupt terrain file (block count mismatch): ", p_path);
		return false;
	}
	// 2. Block sizes, then every block's layers into temporaries
	std::vector<uint32_t> sizes(count);
	uint64_t total = 0;
	for (uint32_t i = 0; i < count; i++) {
		sizes[i] = p_file->get_32();
		total += sizes[i];
	}
	if (p_file->get_position() + total > p_file->get_length()) {
		UtilityFunctions::push_error("MCGrid: corrupt terrain file (truncated blocks): ", p_path);
		return false;
	}
	std::vector<std::vector<std::pair<uint8_t, PackedByteArray>>> blocks(count);
	for (uint32_t i = 0; i < count; i++) {
		if (!read_layers(p_file->get_buffer(sizes[i]), blocks[i])) {
			UtilityFunctions::push_error("MCGrid: corrupt terrain file (bad block ", i, "): ", p_path);
			return false;
		}
	}

	// 3. Apply: same sizes as the file (that drops placed objects only when the size changes)
	bool resize = grid_dims != file_grid || chunk_dims != file_chunk;
	heightmap_mode = mode == MODE_HEIGHT;
	lock_rim = file_lock_rim;
	decor_seed = file_seed;
	if (resize) {
		initialize_grid(file_grid.x, file_grid.y, file_grid.z, file_chunk.x, file_chunk.y, file_chunk.z, false);
	}
	if (!heightmap_mode) {
		for (uint32_t i = 0; i < count; i++) {
			for (const auto &layer : blocks[i]) {
				if (layer.first == LAYER_CORNERS) {
					chunks[i].deserialize_rle(layer.second);
				}
			}
		}
		return true;
	}
	Vector2i columns = _column_counts();
	column_heights.assign(static_cast<size_t>(columns.x) * columns.y, 0);
	water_levels.assign(column_heights.size(), 0);
	uint32_t i = 0;
	for (int cz = 0; cz < grid_dims.z; cz++) {
		for (int cx = 0; cx < grid_dims.x; cx++, i++) {
			Vector2i from, to;
			_block_columns(cx, cz, from, to);
			size_t owned = static_cast<size_t>(to.x - from.x) * (to.y - from.y);
			for (const auto &layer : blocks[i]) {
				std::vector<uint8_t> *target = layer.first == LAYER_HEIGHTS ? &column_heights
						: layer.first == LAYER_WATER						 ? &water_levels
																			 : nullptr;
				if (!target) {
					continue; // a layer this version doesn't know
				}
				if (static_cast<size_t>(layer.second.size()) != owned) {
					UtilityFunctions::push_error("MCGrid: terrain file block ", i, " has the wrong column count: ", p_path);
					continue;
				}
				size_t k = 0;
				for (int z = from.y; z < to.y; z++) {
					for (int x = from.x; x < to.x; x++, k++) {
						(*target)[static_cast<size_t>(_column_index(x, z))] = layer.second[static_cast<int64_t>(k)];
					}
				}
			}
		}
	}
	int top = _max_column_height();
	for (int z = 0; z < columns.y; z++) {
		for (int x = 0; x < columns.x; x++) {
			size_t index = static_cast<size_t>(_column_index(x, z));
			column_heights[index] = static_cast<uint8_t>(std::clamp<int>(column_heights[index], _min_column_height(), top));
			_write_column(x, z, column_heights[index]);
		}
	}
	water_dirty = true;
	return true;
}

} // namespace godot
