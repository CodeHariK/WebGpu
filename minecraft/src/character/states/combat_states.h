#ifndef CHARACTER_COMBAT_STATES_H
#define CHARACTER_COMBAT_STATES_H

#include "../physics_character_state.h"
#include <godot_cpp/variant/vector3.hpp>

namespace godot {

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

class CharGrabState : public CharacterState {
public:
	using CharacterState::CharacterState;
	void enter() override;
	void exit() override;
	void physics_update(float delta) override;
};

} // namespace godot

#endif // CHARACTER_COMBAT_STATES_H
