// Mesh variants: several meshes modelled for the same hash ("00001111_circle", "..._v2", "..._v3").
// Each cell picks one from its grid position, so the choice is random-looking but never changes
// between rebuilds or loads. The Blender add-on (blender/mc_tiles/mc_tile_tools/variants.py) uses
// the same formula, so its playground shows the same variant per cell as the game.
#ifndef MC_VARIANT_H
#define MC_VARIANT_H

#include <cstdint>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/vector3i.hpp>

namespace godot::mc {

// A 32-bit hash of a cell position (spatial hash + MurmurHash3 finaliser, so neighbouring cells
// don't fall into stripes or checkerboards).
inline uint32_t cell_seed(const Vector3i &p_cell) {
	uint32_t h = (static_cast<uint32_t>(p_cell.x) * 73856093u) ^ (static_cast<uint32_t>(p_cell.y) * 19349663u) ^
			(static_cast<uint32_t>(p_cell.z) * 83492791u);
	h ^= h >> 16;
	h *= 0x85ebca6bu;
	h ^= h >> 13;
	h *= 0xc2b2ae35u;
	h ^= h >> 16;
	return h;
}

// Which of p_count variants a cell uses (0 when there is only one).
inline int variant_index(
		const Vector3i &p_cell,
		int p_count
) {
	return p_count <= 1 ? 0 : static_cast<int>(cell_seed(p_cell) % static_cast<uint32_t>(p_count));
}

// Variant number from a tile name: "..._v2" -> 2, no suffix -> 1. A duplicate-name ending is
// ignored: Blender's ".001", which Godot's glTF import turns into "_001"
// ("00001111_v2.001" and "00001111_v2_001" -> 2).
inline int variant_number(const String &p_name) {
	String name = p_name.get_slice(".", 0);
	int at = name.rfind("_v");
	if (at < 0) {
		return 1;
	}
	String rest = name.substr(at + 2); // "2" or "2_001"
	String digits = rest.get_slice("_", 0);
	String tail = rest.get_slice_count("_") > 1 ? rest.substr(digits.length() + 1) : String();
	if (digits.is_empty() || !digits.is_valid_int() || (!tail.is_empty() && !tail.is_valid_int()) ||
		rest.get_slice_count("_") > 2) {
		return 1;
	}
	int number = digits.to_int();
	return number >= 1 ? number : 1;
}

} // namespace godot::mc

#endif // MC_VARIANT_H
