#include "arcade_vehicle.h"
#include "../game_manager/game_manager.h"
#include "../game_manager/player_input.h"
#include "../utils/raycast/mc_raycast.h"
#include "ai/vehicle_states.h"
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/input.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/math.hpp>

namespace godot {

// ---------------------------------------------------------------------------
// Handling-feel constants. Named here (rather than inline magic numbers) so the
// force/torque scales that shape game feel are documented and easy to retune.
// ---------------------------------------------------------------------------
namespace {
constexpr float SUSPENSION_CAST_OFFSET = 0.3f; // ray start lifted above the hardpoint to avoid ground clipping
constexpr float LONGITUDINAL_FORCE_SCALE = 0.7f; // fraction of drive force routed through the pitch-offset point
constexpr float ENGINE_BRAKE_FORCE = 1000.0f; // passive deceleration force when coasting (no throttle)
constexpr float ACCEL_CURVE_MIN = 0.1f; // floor on the accel falloff curve near top speed
constexpr float BOOST_ACCEL_FORCE_MULT = 2.0f; // extra drive force while nitro boosting
constexpr float BOOST_NUDGE_STRENGTH = 0.6f; // arcade-assist nudge strength while boosting
constexpr float BASE_NUDGE_STRENGTH = 0.3f; // arcade-assist nudge strength normally
constexpr float BOOST_INITIAL_KICK_FRACTION = 0.2f; // instant forward kick on boost start (fraction of bonus)
constexpr float DRIFT_ALIGNMENT_SCALE = 0.15f; // velocity-alignment reduction while drifting (allows sliding)
constexpr float TRACTION_BREAK_ALIGN_SCALE = 0.3f; // velocity-alignment reduction when the tyres are sliding
constexpr float TURN_IN_BITE = 0.35f; // extra yaw kick from fast steering input (reactive turn-in)
constexpr float CORNER_SCRUB = 0.06f; // forward speed bled off per m/s of sideways slide (arcade weight)
constexpr float RAMP_NORMAL_Y_THRESHOLD = 0.9f; // avg ground-normal.y below this counts as a ramp
constexpr float GLIDE_MIN_UP_DOT = 0.7f; // min local-up.y required to start gliding
constexpr float MIN_TRICK_SPEED = 10.0f; // min forward speed to trigger glide / ramp tricks
constexpr float ROLL_TRICK_TILT_THRESHOLD = 0.4f; // |roll tilt| above this picks a barrel roll over a backflip
constexpr float WALL_NORMAL_UP_DOT = 0.6f; // hit-normal . up below this is treated as a wall, not ground
constexpr float WALL_PUSH_FORCE_SOFTEN = 0.5f; // soften wall push to avoid abrupt bounces
constexpr float WALL_PUSH_FORCE_CAP = 30.0f; // cap on wall push force (* mass)
constexpr float WALL_SPIN_TORQUE_FACTOR = 25.0f; // wall-scrape spin-assist torque (* mass)
constexpr float WALL_SPIN_MIN_SPEED = 3.0f; // min forward speed for wall-spin assist
constexpr float WALL_SPIN_SPEED_REF = 10.0f; // reference speed for scaling wall-spin assist
constexpr float MIN_SPEED_EPSILON = 0.001f; // guard against divide-by-zero on config speeds
} // namespace

float ArcadeVehicle::_calculate_suspension_force(
		Ref<WheelConfig> wheel,
		float hit_distance,
		Vector3 hardpoint_world,
		Vector3 local_up
) {
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

float ArcadeVehicle::get_wheel_displacement(int p_index) const {
	if (p_index < 0 || p_index >= (int)wheel_displacements.size()) {
		return 0.0f;
	}
	return wheel_displacements[p_index];
}

void ArcadeVehicle::_emit_mini_turbo() {
	if (config.is_null()) {
		return;
	}
	float boost = config->get_mini_turbo_boost();
	if (boost <= 0.0f) {
		return;
	}
	// Instant forward speed kick (added to velocity in _integrate_forces).
	Transform3D trans = get_global_transform();
	Vector3 forward_dir = -trans.basis.get_column(2).normalized();
	velocity_nudge_accumulator += forward_dir * boost;
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
	if ((int)wheel_displacements.size() != wconfigs.size()) {
		wheel_displacements.assign(wconfigs.size(), 0.0f);
	}
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
		float cast_offset = SUSPENSION_CAST_OFFSET;
		Vector3 cast_start = hardpoint_world + local_up * cast_offset;

		// 2. Raycast/Spherecast straight down
		float max_dist = wc->get_suspension_rest_length() + wc->get_radius() + 0.1f + cast_offset; // margin + offset

		// Use our new utility!
		TypedArray<RID> exclude;
		exclude.push_back(get_rid());

		MCRaycastHit hit =
				spherecast_3d(this, cast_start, local_down, max_dist, wc->get_radius() * 0.4f, 0xFFFFFFFF, exclude);

		CSGSphere3D *visual =
				(active_wheel_count < (int)wheel_visuals.size()) ? wheel_visuals[active_wheel_count] : nullptr;

		if (hit.is_hit) {
			// Project the hit position relative to the hardpoint along local_down
			float hit_dist = (hit.position - hardpoint_world).dot(local_down);

			// Compute force
			float force_mag = _calculate_suspension_force(wc, hit_dist, hardpoint_world, local_up);

			if (force_mag > 0.0f) {
				Vector3 force_dir;
				_handle_wall_collision_and_spin(wc, hit, force_dir, force_mag);
				apply_force(force_dir * force_mag, hardpoint_world - trans.origin);
			}

			// How far the wheel centre hangs below the hardpoint along the suspension
			// axis. For the VISUAL we let this go slightly negative (the wheel tucks
			// UP into the arch) so it always sits at the true ground-contact point and
			// never pokes through the ground on a bump or slope; droop is capped at the
			// rest length. The debug sphere keeps the old [0, rest] clamp.
			Vector3 target_pos = hit.position + hit.normal * wc->get_radius();
			float raw_disp = (target_pos - hardpoint_world).dot(local_down);
			wheel_displacements[active_wheel_count] =
					CLAMP(raw_disp, -0.4f, wc->get_suspension_rest_length());
			if (debug_visuals_enabled && visual) {
				float clamped = CLAMP(raw_disp, 0.0f, wc->get_suspension_rest_length());
				visual->set_global_position(hardpoint_world + local_down * clamped);
			}
			grounded_wheels++;
			avg_normal += hit.normal;
		} else {
			// Wheel is fully extended (in the air).
			wheel_displacements[active_wheel_count] = wc->get_suspension_rest_length();
			if (debug_visuals_enabled && visual) {
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
		if (avg_normal.y < RAMP_NORMAL_Y_THRESHOLD) {
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
		bool can_glide = local_up.y > GLIDE_MIN_UP_DOT && forward_speed > MIN_TRICK_SPEED &&
				current_state != ramp_roll_state && current_state != ramp_spin_state;

		if (is_active && current_input.glide && can_glide) {
			if (current_state != gliding_state) {
				change_state(gliding_state);
			}
		} else {
			if (current_state == gliding_state) {
				change_state(airborne_state);
			} else if (
					current_state != ramp_spin_state && current_state != ramp_roll_state &&
					current_state != airborne_state
			) {
				change_state(airborne_state);
			}

			if (was_on_ramp && current_state != ramp_spin_state && current_state != ramp_roll_state &&
				current_state != gliding_state && forward_speed > MIN_TRICK_SPEED) {
				if (Math::abs(last_roll_tilt) > ROLL_TRICK_TILT_THRESHOLD) {
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

	// Tuning tab: only the driven car's tab is shown in the shared panel.
	tuning.set_shown(is_active && debug_visuals_enabled);
	if (is_active) {
		tuning.push_graph(get_linear_velocity().length());
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
				// Handbrake drift: strongly loosen alignment so the car slides sideways.
				alignment_speed *= DRIFT_ALIGNMENT_SCALE;
			} else if (traction_broken) {
				// Grip broke in a hard/fast turn: loosen alignment so the slide shows
				// (emergent drift) instead of the velocity snapping back to the nose.
				alignment_speed *= TRACTION_BREAK_ALIGN_SCALE;
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

	if (Math::abs(input_drive) > 0.01f) {
		bool is_braking = (input_drive * current_forward_speed) < -0.1f;
		float force_limit = is_braking ? config->get_brake_decel() : config->get_max_accel_force();

		float accel_curve = 1.0f;
		if (is_boosting) {
			// Increase acceleration force during boost to push past normal speed quickly
			force_limit *= BOOST_ACCEL_FORCE_MULT;
			// Keep full acceleration force during boost (no drop off as we approach max speed)
			accel_curve = 1.0f;
		} else {
			float speed_ratio = Math::abs(current_forward_speed) / MAX(max_speed, MIN_SPEED_EPSILON);
			accel_curve = 1.0f - (speed_ratio * speed_ratio);
			accel_curve = MAX(ACCEL_CURVE_MIN, accel_curve);
		}

		accel_force = force_limit * accel_curve * input_drive;
	} else {
		if (Math::abs(current_forward_speed) > 0.1f) {
			// If boosting, preserve momentum and do not apply standard heavy engine brake
			if (!is_boosting) {
				accel_force = -ENGINE_BRAKE_FORCE * (current_forward_speed > 0.0f ? 1.0f : -1.0f);
			}
		}
	}

	_apply_longitudinal_force_with_pitch(forward_dir * accel_force * LONGITUDINAL_FORCE_SCALE);

	// Apply arcade assist nudge; make it stronger during Nitro boost to help reach top speed quickly
	float nudge_strength = is_boosting ? BOOST_NUDGE_STRENGTH : BASE_NUDGE_STRENGTH;
	velocity_nudge_accumulator += forward_dir * speed_diff * config->get_arcade_assist() * delta * nudge_strength;

	if (started_boosting) {
		// Initial speed kick forward to feel responsive and punchy
		velocity_nudge_accumulator += forward_dir * (boost_speed_bonus * BOOST_INITIAL_KICK_FRACTION);
	}
}

void ArcadeVehicle::_apply_steering(float delta) {
	Transform3D trans = get_global_transform();
	Vector3 forward_dir = -trans.basis.get_column(2).normalized();
	Vector3 up_dir = trans.basis.get_column(1).normalized();
	float forward_speed = get_linear_velocity().dot(forward_dir);

	float steer_input = CLAMP(current_input.steering, -1.0f, 1.0f);
	if (is_drifting) {
		// Drifting turns tighter, but keep the input bounded to [-1, 1].
		steer_input = CLAMP(steer_input * config->get_drift_steer_torque_multiplier(), -1.0f, 1.0f);
	}

	// Turn-radius model: the car CARVES an arc rather than pivoting on the spot.
	// At full steer the tightest path is config.turn_radius metres, and the yaw rate
	// needed to follow a circle of radius R at speed v is simply v / R. This is
	// zero at a standstill (no spinning without momentum), grows with speed, flips
	// sign in reverse, and is capped by config.max_yaw_rate so it can never whip
	// around. Positive steering (right) is a right turn = negative yaw about +Y.
	float desired_yaw_rate = 0.0f;
	if (Math::abs(steer_input) > 0.001f) {
		float turn_radius = MAX(config->get_turn_radius(), 0.5f);
		float radius = turn_radius / MAX(Math::abs(steer_input), 0.05f); // partial steer = wider arc
		desired_yaw_rate = -steer_input * (forward_speed / radius);
		float max_yaw = MAX(config->get_max_yaw_rate(), 0.05f);
		desired_yaw_rate = CLAMP(desired_yaw_rate, -max_yaw, max_yaw);
	}

	// Ease the actual yaw rate toward the target with a gentle first-order P
	// controller (config.turn_speed sets how fast it gets there — the "turn speed").
	// Controlling a rate is stable and monotonic, so the car never darts the
	// opposite way first. With no steering the target is zero, which also damps out
	// residual spin — no separate yaw-damping pass needed.
	float current_yaw_rate = get_angular_velocity().dot(up_dir);
	float yaw_error = desired_yaw_rate - current_yaw_rate;
	float yaw_torque = yaw_error * config->get_mass() * config->get_turn_speed();

	// Turn-in bite: a brief extra yaw kick the instant you flick the wheel (when the
	// steering input is growing in the same direction), so the nose reacts crisply
	// instead of easing in. Fades as the steering settles.
	float steer_rate = (steer_input - prev_steer) / MAX(delta, MIN_SPEED_EPSILON);
	if (steer_rate * steer_input > 0.0f) {
		yaw_torque += -steer_input * Math::abs(steer_rate) * config->get_mass() * TURN_IN_BITE;
	}
	prev_steer = steer_input;

	apply_torque(up_dir * yaw_torque);
}

void ArcadeVehicle::_apply_lateral_friction(float delta) {
	Transform3D trans = get_global_transform();
	Vector3 right_dir = trans.basis.get_column(0).normalized(); // Local X is right
	Vector3 forward_dir = -trans.basis.get_column(2).normalized();

	// How fast are we sliding sideways?
	float lateral_velocity = get_linear_velocity().dot(right_dir);
	float mass = config->get_mass();

	// The grip that WANTS to cancel the sideways slide this frame (as an accel).
	float desired_accel = -lateral_velocity / delta;

	// Traction limit: the tyres can only give so much sideways grip before they
	// break loose. Below the limit the car holds its line; above it (a too-fast /
	// too-sharp turn, or the handbrake e-brake) the excess slide is NOT cancelled,
	// so the car drifts. The handbrake lowers the limit so the tail steps out.
	float traction = is_drifting ? config->get_drift_lateral_accel() : config->get_grip_lateral_accel();
	float applied_accel = CLAMP(desired_accel, -traction, traction);

	// Flag a broken-traction frame (used to loosen velocity alignment so the slide
	// is actually visible, and to trigger the drift smoke).
	traction_broken = Math::abs(desired_accel) > traction * 1.05f;

	// F = m * a, applied at the roll point so weight transfers into the turn.
	_apply_lateral_force_with_roll(right_dir * (applied_accel * mass));

	// Corner scrub: bleed a little forward speed while genuinely sliding, for arcade
	// weight (a hard slide costs you speed).
	if (traction_broken) {
		float scrub = Math::abs(lateral_velocity) * CORNER_SCRUB * mass;
		float fwd_speed = get_linear_velocity().dot(forward_dir);
		float dir = (fwd_speed >= 0.0f) ? -1.0f : 1.0f;
		apply_central_force(forward_dir * scrub * dir);
	}
}

void ArcadeVehicle::_apply_stability(float delta) {
	// --- 1. DOWNFORCE ---
	// Artificial gravity to keep the car from bouncing around like a beach ball
	float downforce_mag = config->get_downforce();
	apply_central_force(Vector3(0, -1, 0) * downforce_mag);

	// --- 2. YAW ---
	// Yaw damping is now handled inside _apply_steering: its target-yaw-rate
	// controller drives the yaw rate to zero whenever there is no steering input,
	// which is exactly the "locked-in" behaviour the old damping pass provided —
	// without a second torque source fighting the steering controller.

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

void ArcadeVehicle::_handle_wall_collision_and_spin(
		const Ref<WheelConfig> &p_wheel,
		const MCRaycastHit &p_hit,
		Vector3 &r_force_dir,
		float &r_force_mag
) {
	Transform3D trans = get_global_transform();
	Vector3 local_up = trans.basis.get_column(1).normalized();

	float up_dot = p_hit.normal.dot(local_up);
	r_force_dir = local_up;

	if (up_dot < WALL_NORMAL_UP_DOT) {
		// Wall/slope collision: push away from the wall horizontally
		r_force_dir = p_hit.normal;
		r_force_mag *= WALL_PUSH_FORCE_SOFTEN; // Soften the force to avoid abrupt bounces
		r_force_mag =
				MIN(r_force_mag,
					config->get_mass() * WALL_PUSH_FORCE_CAP); // Cap the force to keep it smooth and stable

		// Spin assist: apply horizontal torque when front wheels scrape a wall
		Vector3 forward_dir = -trans.basis.get_column(2).normalized();
		float forward_speed = get_linear_velocity().dot(forward_dir);
		if (forward_speed > WALL_SPIN_MIN_SPEED) {
			float speed_scale = CLAMP(forward_speed / WALL_SPIN_SPEED_REF, 0.5f, 1.5f);
			float assist_torque = config->get_mass() * WALL_SPIN_TORQUE_FACTOR * speed_scale;
			// Only front wheels drive the wall-scrape spin. Forward is local -Z, so a
			// front wheel has hardpoint.z < 0; right is local +X. Front-left scrapes
			// spin clockwise (+yaw), front-right counter-clockwise (-yaw). Deriving this
			// from the hardpoint position keeps it correct regardless of wheel ordering.
			Vector3 hardpoint = p_wheel.is_valid() ? p_wheel->get_hardpoint_offset() : Vector3();
			bool is_front = hardpoint.z < 0.0f;
			if (is_front && Math::abs(hardpoint.x) > 0.01f) {
				float spin_sign = (hardpoint.x < 0.0f) ? 1.0f : -1.0f;
				apply_torque(local_up * assist_torque * spin_sign);
			}
		}
	}
}

} // namespace godot
