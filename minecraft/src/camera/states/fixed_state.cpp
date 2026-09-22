#include "fixed_state.h"
#include "../camera.h"
#include <godot_cpp/classes/input.hpp>

namespace godot {

void CameraStateFixed::enter(GameCamera *p_camera) {
	// Fixed mode never rotates the view, so the mouse stays free.
	Input::get_singleton()->set_mouse_mode(Input::MOUSE_MODE_VISIBLE);
	first_frame = true;
	p_camera->rebase_springs();
}

void CameraStateFixed::update(
		GameCamera *p_camera,
		float p_delta
) {
	Vector3 raw_pivot = (p_camera->get_follow_target_node()) ? p_camera->get_follow_target_node()->get_global_position()
															 : p_camera->get_global_position();

	// A standard top-down offset if none is specified.
	Vector3 offset = Vector3(0, 12, 12);
	if (p_camera->get_follow_offset().length_squared() > 0.001f) {
		offset = p_camera->get_follow_offset();
	}

	if (first_frame) {
		framed_pivot = raw_pivot;
		first_frame = false;
	}

	// Dead-zone follow (Link's Awakening style): the framed point only chases the
	// target once it leaves a dead-zone radius on the ground plane, so small
	// wander doesn't nudge the camera. Vertical position always follows.
	float deadzone = p_camera->get_fixed_deadzone();
	if (deadzone > 0.0f) {
		Vector3 flat_delta = Vector3(raw_pivot.x - framed_pivot.x, 0.0f, raw_pivot.z - framed_pivot.z);
		float dist = flat_delta.length();
		if (dist > deadzone) {
			framed_pivot += (flat_delta / dist) * (dist - deadzone);
		}
		framed_pivot.y = raw_pivot.y;
	} else {
		framed_pivot = raw_pivot;
	}

	Vector3 ideal_pos = framed_pivot + offset;

	// Smooth position follow.
	p_camera->pos_spring.target = ideal_pos;
	if (p_camera->is_pos_smoothing_enabled()) {
		p_camera->pos_spring.step(p_delta, p_camera->get_frequency(), p_camera->get_damping(), p_camera->response);
		p_camera->set_global_position(p_camera->pos_spring.current);
	} else {
		p_camera->set_global_position(ideal_pos);
	}

	// Look at the framed point implied by our (smoothed) position.
	Vector3 look_at_pivot = p_camera->pos_spring.current - offset;
	p_camera->look_at(look_at_pivot, Vector3(0, 1, 0));
}

} // namespace godot
