#ifndef CELESTE_GROUNDED_STATES_H
#define CELESTE_GROUNDED_STATES_H

#include "../celeste_state.h"

namespace godot {

class CelesteGroundedState : public CelesteState {
public:
	using CelesteState::CelesteState;
	void physics_update(float delta) override;
};

class CelesteIdleState : public CelesteGroundedState {
public:
	using CelesteGroundedState::CelesteGroundedState;
	void enter() override;
	void physics_update(float delta) override;
};

class CelesteMoveState : public CelesteGroundedState {
public:
	using CelesteGroundedState::CelesteGroundedState;
	void enter() override;
	void physics_update(float delta) override;
};

} // namespace godot

#endif // CELESTE_GROUNDED_STATES_H
