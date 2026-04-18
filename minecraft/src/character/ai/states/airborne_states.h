#ifndef CHARACTER_AIRBORNE_STATES_H
#define CHARACTER_AIRBORNE_STATES_H

#include "../character_state.h"

namespace godot {

class CharAirborneState : public CharacterState {
public:
	using CharacterState::CharacterState;
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

} // namespace godot

#endif // CHARACTER_AIRBORNE_STATES_H
