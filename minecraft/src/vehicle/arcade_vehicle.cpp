#include "arcade_vehicle.h"
#include "../cui/cui.h"
#include "../game_manager/game_manager.h"
#include "../game_manager/player_input.h"
#include "../utils/raycast/mc_raycast.h"
#include "ai/vehicle_states.h"
#include "debug_draw/debug_manager.h"
#include "godot_cpp/classes/csg_box3d.hpp"
#include "ui/arcade_vehicle_ui.h"
#include <godot_cpp/classes/canvas_layer.hpp>
#include <godot_cpp/classes/config_file.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/global_constants.hpp>
#include <godot_cpp/classes/input.hpp>
#include <godot_cpp/classes/physics_direct_body_state3d.hpp>
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

	ClassDB::bind_method(D_METHOD("_on_ui_toggle"), &ArcadeVehicle::_on_ui_toggle);
	ClassDB::bind_method(D_METHOD("save_settings"), &ArcadeVehicle::save_settings);
	ClassDB::bind_method(D_METHOD("load_settings"), &ArcadeVehicle::load_settings);
	ClassDB::bind_method(D_METHOD("_on_ui_slider_value_changed", "value", "property"), &ArcadeVehicle::_on_ui_slider_value_changed);
}

ArcadeVehicle::ArcadeVehicle() {
	is_boosting = false;
	boost_speed_bonus = 0.0f;
	ui_helper = nullptr;
	ui_root = nullptr;
}

ArcadeVehicle::~ArcadeVehicle() {
	if (ui_helper) {
		delete ui_helper;
		ui_helper = nullptr;
	}
	if (grounded_state)
		delete grounded_state;
	if (airborne_state)
		delete airborne_state;
	if (driving_state)
		delete driving_state;
	if (stunt_state)
		delete stunt_state;

	DebugManager *dm = DebugManager::get_singleton();
	if (dm) {
		dm->clear_text("veh_" + get_name() + "_drift");
		dm->clear_trajectory("traj_" + get_name());
	}
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

	// Setup UI
	ui_root = CUI::create_on_new_layer(this);
	ui_helper = new ArcadeVehicleUI();
	ui_helper->setup(this, ui_root);
	load_settings();

	if (config.is_valid()) {
		nitro_fuel = config->get_nitro_max_fuel();
	}
}

void ArcadeVehicle::_exit_tree() {
	GameManager *gm = GameManager::get_singleton();
	if (gm && gm->get_vehicle() == this) {
		gm->register_vehicle(nullptr);
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
			float force_mag = _calculate_suspension_force(wc, hit_dist, p_delta, hardpoint_world, local_up);

			if (force_mag > 0.0f) {
				Vector3 force_dir;
				_handle_wall_collision_and_spin(i, hit, force_dir, force_mag);
				apply_force(force_dir * force_mag, hardpoint_world - trans.origin);
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
		float recovery_ray_dist = 15.0f;
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
		current_state->physics_update(p_delta);
	}

	// 5. COM Interpolation (Shared logic)
	Vector3 target_com = config->get_center_of_mass_offset();
	if (in_stunt_rotation || (stunt_requested && grounded_wheels > 0)) {
		target_com = config->get_stunt_com_offset();
	}
	current_com_offset = current_com_offset.lerp(target_com, config->get_stunt_com_interpolation_speed() * p_delta);
	set_center_of_mass(current_com_offset);

	_update_debug_arrows();

	// Dynamic UI Visibility & Updates
	if (ui_root) {
		CanvasLayer *cl = Object::cast_to<CanvasLayer>(ui_root->get_parent());
		if (cl) {
			cl->set_visible(is_active);
		}
	}

	if (is_active && ui_helper) {
		ui_helper->update_graph(get_linear_velocity().length());
	}

	debug_draw_trajectory((float)p_delta);
}

void ArcadeVehicle::_integrate_forces(PhysicsDirectBodyState3D *state) {
	if (state == nullptr) {
		return;
	}

	Vector3 vel = state->get_linear_velocity();

	// 1. Apply acceleration nudge
	vel += velocity_nudge_accumulator;

	// 2. Apply velocity alignment if requested
	if (should_align_velocity) {
		float current_speed = vel.length();
		if (current_speed > 1.0f) {
			Transform3D trans = state->get_transform();
			Vector3 forward_dir = -trans.basis.get_column(2).normalized();

			Vector3 target_vel_dir = forward_dir;
			if (vel.dot(forward_dir) < 0) {
				target_vel_dir = -forward_dir; // Going in reverse
			}

			Vector3 current_vel_dir = vel.normalized();
			float alignment_speed = config->get_velocity_alignment();
			if (is_drifting) {
				alignment_speed *= 0.15f; // Reduce velocity alignment when drifting to allow sliding sideways
			}
			Vector3 new_vel_dir = current_vel_dir.lerp(target_vel_dir, alignment_speed * state->get_step());

			vel = new_vel_dir.normalized() * current_speed;
		}
	}

	state->set_linear_velocity(vel);

	// Reset integration accumulators/flags
	velocity_nudge_accumulator = Vector3(0.0f, 0.0f, 0.0f);
	should_align_velocity = false;
}

void ArcadeVehicle::_process_inputs() {
	bool is_active = (game_manager && game_manager->get_active_target() == this);
	const ActionState *input_state = player_input ? &player_input->get_state() : nullptr;

	if (is_active && input_state) {
		current_input.throttle = input_state->vehicle.throttle;
		current_input.brake = input_state->vehicle.brake;
		current_input.steer = input_state->vehicle.steering;
		current_input.handbrake = input_state->vehicle.handbrake;
		current_input.nitro = input_state->vehicle.nitro;
	} else {
		// Zero out inputs if not active
		current_input.throttle = 0.0f;
		current_input.brake = 0.0f;
		current_input.steer = 0.0f;
		current_input.handbrake = false;
		current_input.nitro = false;
	}
}

void ArcadeVehicle::_apply_acceleration(float delta) {
	Transform3D trans = get_global_transform();
	Vector3 forward_dir = -trans.basis.get_column(2).normalized(); // Local -Z is forward
	float current_forward_speed = get_linear_velocity().dot(forward_dir);

	// Handle manual Nitro/Boost input and fuel consumption
	bool started_boosting = false;
	if (current_input.nitro && nitro_fuel > 0.0f) {
		if (!is_boosting) {
			started_boosting = true;
		}
		is_boosting = true;
		nitro_fuel = MAX(0.0f, nitro_fuel - config->get_nitro_depletion_rate() * delta);
		boost_speed_bonus = config->get_drift_boost_max_speed_bonus();
	} else {
		is_boosting = false;
		boost_speed_bonus = 0.0f;
	}

	float input_drive = current_input.throttle - current_input.brake;
	// Nitro auto-accelerates forward if the player is not actively braking
	if (is_boosting && current_input.brake <= 0.01f) {
		input_drive = 1.0f;
	}

	// Calculate max speed, taking drift slowdown and speed boost into account
	float speed_multiplier = 1.0f;
	if (is_drifting) {
		speed_multiplier = config->get_drift_slowdown_factor();
	}
	float max_speed = config->get_max_speed() * speed_multiplier;
	if (is_boosting) {
		max_speed += boost_speed_bonus;
	}

	float target_speed = max_speed * input_drive;
	float speed_diff = target_speed - current_forward_speed;
	float accel_force = 0.0f;

	if (abs(input_drive) > 0.01f) {
		bool is_braking = (input_drive * current_forward_speed) < -0.1f;
		float force_limit = is_braking ? config->get_brake_decel() : config->get_max_accel_force();

		float accel_curve = 1.0f;
		if (is_boosting) {
			// Increase acceleration force during boost to push past normal speed quickly
			force_limit *= 2.0f;
			// Keep full acceleration force during boost (no drop off as we approach max speed)
			accel_curve = 1.0f;
		} else {
			float speed_ratio = abs(current_forward_speed) / max_speed;
			accel_curve = 1.0f - (speed_ratio * speed_ratio);
			accel_curve = MAX(0.1f, accel_curve);
		}

		accel_force = force_limit * accel_curve * input_drive;
	} else {
		if (abs(current_forward_speed) > 0.1f) {
			// If boosting, preserve momentum and do not apply standard heavy engine brake
			if (!is_boosting) {
				accel_force = -1000.0f * (current_forward_speed > 0.0f ? 1.0f : -1.0f);
			}
		}
	}

	_apply_longitudinal_force_with_pitch(forward_dir * accel_force * 0.7f);

	// Apply arcade assist nudge; make it stronger during Nitro boost to help reach top speed quickly
	float nudge_strength = is_boosting ? 0.6f : 0.3f;
	velocity_nudge_accumulator += forward_dir * speed_diff * config->get_arcade_assist() * delta * nudge_strength;

	if (started_boosting) {
		// Initial speed kick forward to feel responsive and punchy
		velocity_nudge_accumulator += forward_dir * (boost_speed_bonus * 0.2f);
	}
}

void ArcadeVehicle::_apply_steering(float delta) {
	Transform3D trans = get_global_transform();
	Vector3 forward_dir = -trans.basis.get_column(2).normalized();
	Vector3 up_dir = trans.basis.get_column(1).normalized();
	float current_forward_speed = get_linear_velocity().dot(forward_dir);

	float max_steer_rad = Math::deg_to_rad(config->get_max_steer_angle_deg());

	// Relax steering angle damping at high speeds when drifting for tighter turns
	float min_steer_clamp = is_drifting ? 0.6f : 0.3f;
	float steer_speed_factor = CLAMP(1.0f - (abs(current_forward_speed) / config->get_max_speed()), min_steer_clamp, 1.0f);
	float steer_angle = current_input.steer * max_steer_rad * steer_speed_factor;

	if (abs(current_forward_speed) > 1.0f && abs(steer_angle) > 0.01f) {
		float dir_sign = (current_forward_speed > 0.0f) ? 1.0f : -1.0f;

		// Apply drift turning force multiplier if drifting
		float steer_multiplier = 1.0f;
		if (is_drifting) {
			steer_multiplier = config->get_drift_steer_torque_multiplier();
		}

		// Positive steer turns Right (CW), which is Negative Y rotation in Godot
		float steering_torque = -steer_angle * config->get_mass() * 5.0f * dir_sign * steer_multiplier;

		apply_torque(up_dir * steering_torque);
	}
}

void ArcadeVehicle::_apply_lateral_friction(float delta) {
	Transform3D trans = get_global_transform();
	Vector3 right_dir = trans.basis.get_column(0).normalized(); // Local X is right
	Vector3 up_dir = trans.basis.get_column(1).normalized();

	// How fast are we sliding sideways?
	float lateral_velocity = get_linear_velocity().dot(right_dir);

	// Dynamic Drift Grip
	Vector3 forward_dir = -trans.basis.get_column(2).normalized();
	float forward_speed = abs(get_linear_velocity().dot(forward_dir));

	float steering_intensity = abs(current_input.steer);

	bool was_drifting = is_drifting;

	// State machine for drifting (Mario Kart style)
	if (is_drifting) {
		// Stop drifting if handbrake is released or speed drops below threshold
		if (!current_input.handbrake || forward_speed < config->get_drift_speed_threshold()) {
			is_drifting = false;
		}
	} else {
		// Start drifting if holding handbrake, steering, and going fast enough
		if (current_input.handbrake && forward_speed > config->get_drift_speed_threshold() &&
				steering_intensity > config->get_drift_steering_threshold()) {
			is_drifting = true;

			// Hop impulse!
			velocity_nudge_accumulator += up_dir * 1.5f;
		}
	}

	// Nitro refuel during active drift
	if (is_drifting) {
		nitro_fuel = MIN(config->get_nitro_max_fuel(), nitro_fuel + config->get_nitro_refuel_rate() * delta);
	}

	float current_grip = config->get_base_grip();

	if (is_drifting) {
		current_grip = config->get_drift_grip();
	}

	// F = ma, so to kill velocity `v` in time `t`, F = m * (v/t)
	// We use delta to act as an impulse, simulating instantaneous grip
	float lateral_force_magnitude = -lateral_velocity * config->get_mass() * current_grip / delta;

	// Optional limit to prevent exploding physics if grip is too high
	float max_lateral_grip_force = config->get_mass() * 50.0f / delta;
	lateral_force_magnitude = CLAMP(lateral_force_magnitude, -max_lateral_grip_force, max_lateral_grip_force);

	//----------- Roll
	_apply_lateral_force_with_roll(right_dir * lateral_force_magnitude);
}

void ArcadeVehicle::_apply_stability(float delta) {
	Transform3D trans = get_global_transform();
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

	should_align_velocity = true;
}

float ArcadeVehicle::_get_average_contact_patch_y() const {
	if (config.is_null()) {
		return -1.0f;
	}
	float total_wheel_y = 0.0f;
	int valid_wheels = 0;
	TypedArray<WheelConfig> wconfigs = config->get_wheel_configs();
	for (int i = 0; i < wconfigs.size(); i++) {
		Ref<WheelConfig> wc = wconfigs[i];
		if (wc.is_valid()) {
			total_wheel_y += wc->get_hardpoint_offset().y - wc->get_suspension_rest_length() - wc->get_radius();
			valid_wheels++;
		}
	}
	return (valid_wheels > 0) ? (total_wheel_y / (float)valid_wheels) : -1.0f;
}

void ArcadeVehicle::_apply_lateral_force_with_roll(Vector3 p_force_global) {
	if (config.is_null()) {
		return;
	}
	Transform3D trans = get_global_transform();
	float contact_patch_y = _get_average_contact_patch_y();

	float roll_inf = config->get_roll_influence();
	float force_y = current_com_offset.y + (contact_patch_y - current_com_offset.y) * roll_inf;

	Vector3 local_force_pos(current_com_offset.x, force_y, current_com_offset.z);
	Vector3 force_offset_global = trans.basis.xform(local_force_pos);

	apply_force(p_force_global, force_offset_global);
}

void ArcadeVehicle::_apply_longitudinal_force_with_pitch(Vector3 p_force_global) {
	if (config.is_null()) {
		return;
	}
	Transform3D trans = get_global_transform();
	float contact_patch_y = _get_average_contact_patch_y();

	float pitch_inf = config->get_pitch_influence();
	float force_y = current_com_offset.y + (contact_patch_y - current_com_offset.y) * pitch_inf;

	Vector3 local_force_pos(current_com_offset.x, force_y, current_com_offset.z);
	Vector3 force_offset_global = trans.basis.xform(local_force_pos);

	apply_force(p_force_global, force_offset_global);
}

void ArcadeVehicle::_handle_wall_collision_and_spin(int p_wheel_index, const MCRaycastHit &p_hit, Vector3 &r_force_dir, float &r_force_mag) {
	Transform3D trans = get_global_transform();
	Vector3 local_up = trans.basis.get_column(1).normalized();
	
	float up_dot = p_hit.normal.dot(local_up);
	r_force_dir = local_up;

	if (up_dot < 0.6f) {
		// Wall/slope collision: push away from the wall horizontally
		r_force_dir = p_hit.normal;
		r_force_mag *= 0.5f; // Soften the force to avoid abrupt bounces
		r_force_mag = MIN(r_force_mag, config->get_mass() * 30.0f); // Cap the force to keep it smooth and stable

		// Spin assist: apply horizontal torque when front wheels scrape a wall
		Vector3 forward_dir = -trans.basis.get_column(2).normalized();
		float forward_speed = get_linear_velocity().dot(forward_dir);
		if (forward_speed > 3.0f) {
			float speed_scale = CLAMP(forward_speed / 10.0f, 0.5f, 1.5f);
			float assist_torque = config->get_mass() * 25.0f * speed_scale;
			if (p_wheel_index == 0) {
				// Front-Left hits wall -> spin clockwise (positive yaw torque)
				apply_torque(local_up * assist_torque);
			} else if (p_wheel_index == 1) {
				// Front-Right hits wall -> spin counter-clockwise (negative yaw torque)
				apply_torque(-local_up * assist_torque);
			}
		}
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

void ArcadeVehicle::_on_ui_toggle() {
	if (ui_helper) {
		ui_helper->toggle_visibility();
	}
}

void ArcadeVehicle::_on_ui_slider_value_changed(double p_value, String p_property) {
	set_ui_var(p_property, (float)p_value);
}

void ArcadeVehicle::save_settings() {
	if (config.is_null()) {
		return;
	}

	Ref<ConfigFile> cfg;
	cfg.instantiate();

	cfg->set_value("Vehicle", "max_speed", config->get_max_speed());
	cfg->set_value("Vehicle", "max_accel_force", config->get_max_accel_force());
	cfg->set_value("Vehicle", "brake_decel", config->get_brake_decel());
	cfg->set_value("Vehicle", "arcade_assist", config->get_arcade_assist());
	cfg->set_value("Vehicle", "max_steer_angle_deg", config->get_max_steer_angle_deg());
	cfg->set_value("Vehicle", "base_grip", config->get_base_grip());
	cfg->set_value("Vehicle", "drift_grip", config->get_drift_grip());
	cfg->set_value("Vehicle", "downforce", config->get_downforce());
	cfg->set_value("Vehicle", "angular_damping", config->get_angular_damping());
	cfg->set_value("Vehicle", "velocity_alignment", config->get_velocity_alignment());
	cfg->set_value("Vehicle", "nitro_max_fuel", config->get_nitro_max_fuel());
	cfg->set_value("Vehicle", "nitro_refuel_rate", config->get_nitro_refuel_rate());
	cfg->set_value("Vehicle", "nitro_depletion_rate", config->get_nitro_depletion_rate());
	cfg->set_value("Vehicle", "roll_influence", config->get_roll_influence());
	cfg->set_value("Vehicle", "pitch_influence", config->get_pitch_influence());

	cfg->set_value("Stunt", "stunt_torque_strength", config->get_stunt_torque_strength());
	cfg->set_value("Stunt", "stunt_com_interpolation_speed", config->get_stunt_com_interpolation_speed());
	cfg->set_value("Stunt", "ramp_detection_threshold", config->get_ramp_detection_threshold());
	cfg->set_value("Stunt", "stunt_recovery_height", config->get_stunt_recovery_height());

	cfg->save("user://vehicle_settings.cfg");
	UtilityFunctions::print("ArcadeVehicle: Settings saved to user://vehicle_settings.cfg");
}

void ArcadeVehicle::load_settings() {
	if (config.is_null()) {
		return;
	}

	Ref<ConfigFile> cfg;
	cfg.instantiate();

	Error err = cfg->load("user://vehicle_settings.cfg");
	if (err != OK) {
		return;
	}

	config->set_max_speed(cfg->get_value("Vehicle", "max_speed", config->get_max_speed()));
	config->set_max_accel_force(cfg->get_value("Vehicle", "max_accel_force", config->get_max_accel_force()));
	config->set_brake_decel(cfg->get_value("Vehicle", "brake_decel", config->get_brake_decel()));
	config->set_arcade_assist(cfg->get_value("Vehicle", "arcade_assist", config->get_arcade_assist()));
	config->set_max_steer_angle_deg(cfg->get_value("Vehicle", "max_steer_angle_deg", config->get_max_steer_angle_deg()));
	config->set_base_grip(cfg->get_value("Vehicle", "base_grip", config->get_base_grip()));
	config->set_drift_grip(cfg->get_value("Vehicle", "drift_grip", config->get_drift_grip()));
	config->set_downforce(cfg->get_value("Vehicle", "downforce", config->get_downforce()));
	config->set_angular_damping(cfg->get_value("Vehicle", "angular_damping", config->get_angular_damping()));
	config->set_velocity_alignment(cfg->get_value("Vehicle", "velocity_alignment", config->get_velocity_alignment()));
	config->set_nitro_max_fuel(cfg->get_value("Vehicle", "nitro_max_fuel", config->get_nitro_max_fuel()));
	config->set_nitro_refuel_rate(cfg->get_value("Vehicle", "nitro_refuel_rate", config->get_nitro_refuel_rate()));
	config->set_nitro_depletion_rate(cfg->get_value("Vehicle", "nitro_depletion_rate", config->get_nitro_depletion_rate()));
	config->set_roll_influence(cfg->get_value("Vehicle", "roll_influence", config->get_roll_influence()));
	config->set_pitch_influence(cfg->get_value("Vehicle", "pitch_influence", config->get_pitch_influence()));

	config->set_stunt_torque_strength(cfg->get_value("Stunt", "stunt_torque_strength", config->get_stunt_torque_strength()));
	config->set_stunt_com_interpolation_speed(cfg->get_value("Stunt", "stunt_com_interpolation_speed", config->get_stunt_com_interpolation_speed()));
	config->set_ramp_detection_threshold(cfg->get_value("Stunt", "ramp_detection_threshold", config->get_ramp_detection_threshold()));
	config->set_stunt_recovery_height(cfg->get_value("Stunt", "stunt_recovery_height", config->get_stunt_recovery_height()));

	// Update UI values if UI exists
	if (ui_root) {
		ui_root->set_value("max_speed", config->get_max_speed());
		ui_root->set_value("max_accel_force", config->get_max_accel_force());
		ui_root->set_value("brake_decel", config->get_brake_decel());
		ui_root->set_value("arcade_assist", config->get_arcade_assist());
		ui_root->set_value("max_steer_angle_deg", config->get_max_steer_angle_deg());
		ui_root->set_value("base_grip", config->get_base_grip());
		ui_root->set_value("drift_grip", config->get_drift_grip());
		ui_root->set_value("downforce", config->get_downforce());
		ui_root->set_value("angular_damping", config->get_angular_damping());
		ui_root->set_value("velocity_alignment", config->get_velocity_alignment());
		ui_root->set_value("roll_influence", config->get_roll_influence());
		ui_root->set_value("pitch_influence", config->get_pitch_influence());

		ui_root->set_value("stunt_torque_strength", config->get_stunt_torque_strength());
		ui_root->set_value("stunt_com_interpolation_speed", config->get_stunt_com_interpolation_speed());
		ui_root->set_value("ramp_detection_threshold", config->get_ramp_detection_threshold());
		ui_root->set_value("stunt_recovery_height", config->get_stunt_recovery_height());
	}

	UtilityFunctions::print("ArcadeVehicle: Settings loaded from user://vehicle_settings.cfg");
}

float ArcadeVehicle::get_ui_var(const String &p_name) const {
	if (config.is_null())
		return 0.0f;
	if (p_name == "max_speed")
		return config->get_max_speed();
	if (p_name == "max_accel_force")
		return config->get_max_accel_force();
	if (p_name == "brake_decel")
		return config->get_brake_decel();
	if (p_name == "arcade_assist")
		return config->get_arcade_assist();
	if (p_name == "max_steer_angle_deg")
		return config->get_max_steer_angle_deg();
	if (p_name == "base_grip")
		return config->get_base_grip();
	if (p_name == "drift_grip")
		return config->get_drift_grip();
	if (p_name == "downforce")
		return config->get_downforce();
	if (p_name == "angular_damping")
		return config->get_angular_damping();
	if (p_name == "velocity_alignment")
		return config->get_velocity_alignment();
	if (p_name == "nitro_max_fuel")
		return config->get_nitro_max_fuel();
	if (p_name == "nitro_refuel_rate")
		return config->get_nitro_refuel_rate();
	if (p_name == "nitro_depletion_rate")
		return config->get_nitro_depletion_rate();
	if (p_name == "roll_influence")
		return config->get_roll_influence();
	if (p_name == "pitch_influence")
		return config->get_pitch_influence();
	if (p_name == "stunt_torque_strength")
		return config->get_stunt_torque_strength();
	if (p_name == "stunt_com_interpolation_speed")
		return config->get_stunt_com_interpolation_speed();
	if (p_name == "ramp_detection_threshold")
		return config->get_ramp_detection_threshold();
	if (p_name == "stunt_recovery_height")
		return config->get_stunt_recovery_height();
	return 0.0f;
}

void ArcadeVehicle::set_ui_var(const String &p_name, float p_value) {
	if (config.is_null())
		return;
	if (p_name == "max_speed")
		config->set_max_speed(p_value);
	else if (p_name == "max_accel_force")
		config->set_max_accel_force(p_value);
	else if (p_name == "brake_decel")
		config->set_brake_decel(p_value);
	else if (p_name == "arcade_assist")
		config->set_arcade_assist(p_value);
	else if (p_name == "max_steer_angle_deg")
		config->set_max_steer_angle_deg(p_value);
	else if (p_name == "base_grip")
		config->set_base_grip(p_value);
	else if (p_name == "drift_grip")
		config->set_drift_grip(p_value);
	else if (p_name == "downforce")
		config->set_downforce(p_value);
	else if (p_name == "angular_damping")
		config->set_angular_damping(p_value);
	else if (p_name == "velocity_alignment")
		config->set_velocity_alignment(p_value);
	else if (p_name == "nitro_max_fuel")
		config->set_nitro_max_fuel(p_value);
	else if (p_name == "nitro_refuel_rate")
		config->set_nitro_refuel_rate(p_value);
	else if (p_name == "nitro_depletion_rate")
		config->set_nitro_depletion_rate(p_value);
	else if (p_name == "roll_influence")
		config->set_roll_influence(p_value);
	else if (p_name == "pitch_influence")
		config->set_pitch_influence(p_value);
	else if (p_name == "stunt_torque_strength")
		config->set_stunt_torque_strength(p_value);
	else if (p_name == "stunt_com_interpolation_speed")
		config->set_stunt_com_interpolation_speed(p_value);
	else if (p_name == "ramp_detection_threshold")
		config->set_ramp_detection_threshold(p_value);
	else if (p_name == "stunt_recovery_height")
		config->set_stunt_recovery_height(p_value);
}

} // namespace godot
