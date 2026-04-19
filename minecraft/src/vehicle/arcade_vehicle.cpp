#include "arcade_vehicle.h"
#include "../game_manager/game_manager.h"
#include "../game_manager/player_input.h"
#include "../utils/raycast/mc_raycast.h"
#include "ai/vehicle_states.h"
#include "godot_cpp/classes/csg_box3d.hpp"
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/global_constants.hpp>
#include <godot_cpp/classes/input.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

void ArcadeVehicle::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_config", "config"), &ArcadeVehicle::set_config);
	ClassDB::bind_method(D_METHOD("get_config"), &ArcadeVehicle::get_config);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "config", PROPERTY_HINT_RESOURCE_TYPE, "VehicleConfig"), "set_config", "get_config");

	ClassDB::bind_method(D_METHOD("set_debug_visuals_enabled", "enabled"), &ArcadeVehicle::set_debug_visuals_enabled);
	ClassDB::bind_method(D_METHOD("get_debug_visuals_enabled"), &ArcadeVehicle::get_debug_visuals_enabled);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "debug_visuals_enabled"), "set_debug_visuals_enabled", "get_debug_visuals_enabled");
}

ArcadeVehicle::ArcadeVehicle() {
}

ArcadeVehicle::~ArcadeVehicle() {
	if (grounded_state)
		delete grounded_state;
	if (airborne_state)
		delete airborne_state;
	if (driving_state)
		delete driving_state;
	if (stunt_state)
		delete stunt_state;
}

void ArcadeVehicle::_ready() {
	if (Engine::get_singleton()->is_editor_hint())
		return;

	// 1. Setup HSM
	grounded_state = new GroundedState(this, nullptr);
	airborne_state = new AirborneState(this, nullptr);
	driving_state = new DrivingState(this, grounded_state);
	stunt_state = new StuntState(this, airborne_state);

	current_state = driving_state;
	current_state->enter();

	_setup_vehicle();

	GameManager *gm = GameManager::get_singleton();
	if (gm) {
		gm->register_vehicle(this);
	}
}

void ArcadeVehicle::change_state(VehicleState *new_state) {
	if (current_state == new_state)
		return;

	if (current_state)
		current_state->exit();
	current_state = new_state;
	if (current_state)
		current_state->enter();
}

void ArcadeVehicle::_setup_vehicle() {
	if (config.is_null()) {
		UtilityFunctions::printerr("ArcadeVehicle has no config assigned.");
		return;
	}

	// 1. Setup physics properties
	set_mass(config->get_mass());
	set_center_of_mass_mode(RigidBody3D::CENTER_OF_MASS_MODE_CUSTOM);
	current_com_offset = config->get_center_of_mass_offset();
	set_center_of_mass(current_com_offset);

	// 2. Clear old children if any
	if (chassis_collider) {
		chassis_collider->queue_free();
		chassis_collider = nullptr;
	}
	if (chassis_mesh) {
		chassis_mesh->queue_free();
		chassis_mesh = nullptr;
	}
	for (CSGSphere3D *visual : wheel_visuals) {
		visual->queue_free();
	}
	wheel_visuals.clear();

	// 3. Create Chassis Collider
	chassis_collider = memnew(CollisionShape3D);
	chassis_shape = memnew(BoxShape3D);
	chassis_shape->set_size(config->get_chassis_size());
	chassis_collider->set_shape(chassis_shape);
	add_child(chassis_collider);

	// 4. Create Chassis Mesh (Visual)
	chassis_mesh = memnew(CSGBox3D);
	chassis_mesh->set_size(config->get_chassis_size());
	chassis_mesh->set_use_collision(false);
	add_child(chassis_mesh);

	// 5. Create wheel debug visuals
	TypedArray<WheelConfig> wconfigs = config->get_wheel_configs();
	for (int i = 0; i < wconfigs.size(); i++) {
		Ref<WheelConfig> wc = wconfigs[i];
		if (wc.is_null())
			continue;

		CSGSphere3D *visual = memnew(CSGSphere3D);
		visual->set_radius(wc->get_radius());
		visual->set_radial_segments(12);
		visual->set_rings(6);
		// Color it so it's a visible tire
		visual->set_use_collision(false);

		if (!debug_visuals_enabled) {
			visual->hide();
		}

		add_child(visual);
		wheel_visuals.push_back(visual);
	}
}

float ArcadeVehicle::_calculate_suspension_force(Ref<WheelConfig> wheel, float hit_distance, float delta, Vector3 hardpoint_world, Vector3 local_up) {
	float rest_length = wheel->get_suspension_rest_length();
	float radius = wheel->get_radius();

	// Distance from hardpoint to the ground contact patch (accounting for tire radius)
	float compression = rest_length - (hit_distance - radius);

	if (compression < 0.0f) {
		return 0.0f; // Wheel is in the air
	}

	// Calculate relative velocity of the hardpoint along the suspension axis (local up)
	Vector3 local_pos = hardpoint_world - get_global_transform().origin;

	// get_velocity_at_local_position is not directly available, but we can compute it:
	// V = linear_velocity + angular_velocity x local_pos
	Vector3 pt_vel = get_linear_velocity() + get_angular_velocity().cross(local_pos);

	// Velocity of the wheel pushing into the chassis is dot product with local up
	float v_rel = pt_vel.dot(local_up);

	// Asymmetric damping:
	// If v_rel < 0, chassis is moving down towards ground -> compression
	// If v_rel > 0, chassis is moving up away from ground -> rebound
	float damping_coeff = (v_rel < 0) ? wheel->get_compression_damping() : wheel->get_rebound_damping();

	// Formula: F = (k * x) - (c * v)
	// Because our v_rel is positive when moving UP (expanding), we actually want to subtract damping.
	float force = (wheel->get_suspension_stiffness() * compression) - (damping_coeff * v_rel);

	return MAX(0.0f, force); // Suspension cannot pull the car down
}

void ArcadeVehicle::_physics_process(double p_delta) {
	if (Engine::get_singleton()->is_editor_hint())
		return;
	if (config.is_null())
		return;

	float delta = static_cast<float>(p_delta);
	Transform3D trans = get_global_transform();
	Vector3 local_up = trans.basis.get_column(1).normalized();
	Vector3 local_down = -local_up;

	TypedArray<WheelConfig> wconfigs = config->get_wheel_configs();
	int active_wheel_count = 0;
	int grounded_wheels = 0;

	Vector3 avg_normal = Vector3(0, 0, 0);
	is_on_ramp = false;

	for (int i = 0; i < wconfigs.size(); i++) {
		Ref<WheelConfig> wc = wconfigs[i];
		if (wc.is_null())
			continue;

		// 1. Where is the suspension hardpoint in world space?
		Vector3 hardpoint_local = wc->get_hardpoint_offset();
		Vector3 hardpoint_world = trans.xform(hardpoint_local);

		// 2. Raycast/Spherecast straight down
		float max_dist = wc->get_suspension_rest_length() + wc->get_radius() + 0.1f; // margin

		// Use our new utility!
		TypedArray<RID> exclude;
		exclude.push_back(get_rid());

		MCRaycastHit hit = spherecast_3d(this, hardpoint_world, local_down, max_dist, wc->get_radius() * 0.4f, 0xFFFFFFFF, exclude);

		CSGSphere3D *visual = wheel_visuals[active_wheel_count];

		if (hit.is_hit) {
			float hit_dist = hardpoint_world.distance_to(hit.position);

			// Compute force
			float force_mag = _calculate_suspension_force(wc, hit_dist, delta, hardpoint_world, local_up);

			if (force_mag > 0.0f) {
				// Apply force at the hardpoint pushing locally UP
				apply_force(local_up * force_mag, hardpoint_world - trans.origin);
			}

			// Position visual at contact patch + radius
			if (debug_visuals_enabled) {
				visual->set_global_position(hit.position + hit.normal * wc->get_radius());
			}
			grounded_wheels++;
			avg_normal += hit.normal;
		} else {
			// Wheel is fully extended
			if (debug_visuals_enabled) {
				visual->set_global_position(hardpoint_world + local_down * wc->get_suspension_rest_length());
			}
		}

		active_wheel_count++;
	}

	// 3. State Transitions & Logic
	Vector3 forward_dir = -trans.basis.get_column(2).normalized();
	float forward_speed = get_linear_velocity().dot(forward_dir);

	if (grounded_wheels > 0) {
		avg_normal /= (float)grounded_wheels;
		if (avg_normal.y < config->get_ramp_detection_threshold()) {
			is_on_ramp = true;
		}

		if (grounded_wheels >= 2) {
			change_state(driving_state);
			in_stunt_rotation = false;
			stunt_requested = false;
		}
	} else {
		// In the air
		if (!in_stunt_rotation) {
			change_state(airborne_state);
		}
	}

	// Trigger stunt
	bool is_active = (game_manager && game_manager->get_active_target() == this);
	const ActionState *input_state = player_input ? &player_input->get_state() : nullptr;

	if (is_active && is_on_ramp && input_state && input_state->character.jump && forward_speed > 10.0f) {
		stunt_requested = true;
	}

	if (stunt_requested && grounded_wheels == 0) {
		change_state(stunt_state);
		in_stunt_rotation = true;
	}

	// Early Recovery Check
	if (in_stunt_rotation) {
		float recovery_ray_dist = 5.0f;
		TypedArray<RID> exclude;
		exclude.push_back(get_rid());
		MCRaycastHit recovery_hit = raycast_3d(this, trans.origin, trans.origin + Vector3(0, -recovery_ray_dist, 0), 0xFFFFFFFF, exclude);
		if (recovery_hit.is_hit) {
			float dist = trans.origin.distance_to(recovery_hit.position);
			if (dist < config->get_stunt_recovery_height()) {
				in_stunt_rotation = false;
				stunt_requested = false;
				change_state(airborne_state);
			}
		}
	}

	// 4. Delegate to HSM
	if (current_state) {
		current_state->physics_update(delta);
	}

	// 5. COM Interpolation (Shared logic)
	Vector3 target_com = config->get_center_of_mass_offset();
	if (in_stunt_rotation || (stunt_requested && grounded_wheels > 0)) {
		target_com = config->get_stunt_com_offset();
	}
	current_com_offset = current_com_offset.lerp(target_com, config->get_stunt_com_interpolation_speed() * delta);
	set_center_of_mass(current_com_offset);

	_update_debug_arrows();
}

void ArcadeVehicle::_process_inputs() {
	bool is_active = (game_manager && game_manager->get_active_target() == this);
	const ActionState *input_state = player_input ? &player_input->get_state() : nullptr;

	if (is_active && input_state) {
		current_input.throttle = input_state->vehicle.throttle;
		current_input.brake = input_state->vehicle.brake;
		current_input.steer = input_state->vehicle.steering;
	} else {
		// Zero out inputs if not active
		current_input.throttle = 0.0f;
		current_input.brake = 0.0f;
		current_input.steer = 0.0f;
	}
}

void ArcadeVehicle::_apply_acceleration(float delta) {
	Transform3D trans = get_global_transform();
	Vector3 forward_dir = -trans.basis.get_column(2).normalized(); // Local -Z is forward
	float current_forward_speed = get_linear_velocity().dot(forward_dir);

	float input_drive = current_input.throttle - current_input.brake;
	float target_speed = config->get_max_speed() * input_drive;
	float speed_diff = target_speed - current_forward_speed;
	float accel_force = 0.0f;

	if (abs(input_drive) > 0.01f) {
		bool is_braking = (input_drive * current_forward_speed) < -0.1f;
		float force_limit = is_braking ? config->get_brake_decel() : config->get_max_accel_force();

		float speed_ratio = abs(current_forward_speed) / config->get_max_speed();
		float accel_curve = 1.0f - (speed_ratio * speed_ratio);
		accel_curve = MAX(0.1f, accel_curve);

		accel_force = force_limit * accel_curve * input_drive;
	} else {
		if (abs(current_forward_speed) > 0.1f) {
			accel_force = -1000.0f * (current_forward_speed > 0.0f ? 1.0f : -1.0f);
		}
	}

	apply_central_force(forward_dir * accel_force * 0.7f);
	set_linear_velocity(get_linear_velocity() + forward_dir * speed_diff * config->get_arcade_assist() * delta * 0.3f);
}

void ArcadeVehicle::_apply_steering(float delta) {
	Transform3D trans = get_global_transform();
	Vector3 forward_dir = -trans.basis.get_column(2).normalized();
	Vector3 up_dir = trans.basis.get_column(1).normalized();
	float current_forward_speed = get_linear_velocity().dot(forward_dir);

	float max_steer_rad = Math::deg_to_rad(config->get_max_steer_angle_deg());
	float steer_speed_factor = CLAMP(1.0f - (abs(current_forward_speed) / config->get_max_speed()), 0.3f, 1.0f);
	float steer_angle = current_input.steer * max_steer_rad * steer_speed_factor;

	if (abs(current_forward_speed) > 1.0f && abs(steer_angle) > 0.01f) {
		float dir_sign = (current_forward_speed > 0.0f) ? 1.0f : -1.0f;
		// Positive steer turns Right (CW), which is Negative Y rotation in Godot
		float steering_torque = -steer_angle * config->get_mass() * 5.0f * dir_sign;

		apply_torque(up_dir * steering_torque);
	}
}

void ArcadeVehicle::_apply_lateral_friction(float delta) {
	Transform3D trans = get_global_transform();
	Vector3 right_dir = trans.basis.get_column(0).normalized(); // Local X is right

	// How fast are we sliding sideways?
	float lateral_velocity = get_linear_velocity().dot(right_dir);

	// Dynamic Drift Grip
	Vector3 forward_dir = -trans.basis.get_column(2).normalized();
	float forward_speed = abs(get_linear_velocity().dot(forward_dir));
	float steering_intensity = abs(current_input.steer);

	float current_grip = config->get_base_grip();

	if (forward_speed > config->get_drift_speed_threshold() && steering_intensity > config->get_drift_steering_threshold()) {
		// Calculate drift lerp. With a keyboard, steering_intensity is 1.0, meaning full drift grip immediately.
		float drift_intensity = (steering_intensity - config->get_drift_steering_threshold()) / (1.0f - config->get_drift_steering_threshold());
		drift_intensity = CLAMP(drift_intensity, 0.0f, 1.0f);

		current_grip = Math::lerp(config->get_base_grip(), config->get_drift_grip(), drift_intensity);
	}

	// F = ma, so to kill velocity `v` in time `t`, F = m * (v/t)
	// We use delta to act as an impulse, simulating instantaneous grip
	float lateral_force_magnitude = -lateral_velocity * config->get_mass() * current_grip / delta;

	// Optional limit to prevent exploding physics if grip is too high
	float max_lateral_grip_force = config->get_mass() * 50.0f / delta;
	lateral_force_magnitude = CLAMP(lateral_force_magnitude, -max_lateral_grip_force, max_lateral_grip_force);

	apply_central_force(right_dir * lateral_force_magnitude);
}

void ArcadeVehicle::_apply_stability(float delta) {
	Transform3D trans = get_global_transform();
	Vector3 forward_dir = -trans.basis.get_column(2).normalized();
	Vector3 up_dir = trans.basis.get_column(1).normalized();

	// --- 1. DOWNFORCE ---
	// Artificial gravity to keep the car from bouncing around like a beach ball
	float downforce_mag = config->get_downforce();
	apply_central_force(Vector3(0, -1, 0) * downforce_mag);

	// --- 2. YAW DAMPING ---
	// Stop the car from spinning like a top when we stop steering
	Vector3 ang_vel = get_angular_velocity();
	float yaw_vel = ang_vel.dot(up_dir);

	bool is_steering = abs(current_input.steer) > 0.01f;
	float damping_multiplier = is_steering ? 1.0f : config->get_angular_damping();

	// Apply counter-torque proportional to yaw velocity
	// Higher damping when not steering gives that "locked-in" arcade feel
	float damping_torque = -yaw_vel * config->get_mass() * damping_multiplier;
	apply_torque(up_dir * damping_torque);

	// --- 3. VELOCITY ALIGNMENT (THE "CHESS" PASS) ---
	// Gently rotate the linear velocity vector to match the car's orientation
	// This makes it feel like the car's wheels are actually directional
	Vector3 current_vel = get_linear_velocity();
	float current_speed = current_vel.length();

	if (current_speed > 1.0f) {
		Vector3 target_vel_dir = forward_dir;
		if (current_vel.dot(forward_dir) < 0) {
			target_vel_dir = -forward_dir; // Going in reverse
		}

		// Interpolate the direction of velocity
		Vector3 current_vel_dir = current_vel.normalized();
		Vector3 new_vel_dir = current_vel_dir.lerp(target_vel_dir, config->get_velocity_alignment() * delta);

		set_linear_velocity(new_vel_dir.normalized() * current_speed);
	}
}

void ArcadeVehicle::set_config(const Ref<VehicleConfig> &p_config) {
	config = p_config;
	if (is_inside_tree() && !Engine::get_singleton()->is_editor_hint()) {
		_setup_vehicle();
	}
}

Ref<VehicleConfig> ArcadeVehicle::get_config() const {
	return config;
}

void ArcadeVehicle::set_debug_visuals_enabled(bool p_enabled) {
	debug_visuals_enabled = p_enabled;
	for (CSGSphere3D *visual : wheel_visuals) {
		visual->set_visible(debug_visuals_enabled);
	}
}

bool ArcadeVehicle::get_debug_visuals_enabled() const {
	return debug_visuals_enabled;
}

} // namespace godot
