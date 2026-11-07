// One undoable terrain / water edit (MCGrid's undo, mc_grid_undo.cpp).
#ifndef MC_EDIT_H
#define MC_EDIT_H

#include <cstdint>
#include <utility>
#include <vector>

namespace godot {

// One undoable edit: the old values it changed (index, value), and the decor seed and M roofs as
// they were when it began (mc_grid_undo.cpp).
struct MCEdit {
	std::vector<std::pair<int, uint8_t>> heights; // corner column index -> old height
	std::vector<std::pair<int, uint8_t>> water; // corner column index -> old water level
	std::vector<std::pair<int, uint8_t>> corners; // global corner key (x fastest) -> old solid bit
	int decor_seed = 0;
	std::vector<uint8_t> roof_m;
};

} // namespace godot

#endif // MC_EDIT_H
