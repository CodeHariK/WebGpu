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
static const float PIVOT_RATE = 12.0f; // how fast the framing chases the character
static const float VELOCITY_RATE = 5.0f; // how fast the "travel direction" settles
static const float YAW_RECENTER_RATE = 1.8f; // Odyssey-ish swing-behind: present but never snappy
static const float PITCH_RETURN_RATE = 1.5f; // how fast pitch eases back to the resting framing
static const float RECENTER_SPEED = 2.5f; // only auto-centre above this horizontal speed (m/s)
static const float RECENTER_DELAY = 0.35f; // must be travelling this long before swinging behind
static const float RECENTER_SUSPEND = 1.5f; // pause auto-centre this long after a manual orbit
static const float LOOK_TARGET_HEIGHT = 1.0f; // aim toward the head so more ground ahead / below reads
static const float LOOK_AHEAD = 2.0f; // lead the framing along travel at speed

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
	RigidBody3D *rb = Object::cast_to<RigidBody3D>(p_camera->get_follow_target_node());
	Vector3 target_pivot = rb ? rb->get_global_position() : p_camera->get_global_position();
	Vector3 raw_velocity = rb ? rb->get_linear_velocity() : Vector3();

	if (first_frame) {
		smoothed_pivot = target_pivot;
		smoothed_velocity = raw_velocity;
		first_frame = false;
	} else {
		smoothed_pivot = smoothed_pivot.lerp(target_pivot, spring_damp_factor(PIVOT_RATE, p_delta));
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
	Vector3 pivot = smoothed_pivot + Vector3(0.0f, LOOK_TARGET_HEIGHT, 0.0f);
	if (h_speed > 0.1f) {
		float lead = LOOK_AHEAD * CLAMP(h_speed / p_camera->max_speed_for_zoom, 0.0f, 1.0f);
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

	// Soft swing-behind: ease the yaw round behind the direction of travel once the
	// player has committed to it. Idle (or orbiting / recently orbited) holds the view.
	if (!orbiting && recenter_suspend <= 0.0f && travel_time > RECENTER_DELAY) {
		float target_yaw = Math::atan2(-horizontal_vel.x, -horizontal_vel.z);
		float yaw_diff = UtilityFunctions::wrapf(target_yaw - p_camera->yaw, -Math::PI, Math::PI);
		p_camera->yaw += yaw_diff * spring_damp_factor(YAW_RECENTER_RATE, p_delta);
	}

	// Ease pitch back to the comfortable resting framing when hands-off.
	if (!orbiting) {
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
