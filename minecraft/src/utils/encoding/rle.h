#ifndef MC_RLE_H
#define MC_RLE_H

#include <godot_cpp/variant/packed_byte_array.hpp>
#include <functional>

namespace godot {

class RLE {
public:
	using BitGetter = std::function<bool(int)>;
	using BitSetter = std::function<void(int, bool)>;

	/**
	 * Encodes a bitstream using a variable-length RLE algorithm.
	 * Format: 
	 * [S:1][T:1][L:6]          (if T=0, Short Run, length L)
	 * [S:1][T:1][LH:6] [LL:8]  (if T=1, Long Run, length (LH << 8) | LL)
	 */
	static PackedByteArray encode_bit_rle(int p_num_bits, const BitGetter &p_getter);

	/**
	 * Decodes a bitstream from the RLE format.
	 */
	static void decode_bit_rle(const PackedByteArray &p_data, int p_num_bits, const BitSetter &p_setter);
};

} // namespace godot

#endif // MC_RLE_H
