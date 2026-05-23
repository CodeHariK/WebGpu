#include "arcade_vehicle.h"
#include "../cui/cui.h"
#include "../game_manager/game_manager.h"
#include "../game_manager/player_input.h"
#include "../utils/raycast/mc_raycast.h"
#include "ai/vehicle_states.h"
#include "ui/arcade_vehicle_ui.h"
#include <godot_cpp/classes/canvas_layer.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/input.hpp>
#include <godot_cpp/core/class_db.hpp>

namespace godot {

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

	_process_inputs();

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

		// Offset starting point upward to prevent clipping below ground
		float cast_offset = 0.3f;
		Vector3 cast_start = hardpoint_world + local_up * cast_offset;

		// 2. Raycast/Spherecast straight down
		float max_dist = wc->get_suspension_rest_length() + wc->get_radius() + 0.1f + cast_offset; // margin + offset

		// Use our new utility!
		TypedArray<RID> exclude;
		exclude.push_back(get_rid());

		MCRaycastHit hit = spherecast_3d(this, cast_start, local_down, max_dist, wc->get_radius() * 0.4f, 0xFFFFFFFF, exclude);

		CSGSphere3D *visual = wheel_visuals[active_wheel_count];

		if (hit.is_hit) {
			// Project the hit position relative to the hardpoint along local_down
			float hit_dist = (hit.position - hardpoint_world).dot(local_down);

			// Compute force
			float force_mag = _calculate_suspension_force(wc, hit_dist, p_delta, hardpoint_world, local_up);

			if (force_mag > 0.0f) {
				Vector3 force_dir;
				_handle_wall_collision_and_spin(i, hit, force_dir, force_mag);
				apply_force(force_dir * force_mag, hardpoint_world - trans.origin);
			}

			// Position visual along the suspension axis, clamped to avoid clipping into chassis
			if (debug_visuals_enabled) {
				Vector3 target_pos = hit.position + hit.normal * wc->get_radius();
				float displacement = (target_pos - hardpoint_world).dot(local_down);
				displacement = CLAMP(displacement, 0.0f, wc->get_suspension_rest_length());
				visual->set_global_position(hardpoint_world + local_down * displacement);
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
	bool is_active = (game_manager && game_manager->get_active_target() == this);

	if (grounded_wheels > 0) {
		avg_normal /= (float)grounded_wheels;
		if (avg_normal.y < 0.9f) {
			is_on_ramp = true;
		}

		Vector3 local_x = trans.basis.get_column(0).normalized();
		last_roll_tilt = local_x.y;

		if (grounded_wheels >= 2) {
			if (current_state != driving_state && current_state != drifting_state) {
				change_state(driving_state);
			}
		}
	} else {
		// In the air
		bool can_glide = local_up.y > 0.7f && forward_speed > 10.0f && current_state != ramp_roll_state && current_state != ramp_spin_state;

		if (is_active && current_input.glide && can_glide) {
			if (current_state != gliding_state) {
				change_state(gliding_state);
			}
		} else {
			if (current_state == gliding_state) {
				change_state(airborne_state);
			} else if (current_state != ramp_spin_state && current_state != ramp_roll_state && current_state != airborne_state) {
				change_state(airborne_state);
			}

			if (was_on_ramp && current_state != ramp_spin_state && current_state != ramp_roll_state && current_state != gliding_state && forward_speed > 10.0f) {
				if (abs(last_roll_tilt) > 0.4f) {
					ramp_roll_state->set_roll_direction(last_roll_tilt > 0.0f ? -1.0f : 1.0f);
					change_state(ramp_roll_state);
				} else {
					change_state(ramp_spin_state);
				}
			}
		}
	}

	// 4. Delegate to HSM
	if (current_state) {
		current_state->physics_update(p_delta);
	}

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

	was_on_ramp = is_on_ramp;
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
	const ActionState *input_state = player_input ? &player_input->get_state() : nullptr;

	if (game_manager && game_manager->is_active(this) && input_state) {
		current_input = input_state->vehicle;
	} else {
		current_input = {};
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
	float steer_angle = current_input.steering * max_steer_rad * steer_speed_factor;

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

	// How fast are we sliding sideways?
	float lateral_velocity = get_linear_velocity().dot(right_dir);

	// Grip is adjusted based on the is_drifting flag set by DriftingState
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

	bool is_steering = abs(current_input.steering) > 0.01f;
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

} // namespace godot
