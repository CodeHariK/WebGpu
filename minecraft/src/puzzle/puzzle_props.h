#ifndef PUZZLE_PROPS_H
#define PUZZLE_PROPS_H

#include <godot_cpp/classes/node3d.hpp>

namespace godot {

/**
 * PuzzleProps — cute toy-primitive visuals for puzzle rooms (no behaviour, no collision).
 * Each builder adds a Node3D under p_parent whose origin sits on the floor at the cell
 * centre, sized for a cell of p_cell metres. Low poly on purpose (mobile).
 */
namespace PuzzleProps {

/// Wooden crate: planks + darker edge frame. Fills ~0.9 of a cell.
Node3D *crate(
		Node3D *p_parent,
		float p_cell
);
/// Box bomb: red-striped crate with a round bomb, fuse and spark on top.
Node3D *bomb_crate(
		Node3D *p_parent,
		float p_cell
);
/// Golden key floating above the floor (animate with spin / bob).
Node3D *key(
		Node3D *p_parent,
		float p_cell
);
/// Green blob alien with eye stalks (animate with a bob).
Node3D *alien(
		Node3D *p_parent,
		float p_cell
);
/// Exit door: stone frame + wooden door, facing +Z.
Node3D *exit_door(
		Node3D *p_parent,
		float p_cell,
		float p_height
);

} // namespace PuzzleProps

} // namespace godot

#endif // PUZZLE_PROPS_H
