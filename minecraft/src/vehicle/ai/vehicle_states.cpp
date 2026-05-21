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

void GlidingState::enter() {
	UtilityFunctions::print("ArcadeVehicle: Entered GlidingState");
}

void GlidingState::exit() {
	UtilityFunctions::print("ArcadeVehicle: Exited GlidingState");
}

void GlidingState::physics_update(float delta) {
	if (!vehicle || vehicle->get_vehicle_config().is_null()) return;

	// 1. Call parent logic (Airborne)
	AirborneState::physics_update(delta);

	// 2. Gliding Physics
	float mass = vehicle->get_vehicle_config()->get_mass();
	Vector3 vel = vehicle->get_linear_velocity();
	Transform3D trans = vehicle->get_global_transform();
	Vector3 forward = -trans.basis.get_column(2).normalized();
	Vector3 local_up = trans.basis.get_column(1).normalized();
	Vector3 right_dir = trans.basis.get_column(0).normalized();

	// Counteract a portion of gravity (e.g., apply a constant upward force equal to 70% of gravity)
	Vector3 lift_force = Vector3(0.0f, 9.8f * 0.7f, 0.0f) * mass;
	vehicle->apply_central_force(lift_force);

	// Translate downward falling speed into forward speed (Gliding lift)
	if (vel.y < 0.0f) {
		float glide_efficiency = 0.8f; // converts 80% of downward velocity to forward thrust
		Vector3 forward_thrust = forward * (-vel.y * glide_efficiency) * mass;
		vehicle->apply_central_force(forward_thrust);
	}

	// Apply extra linear drag to keep the glide smooth
	Vector3 drag = -vel * 0.5f * mass;
	vehicle->apply_central_force(drag);

	// 3. Aerodynamic Controls (Pitch / Roll / Yaw)
	// Roll (steer rolls left/right)
	float steer_input = vehicle->get_input().steer;
	float roll_strength = mass * 15.0f;
	vehicle->apply_torque(-forward * steer_input * roll_strength);

	// Yaw (steer slowly yaw-pivots left/right)
	float yaw_strength = mass * 8.0f;
	vehicle->apply_torque(local_up * steer_input * yaw_strength);

	// Pitch (throttle pitches down, brake pitches up)
	float throttle_input = vehicle->get_input().throttle;
	float brake_input = vehicle->get_input().brake;
	float pitch_input = throttle_input - brake_input;
	float pitch_strength = mass * 12.0f;
	vehicle->apply_torque(right_dir * pitch_input * pitch_strength);
}

} // namespace godot
