#ifndef CHARACTER_STATE_H
#define CHARACTER_STATE_H

#include <godot_cpp/core/defs.hpp>

namespace godot {

class PhysicsCharacter3D;

/**
 * Base class for all character states in the HSM.
 */
class CharacterState {
protected:
	PhysicsCharacter3D *character = nullptr;
	CharacterState *parent = nullptr;

public:
	CharacterState(PhysicsCharacter3D *p_char, CharacterState *p_parent = nullptr) :
			character(p_char), parent(p_parent) {}
	virtual ~CharacterState() {}

	virtual void enter() {}
	virtual void exit() {}

	virtual void update(float delta) {
		if (parent) {
			parent->update(delta);
		}
	}

	virtual void physics_update(float delta) {
		if (parent) {
			parent->physics_update(delta);
		}
	}

	virtual bool is_in_state(CharacterState *p_other) const {
		if (this == p_other) {
			return true;
		}
		return parent ? parent->is_in_state(p_other) : false;
	}
};

} // namespace godot

#endif // CHARACTER_STATE_H
