#ifndef VEHICLE_STATES_H
#define VEHICLE_STATES_H

#include "vehicle_state.h"
#include <godot_cpp/classes/input.hpp>

namespace godot {

// --- PARENT STATES ---

/**
 * Handles all logic shared by grounded states (suspension, ground friction).
 */
class GroundedState : public VehicleState {
public:
	using VehicleState::VehicleState;
	virtual void physics_update(float delta) override;
};

/**
 * Handles all logic shared by airborne states (gravity assist, air rotation).
 */
class AirborneState : public VehicleState {
public:
	using VehicleState::VehicleState;
	virtual void physics_update(float delta) override;
};

// --- CHILD STATES ---

/**
 * The default driving state when on the ground.
 */
class DrivingState : public GroundedState {
public:
	using GroundedState::GroundedState;
	virtual void physics_update(float delta) override;
};

/**
 * Specialized mid-air state for gliding smoothly.
 */
class GlidingState : public AirborneState {
public:
	using AirborneState::AirborneState;
	virtual void enter() override;
	virtual void exit() override;
	virtual void physics_update(float delta) override;
};

/**
 * Mid-air spin state (pitch/backflip) on ramp takeoff.
 */
class RampSpinState : public AirborneState {
public:
	using AirborneState::AirborneState;
	virtual void enter() override;
	virtual void physics_update(float delta) override;
};

/**
 * Mid-air roll state (barrel roll) on ramp takeoff.
 */
class RampRollState : public AirborneState {
private:
	float roll_direction = 1.0f;
public:
	using AirborneState::AirborneState;
	virtual void enter() override;
	virtual void physics_update(float delta) override;
	void set_roll_direction(float p_dir) { roll_direction = p_dir; }
	float get_roll_direction() const { return roll_direction; }
};

} // namespace godot

#endif // VEHICLE_STATES_H
