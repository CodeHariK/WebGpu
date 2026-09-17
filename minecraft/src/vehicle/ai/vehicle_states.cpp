#include "vehicle_states.h"
#include "../arcade_vehicle.h"
#include "vehicle/config/vehicle_config.h"
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

// ---------------------------------------------------------------------------
// State-machine tuning constants and debug logging switch.
// ---------------------------------------------------------------------------
namespace {
constexpr bool VEHICLE_STATE_LOG = false; // set true to trace HSM enter/exit in the console
constexpr float DRIFT_HOP_IMPULSE = 1.5f; // small upward hop when a drift starts
constexpr float GLIDE_LIFT_ACCEL = 25.0f; // upward accel applied while gliding (* mass)
constexpr float GLIDE_FORWARD_EFFICIENCY = 8.0f; // converts downward speed into forward thrust while gliding
constexpr float GLIDE_LATERAL_DRAG = 4.0f; // kills sideways slide for responsive glide steering
constexpr float GLIDE_YAW_TORQUE = 3.0f; // yaw authority while gliding (* mass)
constexpr float GLIDE_MAX_SPEED_FACTOR = 0.85f; // glide forward speed cap as a fraction of max_speed
constexpr float FLIP_SPIN_TIME = 0.8f; // seconds targeted for a full 360 ramp flip/roll
constexpr float FLIP_TORQUE_FACTOR = 6.0f; // torque gain driving toward the target flip/roll speed
constexpr float MIN_SPEED_EPSILON = 0.001f; // divide-by-zero guard
} // namespace

// --- GROUNDED STATE (Parent) ---

void GroundedState::physics_update(float delta) {
	if (!vehicle || vehicle->get_vehicle_config().is_null())
		return;

	// Shared grounded driving physics, used by both DrivingState and DriftingState.
	// The is_drifting flag (set by DriftingState on enter/exit) changes grip and
	// steering feel inside these calls, so the two child states differ only in their
	// extra behaviour (nitro accrual) and transition conditions, not in the core forces.
	vehicle->_apply_acceleration(delta);
	vehicle->_apply_steering(delta);
	vehicle->_apply_lateral_friction(delta);
	vehicle->_apply_stability(delta);
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

	// 1. Shared grounded driving physics (parent).
	GroundedState::physics_update(delta);

	// 2. Transition check: enter drift if conditions are met
	Transform3D trans = vehicle->get_global_transform();
	Vector3 forward_dir = -trans.basis.get_column(2).normalized();
	float forward_speed = vehicle->get_linear_velocity().dot(forward_dir);
	float steering_intensity = Math::abs(vehicle->get_input().steering);

	if (vehicle->get_vehicle_config().is_valid() && vehicle->get_input().handbrake &&
		forward_speed > vehicle->get_vehicle_config()->get_drift_speed_threshold() &&
		steering_intensity > vehicle->get_vehicle_config()->get_drift_steering_threshold()) {
		vehicle->change_state(vehicle->drifting_state);
	}
}

// --- DRIFTING STATE (Child of Grounded) ---

void DriftingState::enter() {
	if (VEHICLE_STATE_LOG)
		UtilityFunctions::print("ArcadeVehicle: Entered DriftingState");
	if (vehicle) {
		vehicle->is_drifting = true;
		Transform3D trans = vehicle->get_global_transform();
		Vector3 up_dir = trans.basis.get_column(1).normalized();
		vehicle->velocity_nudge_accumulator += up_dir * DRIFT_HOP_IMPULSE; // Hop impulse!
	}
}

void DriftingState::exit() {
	if (VEHICLE_STATE_LOG)
		UtilityFunctions::print("ArcadeVehicle: Exited DriftingState");
	if (vehicle) {
		vehicle->is_drifting = false;
	}
}

void DriftingState::physics_update(float delta) {
	if (!vehicle)
		return;
	Ref<VehicleConfig> config = vehicle->get_vehicle_config();
	if (config.is_null())
		return;

	// 1. Shared grounded driving physics (parent) — is_drifting flag is active here.
	GroundedState::physics_update(delta);

	// 2. Accumulate nitro fuel over time while drifting
	vehicle->nitro_fuel =
			MIN(config->get_nitro_max_fuel(), vehicle->nitro_fuel + config->get_nitro_refuel_rate() * delta);

	// 3. Transition check: exit drift if handbrake released or speed falls below threshold
	Transform3D trans = vehicle->get_global_transform();
	Vector3 forward_dir = -trans.basis.get_column(2).normalized();
	float forward_speed = vehicle->get_linear_velocity().dot(forward_dir);

	if (!vehicle->get_input().handbrake || forward_speed < config->get_drift_speed_threshold()) {
		vehicle->change_state(vehicle->driving_state);
	}
}

void GlidingState::enter() {
	if (VEHICLE_STATE_LOG)
		UtilityFunctions::print("ArcadeVehicle: Entered GlidingState");
	if (vehicle) {
		vehicle->set_angular_velocity(Vector3(0.0f, 0.0f, 0.0f));
	}
}

void GlidingState::exit() {
	if (VEHICLE_STATE_LOG)
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

	float forward_speed = vel.dot(forward);
	float max_speed = vehicle->get_vehicle_config()->get_max_speed();
	float max_glide_speed = MAX(max_speed * GLIDE_MAX_SPEED_FACTOR, MIN_SPEED_EPSILON); // Glide top speed < max_speed

	// Constant upward accel (GLIDE_LIFT_ACCEL) applied while gliding, scaled by mass.
	Vector3 lift_force = Vector3(0.0f, GLIDE_LIFT_ACCEL, 0.0f) * mass;
	vehicle->apply_central_force(lift_force);

	// Translate downward falling speed into forward speed (Gliding lift)
	if (vel.y < 0.0f) {
		float glide_efficiency = GLIDE_FORWARD_EFFICIENCY; // converts downward velocity into forward thrust
		Vector3 forward_thrust = forward * (-vel.y * glide_efficiency) * mass;

		if (forward_speed > 0.0f) {
			float speed_ratio = forward_speed / max_glide_speed;
			if (speed_ratio > 1.0f) {
				forward_thrust = Vector3(0.0f, 0.0f, 0.0f);
			} else {
				float scale = 1.0f - speed_ratio;
				forward_thrust *= scale;
			}
		}
		vehicle->apply_central_force(forward_thrust);
	}

	// Apply drag only to the lateral velocity component to reduce sideways slip/momentum when steering, preserving
	// forward and falling momentum
	float lateral_speed = vel.dot(right_dir);
	float lateral_drag_coeff = GLIDE_LATERAL_DRAG; // Rapidly kills sideways slide for responsive steering control
	Vector3 drag = -right_dir * lateral_speed * lateral_drag_coeff * mass;
	vehicle->apply_central_force(drag);

	// 3. Aerodynamic Controls (Pitch / Roll / Yaw)
	// Yaw (steer slowly yaw-pivots left/right)
	float steer_input = vehicle->get_input().steering;
	vehicle->apply_torque(-local_up * steer_input * mass * GLIDE_YAW_TORQUE);
}

// --- RAMP SPIN STATE ---

void RampSpinState::enter() {
	if (VEHICLE_STATE_LOG)
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

	float target_pitch_speed = (2.0f * Math::PI / FLIP_SPIN_TIME); // target angular speed for a full flip
	float pitch_speed_error = target_pitch_speed - pitch_speed;
	float torque_mag = pitch_speed_error * vehicle->get_vehicle_config()->get_mass() * FLIP_TORQUE_FACTOR;
	vehicle->apply_torque(right_dir * torque_mag);
}

// --- RAMP ROLL STATE ---

void RampRollState::enter() {
	if (VEHICLE_STATE_LOG)
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

	float target_roll_speed = roll_direction * (2.0f * Math::PI / FLIP_SPIN_TIME);
	float roll_speed_error = target_roll_speed - roll_speed;
	float torque_mag = roll_speed_error * vehicle->get_vehicle_config()->get_mass() * FLIP_TORQUE_FACTOR;
	vehicle->apply_torque(roll_forward_dir * torque_mag);
}

} // namespace godot
