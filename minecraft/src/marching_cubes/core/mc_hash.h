// Small helpers for 8-bit cell hashes: binary strings (as used in tile mesh names) and bit counts.
#ifndef MC_HASH_H
#define MC_HASH_H

#include <cstdint>
#include <godot_cpp/variant/string.hpp>

namespace godot::mc {

// "00001111" (bit 7 first, as in the .glb mesh names) -> hash. Reads the first 8 characters.
uint8_t binary_to_hash(const String &p_bin);

// hash -> "00001111" (bit 7 first).
String hash_to_binary(uint8_t p_hash);

// How many corners are solid.
int corner_count(uint8_t p_hash);

} // namespace godot::mc

#endif // MC_HASH_H
