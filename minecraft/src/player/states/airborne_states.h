#ifndef CELESTE_AIRBORNE_STATES_H
#define CELESTE_AIRBORNE_STATES_H

#include "../celeste_state.h"

namespace godot {

class CelesteAirborneState : public CelesteState {
public:
	using CelesteState::CelesteState;
	String get_name() const override { return "Airborne"; }
	void physics_update(float delta) override;
};

class CelesteJumpState : public CelesteAirborneState {
public:
	using CelesteAirborneState::CelesteAirborneState;
	String get_name() const override { return "Jump"; }
	void enter() override;
	void physics_update(float delta) override;
};

class CelesteFallState : public CelesteAirborneState {
public:
	using CelesteAirborneState::CelesteAirborneState;
	String get_name() const override { return "Fall"; }
	void enter() override;
	void physics_update(float delta) override;
};

class CelesteDoubleJumpState : public CelesteAirborneState {
public:
	using CelesteAirborneState::CelesteAirborneState;
	String get_name() const override { return "DoubleJump"; }
};

class CelesteFloatState : public CelesteAirborneState {
public:
	using CelesteAirborneState::CelesteAirborneState;
	String get_name() const override { return "Float"; }
};

} // namespace godot

#endif // CELESTE_AIRBORNE_STATES_H
