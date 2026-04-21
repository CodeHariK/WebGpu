#ifndef CELESTE_STATE_H
#define CELESTE_STATE_H

#include <godot_cpp/core/defs.hpp>
#include <godot_cpp/variant/string.hpp>

namespace godot {

class CelesteController;

/**
 * Base class for all Celeste-style character states in the HSM.
 */
class CelesteState {
protected:
	CelesteController *controller = nullptr;
	CelesteState *parent = nullptr;

public:
	CelesteState(CelesteController *p_ctrl, CelesteState *p_parent = nullptr) :
			controller(p_ctrl), parent(p_parent) {}
	virtual ~CelesteState() {}

	virtual String get_name() const = 0;

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

	virtual bool is_in_state(CelesteState *p_other) const {
		if (this == p_other) {
			return true;
		}
		return parent ? parent->is_in_state(p_other) : false;
	}
};

} // namespace godot

#endif // CELESTE_STATE_H
