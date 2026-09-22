#include "car_state.h"
#include "../../game_manager/player_input.h"
#include "../camera.h"
#include "../../utils/spring/spring_dynamics.h"
#include <cmath>
#include <godot_cpp/classes/input.hpp>
#include <godot_cpp/classes/rigid_body3d.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

// Frame-rate-independent smoothing rates (e-folds per second).
static const float PIVOT_RATE = 10.0f; // how fast the framing chases the car
static const float VELOCITY_RATE = 5.0f; // how fast the "travel direction" settles
static const float YAW_FOLLOW_RATE = 4.0f; // how fast yaw re-centres behind the car

void CameraStateCar::enter(GameCamera *p_camera) {
	Input::get_singleton()->set_mouse_mode(Input::MOUSE_MODE_VISIBLE);
	first_frame = true;
	p_camera->rebase_springs();
}

void CameraStateCar::update(
		GameCamera *p_camera,
		float p_delta
) {
	RigidBody3D *rb = Object::cast_to<RigidBody3D>(p_camera->get_follow_target_node());

	Vector3 target_pivot = (rb) ? rb->get_global_position() : p_camera->get_global_position();
	Vector3 raw_velocity = (rb) ? rb->get_linear_velocity() : Vector3();

	if (first_frame) {
		smoothed_pivot = target_pivot;
		smoothed_velocity = raw_velocity;
		first_frame = false;
	} else {
		smoothed_pivot = smoothed_pivot.lerp(target_pivot, spring_damp_factor(PIVOT_RATE, p_delta));
		smoothed_velocity = smoothed_velocity.lerp(raw_velocity, spring_damp_factor(VELOCITY_RATE, p_delta));
	}

	Vector3 horizontal_vel = Vector3(smoothed_velocity.x, 0.0f, smoothed_velocity.z);
	float h_speed = horizontal_vel.length();

	// Look-ahead: lead the framing along the travel direction, scaled by speed,
	// so the player sees more of where they are going.
	Vector3 pivot = smoothed_pivot;
	if (p_camera->car_look_ahead > 0.0f && h_speed > 0.1f) {
		float lead = p_camera->car_look_ahead * CLAMP(h_speed / p_camera->max_speed_for_zoom, 0.0f, 1.0f);
		pivot += (horizontal_vel / h_speed) * lead;
	}

	if (p_camera->get_player_input()) {
		const ActionState &state = p_camera->get_player_input()->get_state();

		if (state.camera.is_orbiting) {
			p_camera->yaw -= state.camera.look_delta.x * p_camera->orbit_sensitivity;
			p_camera->pitch -= state.camera.look_delta.y * p_camera->orbit_sensitivity;
			p_camera->pitch = CLAMP(p_camera->pitch, -Math::PI * 0.49f, Math::PI * 0.49f);
		}

		if (std::abs(state.camera.zoom_delta) > 0.001f) {
			p_camera->target_distance =
					CLAMP(p_camera->target_distance - (state.camera.zoom_delta * p_camera->zoom_speed),
						  p_camera->min_distance, p_camera->max_distance);
		}

		// Auto re-centre behind the car when the player is not manually orbiting.
		if (!state.camera.is_orbiting) {
			float target_yaw = p_camera->yaw;

			if (rb) {
				Vector3 target_forward = -rb->get_global_transform().basis.get_column(2).normalized();
				Vector3 local_up = rb->get_global_transform().basis.get_column(1).normalized();

				bool is_flipped = local_up.y < 0.5f; // tilted past ~60 degrees
				float speed_sq = horizontal_vel.length_squared();

				bool is_reversing = false;
				if (speed_sq > 0.01f) {
					Vector3 horizontal_forward = Vector3(target_forward.x, 0, target_forward.z);
					if (horizontal_forward.length_squared() > 0.001f) {
						is_reversing = horizontal_vel.normalized().dot(horizontal_forward.normalized()) < 0.0f;
					}
				}

				if (is_flipped) {
					// Hold current yaw while the car is flipped to avoid a spin.
					target_yaw = p_camera->yaw;
				} else if (speed_sq > 1.0f && !is_reversing) {
					// Upright and moving forward: chase the velocity vector.
					target_yaw = Math::atan2(-horizontal_vel.x, -horizontal_vel.z);
				} else {
					// Slow / reversing: fall back to the car's facing.
					float horizontal_forward_length = Vector2(target_forward.x, target_forward.z).length();
					if (horizontal_forward_length > 0.001f) {
						target_yaw = Math::atan2(-target_forward.x, -target_forward.z);
					}
				}
			}

			// Ease yaw toward the target along the shortest arc (frame-rate independent).
			float yaw_diff = UtilityFunctions::wrapf(target_yaw - p_camera->yaw, -Math::PI, Math::PI);
			p_camera->yaw += yaw_diff * spring_damp_factor(YAW_FOLLOW_RATE, p_delta);

			// Pitch from the follow offset, flattened a touch at speed for a
			// stronger sense of velocity.
			float h_dist = Vector2(p_camera->follow_offset.x, p_camera->follow_offset.z).length();
			float base_pitch = (h_dist > 0.01f) ? -Math::atan2(p_camera->follow_offset.y, h_dist) : p_camera->pitch;
			float speed_frac = CLAMP(h_speed / p_camera->max_speed_for_zoom, 0.0f, 1.0f);
			p_camera->pitch = base_pitch * (1.0f - p_camera->car_speed_pitch_flatten * speed_frac);
		}
	}

	// Shared rotation smoothing.
	p_camera->smooth_look_angles(p_delta);

	// Distance (with dynamic zoom) + collision, then final position.
	float desired = p_camera->get_current_target_distance();
	Vector3 ideal_full = p_camera->orbit_position(pivot, desired);
	float dist = p_camera->resolve_follow_distance(pivot, ideal_full, desired, p_delta);
	Vector3 ideal_pos = p_camera->orbit_position(pivot, dist);

	p_camera->apply_position(ideal_pos, p_delta);
}

} // namespace godot
