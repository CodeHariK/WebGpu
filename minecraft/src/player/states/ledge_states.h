#ifndef CELESTE_LEDGE_STATES_H
#define CELESTE_LEDGE_STATES_H

#include "../celeste_state.h"

namespace godot {

class CelesteLedgeClimbState : public CelesteState {
public:
	using CelesteState::CelesteState;
	String get_name() const override { return "LedgeClimb"; }
};

class CelesteLedgeJumpState : public CelesteState {
public:
	using CelesteState::CelesteState;
	String get_name() const override { return "LedgeJump"; }
};

} // namespace godot

#endif // CELESTE_LEDGE_STATES_H
