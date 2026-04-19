#ifndef CHARACTER_GROUNDED_STATES_H
#define CHARACTER_GROUNDED_STATES_H

#include "../physics_character_state.h"

namespace godot {

class CharGroundedState : public CharacterState {
public:
	using CharacterState::CharacterState;
	void physics_update(float delta) override;
};

class CharIdleState : public CharGroundedState {
public:
	using CharGroundedState::CharGroundedState;
	void enter() override;
	void physics_update(float delta) override;
};

class CharMoveState : public CharGroundedState {
public:
	using CharGroundedState::CharGroundedState;
	void physics_update(float delta) override;
};

} // namespace godot

#endif // CHARACTER_GROUNDED_STATES_H
