// Binary-string conversion and bit counting for cell hashes.
#include "marching_cubes/core/mc_hash.h"

namespace godot::mc {

uint8_t binary_to_hash(const String &p_bin) {
	uint8_t h = 0;
	for (int i = 0; i < 8; i++) {
		if (p_bin[7 - i] == '1') {
			h |= (1 << i);
		}
	}
	return h;
}

String hash_to_binary(uint8_t p_hash) {
	String s = "";
	for (int i = 7; i >= 0; i--) {
		s += (p_hash & (1 << i)) ? "1" : "0";
	}
	return s;
}

int corner_count(uint8_t p_hash) {
	int count = 0;
	for (int i = 0; i < 8; i++) {
		count += (p_hash >> i) & 1;
	}
	return count;
}

} // namespace godot::mc
