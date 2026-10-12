// Binary-string conversion and bit counting for cell hashes.
#include "marching_cubes/core/mc_hash.h"

namespace godot::mc {

// "00001111" -> hash: character 7 - i is corner i.
uint8_t binary_to_hash(const String &p_bin) {
	uint8_t h = 0;
	for (int i = 0; i < 8; i++) {
		if (p_bin[7 - i] == '1') {
			h |= (1 << i);
		}
	}
	return h;
}

// hash -> "00001111" (bit 7 first).
String hash_to_binary(uint8_t p_hash) {
	String s = "";
	for (int i = 7; i >= 0; i--) {
		s += (p_hash & (1 << i)) ? "1" : "0";
	}
	return s;
}

// Number of solid corners (set bits).
int corner_count(uint8_t p_hash) {
	int count = 0;
	for (int i = 0; i < 8; i++) {
		count += (p_hash >> i) & 1;
	}
	return count;
}

} // namespace godot::mc
