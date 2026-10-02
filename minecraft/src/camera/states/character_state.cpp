#include "character_state.h"
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
static const float VELOCITY_RATE = 6.0f; // how fast the "travel direction" settles
static const float YAW_RECENTER_RATE = 2.5f; // gentler than the car (4.0) to avoid feedback wobble
static const float PITCH_RETURN_RATE = 2.0f; // how fast pitch eases back to the resting framing
static const float RECENTER_SPEED = 2.0f; // only auto-centre above this horizontal speed (m/s)
static const float RECENTER_SUSPEND = 1.0f; // pause auto-centre this long after a manual orbit
static const float LOOK_TARGET_HEIGHT = 0.9f; // raise the aim toward the head (see more ahead / below)
static const float LOOK_AHEAD = 1.5f; // lead the framing along travel at speed
static const float HEADING_FOLLOW_RATE = 7.0f; // how fast the cam locks behind the steered heading

void CameraStateCharacter::enter(GameCamera *p_camera) {
	// Captured: in steer mode plain mouse motion steers the heading (handled by the
	// character), so the cursor must not wander. Esc frees it, left-click recaptures.
	Input::get_singleton()->set_mouse_mode(Input::MOUSE_MODE_CAPTURED);
	first_frame = true;
	recenter_suspend = 0.0f;
	p_camera->rebase_springs();
}

void CameraStateCharacter::update(
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

	// Aim a little above the feet and lead slightly along travel so platforms and
	// enemies ahead stay in frame.
	Vector3 pivot = smoothed_pivot + Vector3(0.0f, LOOK_TARGET_HEIGHT, 0.0f);
	if (h_speed > 0.1f) {
		float lead = LOOK_AHEAD * CLAMP(h_speed / p_camera->max_speed_for_zoom, 0.0f, 1.0f);
		pivot += (horizontal_vel / h_speed) * lead;
	}

	bool orbiting = false;
	if (p_camera->get_player_input()) {
		const ActionState &state = p_camera->get_player_input()->get_state();
		orbiting = state.camera.is_orbiting;

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

	if (!orbiting && p_camera->follow_heading) {
		// Steer mode: lock the camera behind the character's heading so it rotates
		// WITH the character (works even when turning in place).
		float yaw_diff = UtilityFunctions::wrapf(p_camera->heading_yaw - p_camera->yaw, -Math::PI, Math::PI);
		p_camera->yaw += yaw_diff * spring_damp_factor(HEADING_FOLLOW_RATE, p_delta);
	} else if (!orbiting && recenter_suspend <= 0.0f && h_speed > RECENTER_SPEED) {
		// Other schemes: gentle auto-centre behind travel. Idle holds the yaw.
		float target_yaw = Math::atan2(-horizontal_vel.x, -horizontal_vel.z);
		float yaw_diff = UtilityFunctions::wrapf(target_yaw - p_camera->yaw, -Math::PI, Math::PI);
		p_camera->yaw += yaw_diff * spring_damp_factor(YAW_RECENTER_RATE, p_delta);
	}

	// Ease pitch back to a comfortable downward framing when hands-off.
	if (!orbiting) {
		float h_dist = Vector2(p_camera->follow_offset.x, p_camera->follow_offset.z).length();
		float base_pitch = (h_dist > 0.01f) ? -Math::atan2(p_camera->follow_offset.y, h_dist) : p_camera->pitch;
		p_camera->pitch += (base_pitch - p_camera->pitch) * spring_damp_factor(PITCH_RETURN_RATE, p_delta);
	}

	p_camera->smooth_look_angles(p_delta);

	// Distance + collision pull-in, then final position. No speed-FOV / speed-lines.
	float desired = p_camera->get_current_target_distance();
	Vector3 ideal_full = p_camera->orbit_position(pivot, desired);
	float dist = p_camera->resolve_follow_distance(pivot, ideal_full, desired, p_delta);
	Vector3 ideal_pos = p_camera->orbit_position(pivot, dist);
	p_camera->apply_position(ideal_pos, p_delta);
}

} // namespace godot
