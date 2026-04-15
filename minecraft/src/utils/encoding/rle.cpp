#include "utils/encoding/rle.h"

namespace godot {

PackedByteArray RLE::encode_bit_rle(int p_num_bits, const BitGetter &p_getter) {
	PackedByteArray output;
	if (p_num_bits == 0) {
		return output;
	}

	bool current_state = p_getter(0);
	uint32_t run_length = 0;

	auto write_run = [&](bool state, uint32_t length) {
		if (length < 64) {
			uint8_t b = (state ? 0x80 : 0x00) | (length & 0x3F);
			output.push_back(b);
		} else {
			// Long run: [S][1][Length_High:6] [Length_Low:8]
			uint8_t b1 = (state ? 0x80 : 0x00) | 0x40 | ((length >> 8) & 0x3F);
			uint8_t b2 = length & 0xFF;
			output.push_back(b1);
			output.push_back(b2);
		}
	};

	for (int i = 0; i < p_num_bits; i++) {
		bool state = p_getter(i);
		if (state == current_state) {
			run_length++;
			// Max 14-bit length is 16383
			if (run_length == 16383) {
				write_run(current_state, run_length);
				run_length = 0;
			}
		} else {
			if (run_length > 0) {
				write_run(current_state, run_length);
			}
			current_state = state;
			run_length = 1;
		}
	}

	if (run_length > 0) {
		write_run(current_state, run_length);
	}

	return output;
}

void RLE::decode_bit_rle(const PackedByteArray &p_data, int p_num_bits, const BitSetter &p_setter) {
	int corner_index = 0;
	int byte_index = 0;

	while (byte_index < p_data.size() && corner_index < p_num_bits) {
		uint8_t b1 = p_data[byte_index++];
		bool state = (b1 & 0x80) != 0;
		bool is_long = (b1 & 0x40) != 0;
		uint32_t length = 0;

		if (is_long) {
			if (byte_index >= p_data.size()) {
				break;
			}
			uint8_t b2 = p_data[byte_index++];
			length = ((static_cast<uint32_t>(b1) & 0x3F) << 8) | b2;
		} else {
			length = b1 & 0x3F;
		}

		for (uint32_t i = 0; i < length && corner_index < p_num_bits; i++) {
			p_setter(corner_index++, state);
		}
	}
}

} // namespace godot
