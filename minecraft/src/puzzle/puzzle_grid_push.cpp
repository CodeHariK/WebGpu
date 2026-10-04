// PuzzleGrid — pushing: intent detection, one-cell pushes, ice slides, sinking / burning.
#include "puzzle_grid.h"

#include "../game_manager/game_manager.h"
#include "../game_manager/player_input.h"
#include "push_box.h"
#include "puzzle_cells.h"

#include <godot_cpp/core/math.hpp>

#include <algorithm>

namespace godot {

using namespace PuzzleCell;

static const float BURN_TIME = 0.6f; // a box in lava sinks out of sight this fast, then is freed
static const float SINK_TIME = 0.45f;

PushBox *PuzzleGrid::_box_at(int p_x, int p_z) const {
	for (PushBox *b : boxes) {
		if (b->cell_x == p_x && b->cell_z == p_z) {
			return b;
		}
	}
	return nullptr;
}

// Push intent in this node's local space: while the stick is held, the way the player is
// FACING (both characters turn to face where they move, in steer and camera-relative
// schemes alike, so this works with any camera).
Vector3 PuzzleGrid::_wish_local(Node3D *p_player) const {
	PlayerInput *input = PlayerInput::get_singleton();
	if (!input || input->get_state().character.move_axis.length() < 0.3f) {
		return Vector3();
	}
	Vector3 face = -p_player->get_global_basis().get_column(2);
	face.y = 0.0f;
	if (face.length_squared() < 1e-4f) {
		return Vector3();
	}
	Vector3 local = get_global_basis().inverse().xform(face.normalized());
	local.y = 0.0f;
	return local.normalized();
}

// Where a box at (x, z) ends up when pushed by (dx, dz): one cell, more while it slides on
// ice; a liquid swallows it. Same rules as SokobanSolver::_push.
PuzzleGrid::Landing PuzzleGrid::_resolve_push(int p_x, int p_z, int p_dx, int p_dz, int &r_x, int &r_z) const {
	int x = p_x;
	int z = p_z;
	Landing land = LAND_NONE;
	while (true) {
		const int nx = x + p_dx;
		const int nz = z + p_dz;
		if (_box_at(nx, nz)) {
			break;
		}
		const char k = _cell(nx, nz);
		if (is_liquid(k)) {
			x = nx;
			z = nz;
			land = (k == WATER) ? LAND_SINK : LAND_BURN;
			break;
		}
		if (!box_can_rest(k)) {
			break;
		}
		x = nx;
		z = nz;
		land = LAND_REST;
		if (k != ICE) {
			break;
		}
	}
	r_x = x;
	r_z = z;
	return land;
}

void PuzzleGrid::_update_push(float p_dt) {
	GameManager *gm = GameManager::get_singleton();
	Node3D *player = gm ? Object::cast_to<Node3D>(gm->get_active_target()) : nullptr;
	_update_room_mode(player);
	if (!player || !player_inside) {
		return;
	}
	const Vector3 lp = to_local(player->get_global_position());
	const Vector3 wish = _wish_local(player);
	PushBox *candidate = nullptr;
	int dx = 0;
	int dz = 0;
	// Snap the stick to a grid axis; only clearly axis-aligned pushes count.
	if (Math::abs(wish.x) > 0.7f) {
		dx = wish.x > 0.0f ? 1 : -1;
	} else if (Math::abs(wish.z) > 0.7f) {
		dz = wish.z > 0.0f ? 1 : -1;
	}
	// On the island floor, not standing on top of a box.
	if ((dx || dz) && lp.y > -0.5f && lp.y < FLOOR_TOP + cell_size * 1.5f) {
		const int px = (int)Math::floor(lp.x / cell_size);
		const int pz = (int)Math::floor(lp.z / cell_size);
		PushBox *b = _box_at(px + dx, pz + dz);
		if (b && !b->is_sliding()) {
			const Vector3 bc = _cell_center(b->cell_x, b->cell_z);
			const float along = dx ? (bc.x - lp.x) * dx : (bc.z - lp.z) * dz;
			const float lateral = dx ? Math::abs(bc.z - lp.z) : Math::abs(bc.x - lp.x);
			if (along < cell_size * 0.5f + 0.9f && lateral < cell_size * 0.45f) {
				candidate = b;
			}
		}
	}
	if (!candidate || candidate != push_target) {
		push_target = candidate;
		push_timer = 0.0f;
		return;
	}
	push_timer += p_dt;
	if (push_timer < push_delay) {
		return;
	}
	push_timer = 0.0f;
	int tx = 0;
	int tz = 0;
	const Landing land = _resolve_push(candidate->cell_x, candidate->cell_z, dx, dz, tx, tz);
	if (land == LAND_NONE) {
		return;
	}
	const int cells_moved = Math::abs(tx - candidate->cell_x) + Math::abs(tz - candidate->cell_z);
	candidate->cell_x = tx;
	candidate->cell_z = tz;
	candidate->slide_to(_cell_center(tx, tz) + Vector3(0, FLOOR_TOP, 0), slide_time * cells_moved);
	if (land != LAND_REST) {
		// It leaves the push set now (nothing else may push or land on it) and resolves
		// once it has arrived over the liquid.
		candidate->pending = land;
		boxes.erase(std::find(boxes.begin(), boxes.end(), candidate));
		settling.push_back(candidate);
		push_target = nullptr;
	}
}

// Boxes that reached a liquid: water — sink until the top is flush with the floor, and
// the cell becomes walkable; lava — sink out of sight and disappear.
void PuzzleGrid::_settle_boxes() {
	for (size_t i = 0; i < settling.size();) {
		PushBox *b = settling[i];
		if (b->is_sliding()) {
			++i;
			continue;
		}
		const float size = b->get_box_size();
		Vector3 p = b->get_position();
		if (b->pending == LAND_SINK) {
			cells[(size_t)b->cell_z * width + b->cell_x] = FILLED;
			b->slide_to(Vector3(p.x, FLOOR_TOP - size, p.z), SINK_TIME);
			b->pending = LAND_NONE;
			_rebuild_dynamic_collision();
			b->set_wet(true); // the sunk crate is now a raft: reads as wet wood
		} else if (b->pending == LAND_BURN) {
			b->slide_to(Vector3(p.x, FLOOR_TOP - size * 1.6f, p.z), BURN_TIME);
			b->pending = LAND_NONE;
			burning.push_back(b);
		}
		settling.erase(settling.begin() + i);
	}
	for (size_t i = 0; i < burning.size();) {
		if (burning[i]->is_sliding()) {
			++i;
			continue;
		}
		burning[i]->queue_free();
		burning.erase(burning.begin() + i);
	}
}

} // namespace godot
