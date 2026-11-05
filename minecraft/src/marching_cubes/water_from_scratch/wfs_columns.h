// Water from scratch: the column grid every step reads.
//
// The terrain in heightmap mode is a grid of corner columns, each with a ground height (an integer
// number of tiers). Water is stored the same way: each column has a water level; the column is wet
// when its level is above its ground (level > height). That is all the input the water needs.
//
//   x  0 .. width - 1,  z  0 .. depth - 1,  index = z * width + x
#ifndef WFS_COLUMNS_H
#define WFS_COLUMNS_H

#include <cstdint>
#include <vector>

namespace godot {

struct WfsColumns {
	int width = 0; // columns along x
	int depth = 0; // columns along z
	int top = 0; // the highest height a column can have
	int sea_level = 0; // water at this level below it is the sea (0: no sea)
	std::vector<int> heights; // ground height per column
	std::vector<int> levels; // water level per column (0: dry)

	bool inside(
			int p_x,
			int p_z
	) const {
		return p_x >= 0 && p_z >= 0 && p_x < width && p_z < depth;
	}
	int index(
			int p_x,
			int p_z
	) const {
		return p_z * width + p_x;
	}
	bool is_wet(int p_index) const { return levels[static_cast<size_t>(p_index)] > heights[static_cast<size_t>(p_index)]; }
	bool is_sea(int p_index) const {
		return sea_level > 0 && levels[static_cast<size_t>(p_index)] == sea_level &&
				heights[static_cast<size_t>(p_index)] < sea_level;
	}
};

} // namespace godot

#endif // WFS_COLUMNS_H
