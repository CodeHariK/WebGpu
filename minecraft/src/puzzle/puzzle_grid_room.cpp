// PuzzleGrid — room mode: entering / leaving puzzle play (jump lock, optional top-down camera).
#include "puzzle_grid.h"

#include "../camera/camera.h"
#include "../game_manager/game_manager.h"
#include "../game_manager/player_input.h"
#include "puzzle_cells.h"

#include <godot_cpp/core/math.hpp>
#include <godot_cpp/core/object.hpp>

namespace godot {

// Hysteresis so the mode never flickers on a boundary (e.g. standing on an outer wall):
// ENTER only once standing on a room floor cell; LEAVE only when clearly outside the
// grid rectangle (plus a margin) or far above / below it.
static const float LEAVE_MARGIN = 1.0f;

bool PuzzleGrid::_on_room_floor(const Vector3 &p_local) const {
	const char c = _cell((int)Math::floor(p_local.x / cell_size), (int)Math::floor(p_local.z / cell_size));
	return PuzzleCell::is_ground(c) && p_local.y > -1.0f && p_local.y < PuzzleCell::FLOOR_TOP + cell_size * 1.5f;
}

bool PuzzleGrid::_contains(const Vector3 &p_local) const {
	return p_local.x >= -LEAVE_MARGIN && p_local.z >= -LEAVE_MARGIN && p_local.x <= width * cell_size + LEAVE_MARGIN &&
			p_local.z <= depth * cell_size + LEAVE_MARGIN && p_local.y > -3.0f && p_local.y < wall_height + 5.0f;
}

// Entering the grid switches to puzzle play: top-down camera, camera-relative movement,
// jumping locked. Leaving (or switching target) restores everything.
void PuzzleGrid::_update_room_mode(Node3D *p_player) {
	const Vector3 lp = p_player ? to_local(p_player->get_global_position()) : Vector3();
	const bool inside = p_player && (player_inside ? _contains(lp) : _on_room_floor(lp));
	if (player_inside && (!inside || p_player->get_instance_id() != inside_id)) {
		_leave_room();
	}
	if (inside && !player_inside) {
		_enter_room(p_player);
	}
}

void PuzzleGrid::_enter_room(Node3D *p_player) {
	player_inside = true;
	inside_id = p_player->get_instance_id();
	if (PlayerInput *input = PlayerInput::get_singleton()) {
		input->set_jump_locked(true);
	}
	saved_scheme = -1;
	if (!use_topdown_camera) {
		return; // normal camera, controls untouched
	}
	GameManager *gm = GameManager::get_singleton();
	if (gm && gm->get_camera()) {
		gm->get_camera()->push_topdown(topdown_offset);
	}
	// Top-down needs camera-relative stick movement (steer mode turns with the stick).
	if (p_player->has_method("get_control_scheme")) {
		saved_scheme = (int)p_player->call("get_control_scheme");
		p_player->call("set_control_scheme", 1);
	}
}

void PuzzleGrid::_leave_room() {
	GameManager *gm = GameManager::get_singleton();
	if (gm && gm->get_camera()) {
		gm->get_camera()->pop_topdown();
	}
	if (PlayerInput *input = PlayerInput::get_singleton()) {
		input->set_jump_locked(false);
	}
	Object *target = ObjectDB::get_instance(inside_id);
	if (target && saved_scheme >= 0 && target->has_method("set_control_scheme")) {
		target->call("set_control_scheme", saved_scheme);
	}
	player_inside = false;
	inside_id = 0;
	saved_scheme = -1;
}

void PuzzleGrid::_exit_tree() {
	if (player_inside) {
		_leave_room();
	}
}

} // namespace godot
