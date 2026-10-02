#include "character_animator.h"

#include <godot_cpp/core/math.hpp>

namespace godot {

// Hysteresis-free thresholds; the skin's own crossfades absorb brief flicker.
static const float RISING_VY = 0.5f; // above this while airborne = still going up (Jump)
static const float MOVING_SPEED = 0.3f; // above this while grounded = travelling (Move)

bool CharacterAnimator::set_skin(Node *p_skin) {
	skin = nullptr;
	current = LOCO_NONE;
	has_tilt = false;
	if (!p_skin) {
		return false;
	}
	bool ok = p_skin->has_method("idle") && p_skin->has_method("move") && p_skin->has_method("jump") &&
			p_skin->has_method("fall");
	if (!ok) {
		return false;
	}
	skin = p_skin;
	// Optional lean: only drive it if the skin exposes the property.
	has_tilt = skin->get_property_list().size() > 0 && skin->get("run_tilt").get_type() != Variant::NIL;
	return true;
}

void CharacterAnimator::_travel(Locomotion p_state) {
	if (!skin || p_state == current) {
		return; // only tell the skin on a CHANGE; it handles the blend itself
	}
	current = p_state;
	switch (p_state) {
		case LOCO_IDLE:
			skin->call("idle");
			break;
		case LOCO_MOVE:
			skin->call("move");
			break;
		case LOCO_JUMP:
			skin->call("jump");
			break;
		case LOCO_FALL:
			skin->call("fall");
			break;
		default:
			break;
	}
}

void CharacterAnimator::update(
		bool p_grounded,
		float p_vy,
		float p_h_speed,
		float p_lean
) {
	if (!skin) {
		return;
	}
	Locomotion next;
	if (!p_grounded) {
		next = (p_vy > RISING_VY) ? LOCO_JUMP : LOCO_FALL;
	} else {
		next = (p_h_speed > MOVING_SPEED) ? LOCO_MOVE : LOCO_IDLE;
	}
	_travel(next);

	if (has_tilt) {
		skin->set("run_tilt", CLAMP(p_lean, -1.0f, 1.0f));
	}
}

} // namespace godot
