// Chunk save/load as bit run-length encoding.
#include "marching_cubes/core/mc_chunk.h"

namespace godot {

// All (size + 1)^3 corner bits as bit run-length encoding.
PackedByteArray Chunk::serialize_rle() const {
	int num_corners = (size_x + 1) * (size_y + 1) * (size_z + 1);
	return RLE::encode_bit_rle(num_corners, [this](int i) { return get_corner_bit(i); });
}

// Reset the corner bits and fill them from run-length encoded p_data.
void Chunk::deserialize_rle(const PackedByteArray &p_data) {
	int num_corners = (size_x + 1) * (size_y + 1) * (size_z + 1);
	int num_bytes = (num_corners + 7) / 8;
	corner_bits.assign(static_cast<size_t>(num_bytes), 0);

	RLE::decode_bit_rle(p_data, num_corners, [this](int i, bool state) { set_corner_bit(i, state); });
}

} // namespace godot
