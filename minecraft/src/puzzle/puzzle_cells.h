#ifndef PUZZLE_CELLS_H
#define PUZZLE_CELLS_H

namespace godot {

/**
 * PuzzleCell — the cell kinds PuzzleGrid keeps per grid cell, and the rules shared by
 * building, pushing and room detection. Layout characters map onto them in from_layout().
 */
namespace PuzzleCell {

constexpr char VOID = ' '; ///< outside the level
constexpr char WALL = '#';
constexpr char DOOR = 'D'; ///< exit door: solid like a wall
constexpr char FLOOR = '.'; ///< floor, start, and the cell under a starting box
constexpr char PROP = 'o'; ///< floor with a key / alien on it (boxes can't enter)
constexpr char BRIDGE = '=';
constexpr char ICE = 'i'; ///< a box pushed onto ice keeps sliding
constexpr char WATER = 'w'; ///< not walkable; a box pushed in sinks and fills it
constexpr char LAVA = 'l'; ///< not walkable; a box pushed in burns
constexpr char FILLED = 'f'; ///< water filled by a sunk box: walkable floor now

/// Island floors sit this high above the grid origin; liquids lie below.
constexpr float FLOOR_TOP = 0.4f;

inline char from_layout(char32_t p_c) {
	switch (p_c) {
		case '#':
			return WALL;
		case 'E':
			return DOOR;
		case 'K':
		case 'A':
			return PROP;
		case '~':
			return WATER;
		case '^':
			return LAVA;
		case '=':
			return BRIDGE;
		case 'I':
			return ICE;
		case ' ':
			return VOID;
		default:
			return FLOOR; // '.', 'P', 'B', 'X'
	}
}

/// The player can stand here (props aside).
inline bool is_ground(char p_k) {
	return p_k == FLOOR || p_k == PROP || p_k == BRIDGE || p_k == ICE || p_k == FILLED;
}
/// A pushed box may come to rest here.
inline bool box_can_rest(char p_k) { return p_k == FLOOR || p_k == BRIDGE || p_k == ICE || p_k == FILLED; }
inline bool is_liquid(char p_k) { return p_k == WATER || p_k == LAVA; }
inline bool is_solid(char p_k) { return p_k == WALL || p_k == DOOR; }

} // namespace PuzzleCell

} // namespace godot

#endif // PUZZLE_CELLS_H
