#ifndef SOKOBAN_GENERATOR_H
#define SOKOBAN_GENERATOR_H

#include <cstdint>
#include <string>
#include <vector>

namespace godot {

/**
 * SokobanGenerator — seeded room puzzles for PuzzleGrid (Dino and Aliens style).
 * -----------------------------------------------------------------------------
 * Plain C++; output is the same ASCII layout PuzzleGrid builds from.
 *
 *  1. Rooms: binary space partition of a width x height box into `rooms` rooms; every
 *     split leaves a one-cell doorway, so rooms form a tree and doorways are chokepoints.
 *  2. Goals: start room (+ entrance gap in the outer wall), key in the room farthest
 *     from the start, exit door in the outer wall of another room, aliens in a room.
 *  3. Blockers: every doorway on the start -> key -> exit route holds a crate, or (island
 *     levels) is a water gap with a crate lined up to be pushed in as a bridge; extra
 *     crates; box bombs a few cells from the aliens; water / lava pools; ice patches.
 *     Island look: the outer wall ring becomes water (or lava), entrance = a bridge.
 *  4. Verify: SokobanSolver must reach key then exit in >= min_pushes pushes, and be
 *     able to push a bomb next to an alien. Otherwise retry (same seed => same level).
 */
class SokobanGenerator {
public:
	struct Params {
		uint32_t seed = 1;
		int width = 21;
		int height = 13;
		int rooms = 4;
		int extra_crates = 3;
		int bombs = 2;
		int aliens = 2;
		int min_pushes = 3; ///< reject levels solvable with fewer pushes
		int ring = 1; ///< outer ring: 0 walls, 1 water (islands), 2 lava
		float gap_chance = 0.5f; ///< route doorway is a water gap (push a crate in) instead of a crate
		int pools = 2; ///< small water / lava pools inside rooms
		float lava_chance = 0.25f; ///< chance a pool is lava rather than water
		int ice_patches = 1; ///< ice patches (boxes slide)
		int max_attempts = 400;
	};

	struct Output {
		bool ok = false;
		std::vector<std::string> rows;
		int pushes = 0; ///< min pushes start -> key -> exit
		int bomb_pushes = 0; ///< min pushes to get a bomb next to an alien
		int attempts = 0;
		int states = 0; ///< solver states explored on the accepted level
		// Why earlier attempts were rejected (tuning aid).
		int rejected_layout = 0; ///< rooms / placement didn't fit
		int rejected_unsolvable = 0; ///< key + exit route not found
		int rejected_easy = 0; ///< solvable in fewer than min_pushes
		int rejected_bomb = 0; ///< no bomb can reach an alien
	};

	static Output generate(const Params &p_params);
};

} // namespace godot

#endif // SOKOBAN_GENERATOR_H
