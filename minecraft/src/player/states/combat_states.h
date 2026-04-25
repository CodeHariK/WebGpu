#ifndef CELESTE_COMBAT_STATES_H
#define CELESTE_COMBAT_STATES_H

#include "../celeste_state.h"
#include <godot_cpp/classes/node3d.hpp>

namespace godot {

class CelesteJumpKickState : public CelesteState {
	Node3D *target = nullptr;
	Vector3 target_pos;
	float duration = 0.0f;
	const float MAX_DURATION = 0.5f;

public:
	using CelesteState::CelesteState;
	String get_name() const override { return "JumpKick"; }

	void enter() override;
	void physics_update(float delta) override;
};

} // namespace godot

#endif // CELESTE_COMBAT_STATES_H
