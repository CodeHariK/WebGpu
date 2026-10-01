#ifndef CHARACTER_STATE_H
#define CHARACTER_STATE_H

#include <godot_cpp/variant/string.hpp>

namespace godot {

class SpringCharacter;

/**
 * Base class for every SpringCharacter moveset state (a small hierarchical
 * state machine, mirroring ArcadeVehicle's ai/ pattern).
 *
 * A concrete state implements physics_update() to apply that state's forces;
 * the owner (SpringCharacter) decides transitions once per frame and then calls
 * physics_update() on the active state. `parent` lets a state share behaviour via
 * inheritance and answer "am I an airborne state?" through is_in_state().
 */
class CharacterState {
protected:
	SpringCharacter *character = nullptr;
	CharacterState *parent = nullptr;

public:
	String state_name;

	CharacterState(
			String p_name,
			SpringCharacter *p_character,
			CharacterState *p_parent = nullptr
	) :
			character(p_character),
			parent(p_parent),
			state_name(p_name) {}
	virtual ~CharacterState() {}

	// Called once when this state becomes active / inactive.
	virtual void enter() {}
	virtual void exit() {}

	// Apply this state's forces for one physics frame. Default delegates up to the
	// parent so a child with nothing to add inherits its parent's behaviour.
	virtual void physics_update(float delta) {
		if (parent) {
			parent->physics_update(delta);
		}
	}

	// True if this state or any ancestor is p_other (e.g. a CharacterAirborneState check).
	bool is_in_state(CharacterState *p_other) const {
		if (this == p_other) {
			return true;
		}
		return parent ? parent->is_in_state(p_other) : false;
	}
};

} // namespace godot

#endif // CHARACTER_STATE_H
