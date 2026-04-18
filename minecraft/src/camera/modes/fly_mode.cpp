#include "fly_mode.h"
#include "../camera.h"
#include "../../game_manager/player_input.h"
#include <godot_cpp/classes/input.hpp>

namespace godot {

void MCFlyCameraMode::enter(MCCamera *p_camera) {
	Input::get_singleton()->set_mouse_mode(Input::MOUSE_MODE_VISIBLE);
}

void MCFlyCameraMode::update(MCCamera *p_camera, float p_delta) {
	if (p_camera->player_input) {
		const ActionState &state = p_camera->player_input->get_state();
		
		if (state.camera.is_orbiting) {
			if (state.camera.is_panning) {
				// Panning logic: translate instead of rotate
				Transform3D t = p_camera->get_global_transform();
				Vector3 right = t.basis.get_column(0);
				Vector3 up = t.basis.get_column(1);
				p_camera->pos_spring.target += right * (-state.camera.look_delta.x * p_camera->pan_speed) + up * (state.camera.look_delta.y * p_camera->pan_speed);
			} else {
				p_camera->yaw -= state.camera.look_delta.x * p_camera->orbit_sensitivity;
				p_camera->pitch -= state.camera.look_delta.y * p_camera->orbit_sensitivity;
				p_camera->pitch = CLAMP(p_camera->pitch, -Math_PI * 0.49f, Math_PI * 0.49f);
			}
		}

		if (std::abs(state.camera.zoom_delta) > 0.001f) {
			Transform3D t = p_camera->get_global_transform();
			Vector3 forward = -t.basis.get_column(2);
			p_camera->pos_spring.target += forward * (state.camera.zoom_delta * p_camera->zoom_speed);
		}
	}

	// Free look physics update
	p_camera->yaw_spring.target = p_camera->yaw;
	p_camera->pitch_spring.target = p_camera->pitch;

	p_camera->yaw_spring.step(p_delta, p_camera->frequency * 2.0f, p_camera->damping, p_camera->response);
	p_camera->pitch_spring.step(p_delta, p_camera->frequency * 2.0f, p_camera->damping, p_camera->response);

	// Position physics update
	Vector3 ideal_pos = p_camera->_calculate_ideal_position();
	p_camera->pos_spring.target = ideal_pos;
	p_camera->pos_spring.step(p_delta, p_camera->frequency, p_camera->damping, p_camera->response);

	p_camera->set_global_position(p_camera->pos_spring.current);
	p_camera->set_rotation(Vector3(p_camera->pitch_spring.current, p_camera->yaw_spring.current, 0));
}

} // namespace godot
