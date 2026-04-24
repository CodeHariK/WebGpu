#ifndef CELESTE_DASH_STATES_H
#define CELESTE_DASH_STATES_H

#include "../celeste_state.h"

namespace godot {

class CelesteDashState : public CelesteState {
public:
	using CelesteState::CelesteState;
	String get_name() const override { return "Dash"; }
	void enter() override;
	void physics_update(float delta) override;
};

} // namespace godot

#endif // CELESTE_DASH_STATES_H
