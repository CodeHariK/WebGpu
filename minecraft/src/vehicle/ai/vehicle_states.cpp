#include "vehicle_states.h"
#include "../arcade_vehicle.h"
#include <godot_cpp/classes/input.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

// --- GROUNDED STATE (Parent) ---

void GroundedState::physics_update(float delta) {
	if (!vehicle || vehicle->get_vehicle_config().is_null()) return;

	// In Grounded state, we always process basic physics if enough wheels are down
	// But first, the base class (VehicleState) would call parent, which is null here.
	
	// Transition check: If no wheels grounded, go to Airborne
	// We'll let the child (DrivingState) or ArcadeVehicle handle the actual transition 
	// based on the raycast results from the main loop.
}

// --- AIRBORNE STATE (Parent) ---

void AirborneState::physics_update(float delta) {
	if (!vehicle || vehicle->get_vehicle_config().is_null()) return;

	// Apply Downforce even in air (to simulate air resistance/gravity assist)
	float downforce_mag = vehicle->get_vehicle_config()->get_downforce();
	vehicle->apply_central_force(Vector3(0, -1, 0) * downforce_mag);

	// Stabilization logic could go here
}

// --- DRIVING STATE (Child of Grounded) ---

void DrivingState::physics_update(float delta) {
	if (!vehicle) return;

	// 1. Call parent logic (Grounded)
	GroundedState::physics_update(delta);

	// 2. Normal Driving Logic (Acceleration, Steering, Lateral Friction)
	// These were previously in _apply_acceleration, _apply_steering, etc.
	// Since ArcadeVehicle is a friend, we can call its private methods or rewrite them here.
	// For "lightweight" refactor, we'll call the existing methods on the vehicle for now.
	
	vehicle->_process_inputs();
	vehicle->_apply_acceleration(delta);
	vehicle->_apply_steering(delta);
	vehicle->_apply_lateral_friction(delta);
	vehicle->_apply_stability(delta);
}

// --- STUNT STATE (Child of Airborne) ---

void StuntState::enter() {
	// Shift COM to center for cleaner rotations
	if (vehicle && !vehicle->get_vehicle_config().is_null()) {
		// The interpolation is handled in ArcadeVehicle::_physics_process currently
	}
}

void StuntState::physics_update(float delta) {
	if (!vehicle || vehicle->get_vehicle_config().is_null()) return;

	// 1. Call parent logic (Airborne)
	AirborneState::physics_update(delta);

	// 2. Apply Stunt Torque (Pitch around X)
	Transform3D trans = vehicle->get_global_transform();
	Vector3 right_dir = trans.basis.get_column(0).normalized();
	
	vehicle->apply_torque(right_dir * vehicle->get_vehicle_config()->get_stunt_torque_strength());
}

} // namespace godot
