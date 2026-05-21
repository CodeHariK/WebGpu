#include "car_state.h"
#include "../../game_manager/player_input.h"
#include "../camera.h"
#include "godot_cpp/variant/utility_functions.hpp"
#include <godot_cpp/classes/input.hpp>
#include <godot_cpp/classes/rigid_body3d.hpp>

namespace godot {

void CameraStateCar::enter(GameCamera *p_camera) {
	Input::get_singleton()->set_mouse_mode(Input::MOUSE_MODE_VISIBLE);
	first_frame = true;
}

void CameraStateCar::update(GameCamera *p_camera, float p_delta) {
	RigidBody3D *rb = Object::cast_to<RigidBody3D>(p_camera->get_follow_target_node());

	Vector3 target_pivot = (rb) ? rb->get_global_position() : p_camera->get_global_position();
	Vector3 raw_velocity = (rb) ? rb->get_linear_velocity() : Vector3();

	if (first_frame) {
		smoothed_pivot = target_pivot;
		smoothed_velocity = raw_velocity;
		first_frame = false;
	} else {
		smoothed_pivot = smoothed_pivot.lerp(target_pivot, MIN(1.0f, p_delta * 10.0f));
		smoothed_velocity = smoothed_velocity.lerp(raw_velocity, MIN(1.0f, p_delta * 5.0f));
	}

	Vector3 pivot = smoothed_pivot;

	if (p_camera->get_player_input()) {
		const ActionState &state = p_camera->get_player_input()->get_state();

		if (state.camera.is_orbiting) {
			p_camera->yaw -= state.camera.look_delta.x * p_camera->orbit_sensitivity;
			p_camera->pitch -= state.camera.look_delta.y * p_camera->orbit_sensitivity;
			p_camera->pitch = CLAMP(p_camera->pitch, -Math_PI * 0.49f, Math_PI * 0.49f);
		}

		if (std::abs(state.camera.zoom_delta) > 0.001f) {
			p_camera->target_distance = CLAMP(p_camera->target_distance - (state.camera.zoom_delta * p_camera->zoom_speed), p_camera->min_distance, p_camera->max_distance);
		}

		// Car orientation tracking
		if (!state.camera.is_orbiting) {
			float target_yaw = 0.0f;

			if (rb) {
				Vector3 linear_vel = smoothed_velocity;
				Vector3 horizontal_vel = Vector3(linear_vel.x, 0, linear_vel.z);
				Vector3 target_forward = -rb->get_global_transform().basis.get_column(2).normalized();
				Vector3 local_up = rb->get_global_transform().basis.get_column(1).normalized();

				bool is_flipped = local_up.y < 0.5f; // Tilted more than ~60 degrees
				float speed_sq = horizontal_vel.length_squared();

				bool is_reversing = false;
				if (speed_sq > 0.01f) {
					Vector3 horizontal_forward = Vector3(target_forward.x, 0, target_forward.z);
					if (horizontal_forward.length_squared() > 0.001f) {
						is_reversing = horizontal_vel.normalized().dot(horizontal_forward.normalized()) < 0.0f;
					}
				}

				if (is_flipped) {
					// If the car is flipped/tilted wildly, hold the camera's current yaw to prevent spinning/jittering
					target_yaw = p_camera->yaw;
				} else if (speed_sq > 1.0f && !is_reversing) {
					// When upright and moving forward, follow the horizontal velocity vector (smooth drifting)
					target_yaw = Math::atan2(-horizontal_vel.x, -horizontal_vel.z);
				} else {
					// When upright and slow/stationary or reversing, follow the car's forward orientation
					float horizontal_forward_length = Vector2(target_forward.x, target_forward.z).length();
					if (horizontal_forward_length > 0.001f) {
						target_yaw = Math::atan2(-target_forward.x, -target_forward.z);
					} else {
						target_yaw = p_camera->yaw;
					}
				}
			} else {
				target_yaw = p_camera->yaw;
			}

			// Lerp p_camera->yaw towards target_yaw to avoid sudden snapping
			float yaw_diff_raw = UtilityFunctions::wrapf(target_yaw - p_camera->yaw, -Math_PI, Math_PI);
			p_camera->yaw += yaw_diff_raw * MIN(1.0f, p_delta * 4.0f);

			float h_dist = Vector2(p_camera->follow_offset.x, p_camera->follow_offset.z).length();
			float target_pitch = (h_dist > 0.01f) ? -Math::atan2(p_camera->follow_offset.y, h_dist) : p_camera->pitch;
			p_camera->pitch = target_pitch;
		}
	}

	// Shared Follow Logic (Orient + Collision + Springs)
	p_camera->yaw = UtilityFunctions::wrapf(p_camera->yaw, -Math_PI, Math_PI);

	float yaw_diff = UtilityFunctions::wrapf(p_camera->yaw - p_camera->yaw_spring.current, -Math_PI, Math_PI);
	p_camera->yaw_spring.target = p_camera->yaw_spring.current + yaw_diff;

	p_camera->pitch_spring.target = p_camera->pitch;
	p_camera->yaw_spring.step(p_delta, p_camera->get_frequency() * 2.0f, p_camera->get_damping(), p_camera->response);
	p_camera->pitch_spring.step(p_delta, p_camera->get_frequency() * 2.0f, p_camera->get_damping(), p_camera->response);

	p_camera->yaw_spring.current = UtilityFunctions::wrapf(p_camera->yaw_spring.current, -Math_PI, Math_PI);

	// Final Ideal Position Calculation
	Basis ideal_rot_basis = Basis::from_euler(Vector3(p_camera->pitch, p_camera->yaw, 0));
	Vector3 base_dir = p_camera->follow_offset.length_squared() > 0.001f ? p_camera->follow_offset.normalized() : Vector3(0, 0, 1);
	Vector3 ideal_pos = pivot + ideal_rot_basis.xform(base_dir * p_camera->get_current_target_distance());

	float actual_dist = p_camera->get_current_target_distance();
	if (p_camera->is_collision_enabled() && rb) {
		actual_dist = p_camera->_solve_collision(pivot, ideal_pos);
	}

	p_camera->dist_spring.target = actual_dist;
	if (actual_dist < p_camera->dist_spring.current) {
		// Snap instantly on collision to prevent wall clipping
		p_camera->dist_spring.current = actual_dist;
		p_camera->dist_spring.velocity = 0.0f;
	} else {
		// Smoothly recover distance when moving away from obstacles
		p_camera->dist_spring.step(p_delta, p_camera->get_frequency() * 1.5f, p_camera->get_damping(), p_camera->response);
	}

	Basis rot_basis = Basis::from_euler(Vector3(p_camera->pitch_spring.current, p_camera->yaw_spring.current, 0));
	Vector3 spring_target_pos = pivot + rot_basis.xform(base_dir * p_camera->dist_spring.current);

	// 6. Final Movement Smoothing
	p_camera->pos_spring.target = spring_target_pos;
	if (p_camera->is_pos_smoothing_enabled()) {
		p_camera->pos_spring.step(p_delta, p_camera->get_frequency(), p_camera->get_damping(), p_camera->response);
		p_camera->set_global_position(p_camera->pos_spring.current);
	} else {
		p_camera->set_global_position(spring_target_pos);
	}
	p_camera->set_rotation(Vector3(p_camera->pitch_spring.current, p_camera->yaw_spring.current, 0));

	// UtilityFunctions::print(
	// 		"CameraYaw ", p_camera->yaw,
	// 		" CameraPitch ", p_camera->pitch,
	// 		" CameraPitchSpring ", p_camera->pitch_spring.current,
	// 		" CameraYawSpring ", p_camera->yaw_spring.current,
	// 		" CameraPos ", p_camera->get_global_position(),
	// 		" CameraRot ", p_camera->get_global_rotation());
}

} // namespace godot
