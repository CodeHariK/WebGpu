#include "fly_state.h"
#include "../../game_manager/player_input.h"
#include "../camera.h"
#include <cmath>
#include <godot_cpp/classes/input.hpp>

namespace godot {

void CameraStateFly::enter(GameCamera *p_camera) {
	Input::get_singleton()->set_mouse_mode(Input::MOUSE_MODE_VISIBLE);
	// Seat the springs on wherever the camera currently is so entering fly mode
	// (or returning to it) never snaps.
	p_camera->rebase_springs();
}

void CameraStateFly::update(
		GameCamera *p_camera,
		float p_delta
) {
	if (p_camera->get_player_input()) {
		const ActionState &state = p_camera->get_player_input()->get_state();

		if (state.camera.is_orbiting) {
			if (state.camera.is_panning) {
				// Panning: translate the free-fly focus instead of rotating.
				Transform3D t = p_camera->get_global_transform();
				Vector3 right = t.basis.get_column(0);
				Vector3 up = t.basis.get_column(1);
				p_camera->pos_spring.target += right * (-state.camera.look_delta.x * p_camera->pan_speed) +
						up * (state.camera.look_delta.y * p_camera->pan_speed);
			} else {
				p_camera->yaw -= state.camera.look_delta.x * p_camera->orbit_sensitivity;
				p_camera->pitch -= state.camera.look_delta.y * p_camera->orbit_sensitivity;
				p_camera->pitch = CLAMP(p_camera->pitch, -Math::PI * 0.49f, Math::PI * 0.49f);
			}
		}

		if (std::abs(state.camera.zoom_delta) > 0.001f) {
			Transform3D t = p_camera->get_global_transform();
			Vector3 forward = -t.basis.get_column(2);
			p_camera->pos_spring.target += forward * (state.camera.zoom_delta * p_camera->zoom_speed);
		}
	}

	// Shared rotation + position smoothing (free-fly keeps its own moving focus in
	// pos_spring.target, so we just smooth toward it).
	p_camera->smooth_look_angles(p_delta);
	p_camera->apply_position(p_camera->pos_spring.target, p_delta);
}

} // namespace godot
