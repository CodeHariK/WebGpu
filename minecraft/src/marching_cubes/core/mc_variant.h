// Mesh variants: several meshes modelled for the same hash, tagged by height, group and number
// ("00001111_ma1_circus", "00001111_ma2_circus", "00001111_hb1_circus"; older sets: "..._v2").
// Each cell picks a number from its grid position, so the choice is random-looking but never
// changes between rebuilds or loads. The Blender add-on (blender/mc_tiles/mc_tile_tools/variants.py)
// uses the same tag rules and the same formula, so its playground shows the same variant per cell.
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

// Old-style variant number: "..._v2" -> 2, no suffix -> 1. A duplicate-name ending is ignored:
// Blender's ".001", which Godot's glTF import turns into "_001"
// ("00001111_v2.001" and "00001111_v2_001" -> 2).
inline int legacy_variant_number(const String &p_name) {
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

// A tile's tag: the "_<height><group><number>" token after the hash digits
// ("00001111_ma2_circus": middle, group a, number 2). Same rules as the add-on's name_tag:
//   height  'l' ground, 'm' middle, 'h' roof
//   group   'a'..'z' style family
//   number  interchangeable alternatives, 1, 2, ...
// No tag: the old "_v2" ending gives ('m', 'a', 2), a bare hash ('m', 'a', 1).
struct TileTag {
	char height = 'm';
	char group = 'a';
	int number = 1;

	// The main role (M a): the only tiles MCTileLibrary draws until L / H are placed by grid rules.
	bool is_main_role() const { return height == 'm' && group == 'a'; }
};

// True for "ma2", "hb10": a height letter, a group letter, then only digits.
inline bool is_tag_token(const String &p_token) {
	if (p_token.length() < 3) {
		return false;
	}
	char32_t height = p_token[0];
	char32_t group = p_token[1];
	if ((height != 'l' && height != 'm' && height != 'h') || group < 'a' || group > 'z') {
		return false;
	}
	for (int i = 2; i < p_token.length(); i++) {
		if (p_token[i] < '0' || p_token[i] > '9') {
			return false;
		}
	}
	return true;
}

// The tag of a tile name; the first tag-shaped token after the hash wins (the set name comes later).
inline TileTag tile_tag(const String &p_name) {
	String name = p_name.get_slice(".", 0);
	int count = name.get_slice_count("_");
	for (int i = 1; i < count; i++) {
		String token = name.get_slice("_", i);
		if (is_tag_token(token)) {
			TileTag tag;
			tag.height = static_cast<char>(token[0]);
			tag.group = static_cast<char>(token[1]);
			int number = token.substr(2).to_int();
			tag.number = number >= 1 ? number : 1;
			return tag;
		}
	}
	TileTag tag;
	tag.number = legacy_variant_number(name);
	return tag;
}

// Variant number from a tile name: the tag's number ("_ma2" -> 2), or "_v2" -> 2, else 1.
inline int variant_number(const String &p_name) { return tile_tag(p_name).number; }

} // namespace godot::mc

#endif // MC_VARIANT_H
