#ifndef PUSH_BOX_H
#define PUSH_BOX_H

#include <godot_cpp/classes/animatable_body3d.hpp>

namespace godot {

/**
 * PushBox — a Sokoban box on a PuzzleGrid cell (plain crate or box bomb).
 * --------------------------------------------------------------------------
 * A kinematic body (AnimatableBody3D, synced to physics) so characters collide with it and
 * stand on it. It never moves by physics: the grid decides a push and calls slide_to(),
 * which eases it exactly one cell over `slide_time`. Cell coordinates live here; the grid
 * owns the occupancy rules.
 */
class PushBox : public AnimatableBody3D {
	GDCLASS(PushBox,
			AnimatableBody3D)

private:
	bool bomb = false;
	bool sliding = false;
	float size = 1.0f;
	float slide_t = 0.0f;
	float slide_time = 0.18f;
	Vector3 from;
	Vector3 to;

protected:
	static void _bind_methods() {}

public:
	int cell_x = 0;
	int cell_z = 0;
	int pending = 0; ///< PuzzleGrid::Landing to resolve once the slide ends (sink / burn)

	/// Build visual + collision for a cell of p_cell metres.
	void setup(
			float p_cell,
			bool p_bomb
	);
	void slide_to(
			const Vector3 &p_local_target,
			float p_time
	);
	bool is_sliding() const { return sliding; }
	bool is_bomb() const { return bomb; }
	/// Edge length of the crate (m).
	float get_box_size() const { return size; }
	/// Darkened "wet wood" look for a crate sunk into water as a raft.
	void set_wet(bool p_wet);

	void _physics_process(double p_delta) override;
};

} // namespace godot

#endif // PUSH_BOX_H
