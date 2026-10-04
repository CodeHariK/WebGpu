#ifndef SOKOBAN_SOLVER_H
#define SOKOBAN_SOLVER_H

#include <string>
#include <vector>

namespace godot {

/**
 * SokobanSolver — breadth-first search over push states for PuzzleGrid levels.
 * ---------------------------------------------------------------------------
 * Plain C++ (no Godot types) so the generator can call it thousands of times.
 * A state is (player region, box positions + kinds, filled water cells); the player's
 * free walking is folded into the region (flood fill), so every BFS edge is exactly one
 * push and the first goal hit is the minimum number of pushes.
 *
 * Layout characters understood (same as PuzzleGrid):
 *   #  wall    E  door (solid)    A  alien (blocks player and boxes)
 *   K  key (player may stand on it, boxes may not)    B  crate    X  box bomb
 *   P  player start    .  floor    =  bridge (floor)    (space)  outside
 *   ~  water: not walkable; a box pushed in sinks and the cell becomes floor
 *   ^  lava:  not walkable; a box pushed in burns (is lost)
 *   I  ice:   walkable; a box pushed onto ice keeps sliding until blocked
 */
class SokobanSolver {
public:
	struct Result {
		bool solved = false;
		int pushes = -1; ///< minimum pushes (when solved)
		int states = 0; ///< states explored (cost / difficulty hint)
	};

	explicit SokobanSolver(const std::vector<std::string> &p_rows);

	/// Fewest pushes for the player to reach the key, then a floor cell beside the door.
	Result solve_key_and_exit(int p_max_states = 30000) const;
	/// Fewest pushes to get any box bomb next to (4-neighbour) any alien.
	Result solve_bomb_to_alien(int p_max_states = 8000) const;

private:
	struct State {
		std::vector<int> boxes; ///< cell * 2 + (is_bomb ? 1 : 0), sorted
		std::vector<int> filled; ///< water cells filled by sunk boxes, sorted
		int player = -1;
		int pushes = 0;
	};

	int w = 0;
	int h = 0;
	std::vector<char> base; ///< '#' solid, '.' floor, 'i' ice, 'K' key, 'A' alien, '~' water, '^' lava, ' ' outside
	State start;
	int key = -1;
	std::vector<int> exit_cells; ///< floor cells next to the door
	std::vector<int> alien_cells;

	enum Goal {
		GOAL_CELL, ///< player reaches one of `targets`
		GOAL_BOMB, ///< a bomb box is next to an alien
	};

	bool _walk(
			int p_cell,
			const State &p_s
	) const; ///< player may stand here (ignoring boxes)
	bool _rest(
			int p_cell,
			const State &p_s
	) const; ///< a box may rest here (ignoring other boxes)
	void _reach(
			const State &p_s,
			std::vector<char> &r_seen
	) const;
	bool _push(
			const State &p_s,
			size_t p_box,
			int p_dir,
			State &r_next
	) const; ///< apply one push (slide on ice, sink / burn in liquid)
	Result _search(
			const State &p_start,
			Goal p_goal,
			const std::vector<int> &p_targets,
			int p_max_states,
			State *r_end
	) const;
	bool _bomb_goal(const State &p_s) const;
};

} // namespace godot

#endif // SOKOBAN_SOLVER_H
