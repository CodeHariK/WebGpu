#include "car_state.h"
#include "../../game_manager/player_input.h"
#include "../camera.h"
#include <godot_cpp/classes/input.hpp>
#include <godot_cpp/classes/rigid_body3d.hpp>

namespace godot {

void CameraStateCar::enter(GameCamera *p_camera) {
	Input::get_singleton()->set_mouse_mode(Input::MOUSE_MODE_VISIBLE);
}

void CameraStateCar::update(GameCamera *p_camera, float p_delta) {
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
			RigidBody3D *rb = Object::cast_to<RigidBody3D>(p_camera->get_follow_target_node());
			
			if (rb) {
				Vector3 linear_vel = rb->get_linear_velocity();
				Vector3 horizontal_vel = Vector3(linear_vel.x, 0, linear_vel.z);
				Vector3 target_forward = -p_camera->get_follow_target_node()->get_global_transform().basis.get_column(2).normalized();
				float forward_dot_vel = (horizontal_vel.length_squared() > 0.01f) ? horizontal_vel.normalized().dot(target_forward) : 1.0f;

				if (horizontal_vel.length_squared() > 1.0f && forward_dot_vel > 0.0f) {
					target_yaw = Math::atan2(-horizontal_vel.x, -horizontal_vel.z);
				} else {
					target_yaw = p_camera->get_follow_target_node()->get_global_rotation().y;
				}
			} else {
				target_yaw = (p_camera->get_follow_target_node()) ? p_camera->get_follow_target_node()->get_global_rotation().y : p_camera->yaw;
			}

			float h_dist = Vector2(p_camera->follow_offset.x, p_camera->follow_offset.z).length();
			float target_pitch = (h_dist > 0.01f) ? -Math::atan2(p_camera->follow_offset.y, h_dist) : p_camera->pitch;

			p_camera->yaw = target_yaw;
			p_camera->pitch = target_pitch;
		}
	}

	// Shared Follow Logic (Orient + Collision + Springs)
	p_camera->yaw_spring.target = p_camera->yaw;
	p_camera->pitch_spring.target = p_camera->pitch;
	p_camera->yaw_spring.step(p_delta, p_camera->get_frequency() * 2.0f, p_camera->get_damping(), p_camera->response);
	p_camera->pitch_spring.step(p_delta, p_camera->get_frequency() * 2.0f, p_camera->get_damping(), p_camera->response);

	// Final Ideal Position Calculation
	Vector3 pivot = (p_camera->get_follow_target_node()) ? p_camera->get_follow_target_node()->get_global_position() : p_camera->get_global_position();
	Vector3 ideal_pos = p_camera->_calculate_ideal_position();

	float actual_dist = p_camera->target_distance;
	if (p_camera->is_collision_enabled() && p_camera->get_follow_target_node()) {
		actual_dist = p_camera->_solve_collision(pivot, ideal_pos);
	}

	p_camera->dist_spring.target = actual_dist;
	p_camera->dist_spring.step(p_delta, p_camera->get_frequency() * 1.5f, p_camera->get_damping(), p_camera->response);

	Basis rot_basis = Basis::from_euler(Vector3(p_camera->pitch_spring.current, p_camera->yaw_spring.current, 0));
	Vector3 base_dir = p_camera->follow_offset.length_squared() > 0.001f ? p_camera->follow_offset.normalized() : Vector3(0, 0, 1);
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
}

} // namespace godot
