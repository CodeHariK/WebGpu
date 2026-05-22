#include "vehicle_states.h"
#include "../arcade_vehicle.h"
#include <godot_cpp/classes/input.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

// --- GROUNDED STATE (Parent) ---

void GroundedState::physics_update(float delta) {
	if (!vehicle || vehicle->get_vehicle_config().is_null())
		return;

	// In Grounded state, we always process basic physics if enough wheels are down
	// But first, the base class (VehicleState) would call parent, which is null here.

	// Transition check: If no wheels grounded, go to Airborne
	// We'll let the child (DrivingState) or ArcadeVehicle handle the actual transition
	// based on the raycast results from the main loop.
}

// --- AIRBORNE STATE (Parent) ---

void AirborneState::physics_update(float delta) {
	if (!vehicle || vehicle->get_vehicle_config().is_null())
		return;

	// Apply Downforce even in air (to simulate air resistance/gravity assist)
	float downforce_mag = vehicle->get_vehicle_config()->get_downforce();
	vehicle->apply_central_force(Vector3(0, -1, 0) * downforce_mag);
}

// --- DRIVING STATE (Child of Grounded) ---

void DrivingState::physics_update(float delta) {
	if (!vehicle)
		return;

	// 1. Call parent logic (Grounded)
	GroundedState::physics_update(delta);

	// 2. Normal Driving Logic (Acceleration, Steering, Lateral Friction)
	// These were previously in _apply_acceleration, _apply_steering, etc.
	// Since ArcadeVehicle is a friend, we can call its private methods or rewrite them here.
	// For "lightweight" refactor, we'll call the existing methods on the vehicle for now.

	vehicle->_apply_acceleration(delta);
	vehicle->_apply_steering(delta);
	vehicle->_apply_lateral_friction(delta);
	vehicle->_apply_stability(delta);
}

void GlidingState::enter() {
	UtilityFunctions::print("ArcadeVehicle: Entered GlidingState");
	if (vehicle) {
		vehicle->set_angular_velocity(Vector3(0.0f, 0.0f, 0.0f));
	}
}

void GlidingState::exit() {
	UtilityFunctions::print("ArcadeVehicle: Exited GlidingState");
}

void GlidingState::physics_update(float delta) {
	if (!vehicle || vehicle->get_vehicle_config().is_null())
		return;

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
	Vector3 lift_force = Vector3(0.0f, 25, 0.0f) * mass;
	vehicle->apply_central_force(lift_force);

	// Translate downward falling speed into forward speed (Gliding lift)
	if (vel.y < 0.0f) {
		float glide_efficiency = 8.0f; // converts 80% of downward velocity to forward thrust
		Vector3 forward_thrust = forward * (-vel.y * glide_efficiency) * mass;
		vehicle->apply_central_force(forward_thrust);
	}

	// Apply extra linear drag to keep the glide smooth
	Vector3 drag = -vel * 0.5f * mass;
	vehicle->apply_central_force(drag);

	// 3. Aerodynamic Controls (Pitch / Roll / Yaw)
	// Yaw (steer slowly yaw-pivots left/right)
	float steer_input = vehicle->get_input().steering;
	vehicle->apply_torque(-local_up * steer_input * mass * 2.0f);
}

// --- RAMP SPIN STATE ---

void RampSpinState::enter() {
	UtilityFunctions::print("ArcadeVehicle: Entered RampSpinState");
}

void RampSpinState::physics_update(float delta) {
	if (!vehicle || vehicle->get_vehicle_config().is_null())
		return;

	// 1. Call parent (AirborneState) to apply downforce
	AirborneState::physics_update(delta);

	// 2. Apply spin torque driving towards target pitch speed
	Transform3D trans = vehicle->get_global_transform();
	Vector3 right_dir = trans.basis.get_column(0).normalized();
	float pitch_speed = vehicle->get_angular_velocity().dot(right_dir);

	float target_pitch_speed = (2.0f * Math_PI / 0.8f); // target speed for 360 flip
	float pitch_speed_error = target_pitch_speed - pitch_speed;
	float torque_mag = pitch_speed_error * vehicle->get_vehicle_config()->get_mass() * 6.0f;
	vehicle->apply_torque(right_dir * torque_mag);
}

// --- RAMP ROLL STATE ---

void RampRollState::enter() {
	UtilityFunctions::print("ArcadeVehicle: Entered RampRollState");
}

void RampRollState::physics_update(float delta) {
	if (!vehicle || vehicle->get_vehicle_config().is_null())
		return;

	// 1. Call parent (AirborneState) to apply downforce
	AirborneState::physics_update(delta);

	// 2. Apply roll torque driving towards target roll speed
	Transform3D trans = vehicle->get_global_transform();
	Vector3 roll_forward_dir = -trans.basis.get_column(2).normalized(); // Local -Z is forward
	float roll_speed = vehicle->get_angular_velocity().dot(roll_forward_dir);

	float target_roll_speed = roll_direction * (2.0f * Math_PI / 0.8f);
	float roll_speed_error = target_roll_speed - roll_speed;
	float torque_mag = roll_speed_error * vehicle->get_vehicle_config()->get_mass() * 6.0f;
	vehicle->apply_torque(roll_forward_dir * torque_mag);
}

} // namespace godot
