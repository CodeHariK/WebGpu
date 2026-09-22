#include "tps_state.h"
#include "../../game_manager/player_input.h"
#include "../camera.h"
#include <cmath>
#include <godot_cpp/classes/input.hpp>

namespace godot {

void CameraStateTPS::enter(GameCamera *p_camera) {
	Input::get_singleton()->set_mouse_mode(Input::MOUSE_MODE_CAPTURED);
	p_camera->rebase_springs();
}

void CameraStateTPS::update(
		GameCamera *p_camera,
		float p_delta
) {
	if (p_camera->get_player_input()) {
		const ActionState &state = p_camera->get_player_input()->get_state();

		// Mouse is captured in TPS, so we always steer the look.
		p_camera->yaw -= state.camera.look_delta.x * p_camera->orbit_sensitivity;
		p_camera->pitch -= state.camera.look_delta.y * p_camera->orbit_sensitivity;
		p_camera->pitch = CLAMP(p_camera->pitch, -Math::PI * 0.45f, Math::PI * 0.45f);

		if (std::abs(state.camera.zoom_delta) > 0.001f) {
			p_camera->target_distance =
					CLAMP(p_camera->target_distance - (state.camera.zoom_delta * p_camera->zoom_speed),
						  p_camera->min_distance, p_camera->max_distance);
		}
	}

	// Shared rotation smoothing.
	p_camera->smooth_look_angles(p_delta);

	Vector3 pivot = (p_camera->get_follow_target_node()) ? p_camera->get_follow_target_node()->get_global_position()
														 : p_camera->get_global_position();

	// The shoulder/back framing lives in follow_offset: its direction is the
	// orbit direction and its length is the resting distance. Collision now works
	// in absolute metres (same units as Car), so switching TPS<->Car no longer
	// jumps.
	float desired = p_camera->target_distance;
	Vector3 ideal_full = p_camera->orbit_position(pivot, desired);
	float dist = p_camera->resolve_follow_distance(pivot, ideal_full, desired, p_delta);
	Vector3 ideal_pos = p_camera->orbit_position(pivot, dist);

	p_camera->apply_position(ideal_pos, p_delta);
}

} // namespace godot
