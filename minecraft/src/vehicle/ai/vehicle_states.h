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
 * Specialized mid-air state for performing stunts/flips.
 */
class StuntState : public AirborneState {
public:
	using AirborneState::AirborneState;
	virtual void enter() override;
	virtual void physics_update(float delta) override;
};

} // namespace godot

#endif // VEHICLE_STATES_H
