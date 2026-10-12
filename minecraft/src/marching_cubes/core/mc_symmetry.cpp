// Turning a cell's hash and transform about Y.
#include "marching_cubes/core/mc_symmetry.h"
#include "marching_cubes/core/mc_corners.h"
#include <godot_cpp/core/math.hpp>

namespace godot::mc {

// Hash after one +90 degree turn about Y: every solid corner moves to its turned position.
uint8_t rotate_y(uint8_t p_hash) {
	// Where each corner goes after a +90 degree turn: 0->1->2->3->0 and 4->5->6->7->4.
	static const uint8_t swap_table[8] = { C1, C2, C3, C0, C5, C6, C7, C4 };
	uint8_t next = 0;
	for (int i = 7; i >= 0; i--) {
		if ((p_hash >> i) & 1) {
			next |= (1 << swap_table[i]);
		}
	}
	return next;
}

// Turn hash and transform together by p_quarter_turns x 90 degrees about Y.
void turn_y(
		uint8_t &p_hash,
		Transform3D &p_transform,
		int p_quarter_turns
) {
	// 270 degrees is written as -90 so the basis matches what the tiles were built with before.
	static const float angles[4] = { 0.0f, Math::PI / 2.0f, Math::PI, -Math::PI / 2.0f };
	int turns = ((p_quarter_turns % 4) + 4) % 4;
	if (turns == 0) {
		return;
	}
	for (int i = 0; i < turns; i++) {
		p_hash = rotate_y(p_hash);
	}
	p_transform = p_transform.rotated(Vector3(0, 1, 0), angles[turns]);
}

} // namespace godot::mc
