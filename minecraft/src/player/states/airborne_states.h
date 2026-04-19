#ifndef CELESTE_AIRBORNE_STATES_H
#define CELESTE_AIRBORNE_STATES_H

#include "../celeste_state.h"

namespace godot {

class CelesteAirborneState : public CelesteState {
public:
	using CelesteState::CelesteState;
	void physics_update(float delta) override;
};

class CelesteJumpState : public CelesteAirborneState {
public:
	using CelesteAirborneState::CelesteAirborneState;
	void enter() override;
	void physics_update(float delta) override;
};

class CelesteFallState : public CelesteAirborneState {
public:
	using CelesteAirborneState::CelesteAirborneState;
	void enter() override;
	void physics_update(float delta) override;
};

} // namespace godot

#endif // CELESTE_AIRBORNE_STATES_H
