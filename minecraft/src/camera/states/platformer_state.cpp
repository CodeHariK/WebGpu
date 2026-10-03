#include "platformer_state.h"
#include "../../game_manager/player_input.h"
#include "../../utils/spring/stateful_spring.h"
#include "../camera.h"
#include <cmath>
#include <godot_cpp/classes/input.hpp>
#include <godot_cpp/classes/rigid_body3d.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/variant/vector2.hpp>

namespace godot {

// Frame-rate-independent smoothing rates (e-folds per second).
static const float PIVOT_RATE = 12.0f; // how fast the framing chases the character across the ground (x, z)
static const float VELOCITY_RATE = 5.0f; // how fast the "travel direction" settles
static const float PITCH_RETURN_RATE = 0.6f; // gentle: pitch drifts back to the resting framing while running
static const float RECENTER_SPEED = 2.5f; // only auto-centre above this horizontal speed (m/s)
static const float AWAY_MIN = 0.35f; // need a real 'away from camera' component; sideways/toward never swing
static const float AWAY_FULL = 0.85f; // travel this far away from the camera: full swing-behind rate
static const float RECENTER_SUSPEND = 1.5f; // pause auto-centre this long after a manual orbit
static const float LOOK_TARGET_HEIGHT = 1.0f; // aim toward the head so more ground ahead / below reads

void CameraStatePlatformer::enter(GameCamera *p_camera) {
	// Captured mouse: plain mouse motion orbits with no button held (a closer stand-in
	// for a mobile thumb-drag). Esc frees the cursor, left-click recaptures (GameManager).
	Input::get_singleton()->set_mouse_mode(Input::MOUSE_MODE_CAPTURED);
	first_frame = true;
	recenter_suspend = 0.0f;
	travel_time = 0.0f;
	// This camera is free: never lock behind a steered heading.
	p_camera->set_heading_follow(false, 0.0f);
	p_camera->rebase_springs();
}

void CameraStatePlatformer::update(
		GameCamera *p_camera,
		float p_delta
) {
	// Any body type: RigidBody3D (SpringCharacter) or CharacterBody3D (CelesteController).
	Node3D *target = p_camera->get_follow_target_node();
	Vector3 target_pivot = target ? target->get_global_position() : p_camera->get_global_position();
	Vector3 raw_velocity = p_camera->follow_target_velocity();

	if (first_frame) {
		smoothed_pivot = Vector3(target_pivot.x, 0.0f, target_pivot.z);
		smoothed_velocity = raw_velocity;
		vertical_hold.reset(target_pivot.y);
		first_frame = false;
	} else {
		Vector3 target_flat(target_pivot.x, 0.0f, target_pivot.z);
		smoothed_pivot = smoothed_pivot.lerp(target_flat, spring_damp_factor(PIVOT_RATE, p_delta));
		smoothed_velocity = smoothed_velocity.lerp(raw_velocity, spring_damp_factor(VELOCITY_RATE, p_delta));
	}

	Vector3 horizontal_vel(smoothed_velocity.x, 0.0f, smoothed_velocity.z);
	float h_speed = horizontal_vel.length();

	// Track how long we've been travelling: Odyssey only swings behind you once you
	// commit to a direction, not on the first step, so quick turns don't yank the view.
	if (h_speed > RECENTER_SPEED) {
		travel_time += p_delta;
	} else {
		travel_time = 0.0f;
	}

	// Aim above the feet and lead along travel so what is ahead stays in frame.
	// Ground plane from the smoothed position; height from the hold (it doesn't follow jumps).
	Vector3 pivot = smoothed_pivot;
	pivot.y = vertical_hold.update(
			target_pivot,
			raw_velocity,
			CameraVerticalHold::target_grounded(target),
			p_camera,
			p_camera->frame_top,
			p_camera->frame_bottom,
			p_camera->frame_soft_zone,
			p_delta
	);
	pivot.y += LOOK_TARGET_HEIGHT;
	if (h_speed > 0.1f) {
		float lead = p_camera->platformer_look_ahead * CLAMP(h_speed / p_camera->max_speed_for_zoom, 0.0f, 1.0f);
		pivot += (horizontal_vel / h_speed) * lead;
	}

	// Manual orbit always wins and suspends the auto-centre. With the mouse captured
	// any look motion counts as orbiting; a drag button still works when the cursor is free.
	bool orbiting = false;
	if (p_camera->get_player_input()) {
		const ActionState &state = p_camera->get_player_input()->get_state();
		bool captured = Input::get_singleton()->get_mouse_mode() == Input::MOUSE_MODE_CAPTURED;
		bool moved = state.camera.look_delta.length_squared() > 0.0001f;
		orbiting = state.camera.is_orbiting || (captured && moved);
		if (orbiting) {
			p_camera->yaw -= state.camera.look_delta.x * p_camera->orbit_sensitivity;
			p_camera->pitch -= state.camera.look_delta.y * p_camera->orbit_sensitivity;
			p_camera->pitch = CLAMP(p_camera->pitch, -Math::PI * 0.49f, Math::PI * 0.49f);
			recenter_suspend = RECENTER_SUSPEND;
		}
		if (std::abs(state.camera.zoom_delta) > 0.001f) {
			p_camera->target_distance =
					CLAMP(p_camera->target_distance - (state.camera.zoom_delta * p_camera->zoom_speed),
						  p_camera->min_distance, p_camera->max_distance);
		}
	}
	if (recenter_suspend > 0.0f) {
		recenter_suspend -= p_delta;
	}

	// Auto-recentre (yaw swing + pitch return) only once the player has committed to running,
	// hasn't just looked around, and isn't aiming: standing still, the view stays exactly where
	// it was left (Odyssey / A Hat in Time). Aiming must never have the camera drag the aim.
	bool aiming = target && target->has_method("is_aiming") && bool(target->call("is_aiming"));
	bool auto_recenter = !orbiting && !aiming && recenter_suspend <= 0.0f &&
			travel_time > p_camera->platformer_recenter_delay;

	// Soft swing-behind: ease the yaw round behind the direction of travel once the
	// player has committed to it. Idle (or orbiting / recently orbited) holds the view.
	//
	// Only swing when travel points AWAY from the camera (or sideways). Running TOWARD the
	// camera must never trigger it: movement is camera-relative, so swinging round to get
	// behind a character running at you rotates "toward the camera" with it, which turns
	// the character, which turns the camera... a feedback loop that runs you in circles.
	// Odyssey does the same: run at the camera and it just lets you come to it.
	if (auto_recenter) {
		Vector3 travel_dir = horizontal_vel / h_speed; // h_speed > RECENTER_SPEED here
		Vector3 cam_fwd(-Math::sin(p_camera->yaw), 0.0f, -Math::cos(p_camera->yaw)); // ground-plane look dir
		float away = travel_dir.dot(cam_fwd); // +1 away from camera, 0 sideways, -1 toward it
		// 0 when heading toward the camera, ramping to full strength when heading away.
		float t = CLAMP((away - AWAY_MIN) / (AWAY_FULL - AWAY_MIN), 0.0f, 1.0f);
		float weight = t * t * (3.0f - 2.0f * t); // smoothstep: no hard edge in the rate
		if (weight > 0.0f) {
			float target_yaw = Math::atan2(-horizontal_vel.x, -horizontal_vel.z);
			float yaw_diff = UtilityFunctions::wrapf(target_yaw - p_camera->yaw, -Math::PI, Math::PI);
			p_camera->yaw += yaw_diff * spring_damp_factor(p_camera->platformer_recenter_rate * weight, p_delta);
		}
	}

	// Ease pitch back to the resting framing, under the same rule as the yaw swing.
	if (auto_recenter) {
		float h_dist = Vector2(p_camera->follow_offset.x, p_camera->follow_offset.z).length();
		float base_pitch = (h_dist > 0.01f) ? -Math::atan2(p_camera->follow_offset.y, h_dist) : p_camera->pitch;
		p_camera->pitch += (base_pitch - p_camera->pitch) * spring_damp_factor(PITCH_RETURN_RATE, p_delta);
	}

	p_camera->smooth_look_angles(p_delta);

	// Distance + collision pull-in, then the final position.
	float desired = p_camera->get_current_target_distance();
	Vector3 ideal_full = p_camera->orbit_position(pivot, desired);
	float dist = p_camera->resolve_follow_distance(pivot, ideal_full, desired, p_delta);
	Vector3 ideal_pos = p_camera->orbit_position(pivot, dist);
	p_camera->apply_position(ideal_pos, p_delta);
}

} // namespace godot
