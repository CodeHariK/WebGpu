#include "tps_state.h"
#include "../../game_manager/player_input.h"
#include "../camera.h"
#include <godot_cpp/classes/input.hpp>

namespace godot {

void CameraStateTPS::enter(GameCamera *p_camera) {
	Input::get_singleton()->set_mouse_mode(Input::MOUSE_MODE_CAPTURED);
}

void CameraStateTPS::update(GameCamera *p_camera, float p_delta) {
	if (p_camera->get_player_input()) {
		const ActionState &state = p_camera->get_player_input()->get_state();

		// 1. Update Rotation (TPS captures mouse, so we always look)
		p_camera->yaw -= state.camera.look_delta.x * p_camera->orbit_sensitivity;
		p_camera->pitch -= state.camera.look_delta.y * p_camera->orbit_sensitivity;
		p_camera->pitch = CLAMP(p_camera->pitch, -Math_PI * 0.45f, Math_PI * 0.45f);

		// 2. Zoom handling
		if (std::abs(state.camera.zoom_delta) > 0.001f) {
			p_camera->target_distance = CLAMP(p_camera->target_distance - (state.camera.zoom_delta * p_camera->zoom_speed), p_camera->min_distance, p_camera->max_distance);
		}
	}

	// 3. Spring Smoothing for Rotation
	p_camera->yaw_spring.target = p_camera->yaw;
	p_camera->pitch_spring.target = p_camera->pitch;
	p_camera->yaw_spring.step(p_delta, p_camera->get_frequency() * 2.0f, p_camera->get_damping(), p_camera->response);
	p_camera->pitch_spring.step(p_delta, p_camera->get_frequency() * 2.0f, p_camera->get_damping(), p_camera->response);

	// 4. Position Calculation
	Vector3 pivot = (p_camera->get_follow_target_node()) ? p_camera->get_follow_target_node()->get_global_position() : p_camera->get_global_position();
	
	// Apply rotation to the offset
	Basis rot_basis = Basis::from_euler(Vector3(p_camera->pitch_spring.current, p_camera->yaw_spring.current, 0));
	
	// Default Offset if none specified: slightly right and back
	Vector3 local_offset = p_camera->follow_offset.length_squared() > 0.001f ? p_camera->follow_offset : Vector3(0.5f, 1.5f, 3.0f);
	Vector3 target_pos = pivot + rot_basis.xform(local_offset);

	// 5. Collision Solving
	float dist_multiplier = 1.0f;
	if (p_camera->is_collision_enabled() && p_camera->get_follow_target_node()) {
		// We solve collision relative to the pivot
		float actual_dist = p_camera->_solve_collision(pivot, target_pos);
		dist_multiplier = actual_dist / local_offset.length();
	}
	
	// Smooth the distance factor
	p_camera->dist_spring.target = dist_multiplier;
	p_camera->dist_spring.step(p_delta, p_camera->get_frequency() * 1.5f, p_camera->get_damping(), p_camera->response);

	// Final Position
	Vector3 final_offset = rot_basis.xform(local_offset * p_camera->dist_spring.current);
	
	p_camera->set_global_position(pivot + final_offset);
	p_camera->set_rotation(Vector3(p_camera->pitch_spring.current, p_camera->yaw_spring.current, 0));
}

} // namespace godot
