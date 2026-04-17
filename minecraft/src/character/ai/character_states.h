#ifndef CHARACTER_STATES_H
#define CHARACTER_STATES_H

#include "character_state.h"
#include <godot_cpp/classes/input.hpp>

namespace godot {

// --- PARENT STATES ---

class CharGroundedState : public CharacterState {
public:
	using CharacterState::CharacterState;
	void physics_update(float delta) override;
};

class CharAirborneState : public CharacterState {
public:
	using CharacterState::CharacterState;
	void physics_update(float delta) override;
};

// --- LEAF STATES ---

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

class CharJumpState : public CharAirborneState {
private:
	float jump_timer = 0.0f;
public:
	using CharAirborneState::CharAirborneState;
	void enter() override;
	void physics_update(float delta) override;
};

class CharFallState : public CharAirborneState {
public:
	using CharAirborneState::CharAirborneState;
	void physics_update(float delta) override;
};

class CharDropkickState : public CharacterState {
private:
	Vector3 target_point;
	bool has_target = false;
	float kick_timer = 0.0f;

public:
	using CharacterState::CharacterState;
	void enter() override;
	void physics_update(float delta) override;
};

} // namespace godot

#endif // CHARACTER_STATES_H
