#ifndef CHARACTER_STATES_H
#define CHARACTER_STATES_H

#include "character_state.h"

namespace godot {

// --- Parent states (shared behaviour) ---

/**
 * Shared grounded behaviour: ride/float servo, upright + facing, planar move.
 */
class CharacterGroundedState : public CharacterState {
public:
	using CharacterState::CharacterState;
	virtual void physics_update(float delta) override;
};

/**
 * Shared airborne behaviour: upright + facing, air control, shaped gravity.
 */
class CharacterAirborneState : public CharacterState {
public:
	using CharacterState::CharacterState;
	virtual void physics_update(float delta) override;
};

// --- Concrete states ---

/** Default on-ground state. Pure grounded behaviour. */
class CharacterWalkState : public CharacterGroundedState {
public:
	using CharacterGroundedState::CharacterGroundedState;
};

/** Default in-air state (rising or falling). Pure airborne behaviour. */
class CharacterFallState : public CharacterAirborneState {
public:
	using CharacterAirborneState::CharacterAirborneState;
};

/** A short flat burst along the stick / facing direction. */
class CharacterDashState : public CharacterState {
public:
	using CharacterState::CharacterState;
	virtual void enter() override;
	virtual void physics_update(float delta) override;
};

/** Hover briefly, then slam straight down. */
class CharacterGroundPoundState : public CharacterAirborneState {
public:
	using CharacterAirborneState::CharacterAirborneState;
	virtual void enter() override;
	virtual void physics_update(float delta) override;
};

} // namespace godot

#endif // CHARACTER_STATES_H
