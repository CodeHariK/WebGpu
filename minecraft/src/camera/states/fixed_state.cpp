#include "fixed_state.h"
#include "../camera.h"
#include <godot_cpp/classes/input.hpp>

namespace godot {

void CameraStateFixed::enter(GameCamera *p_camera) {
	// In Fixed mode, we don't need to capture the mouse since we aren't looking.
	Input::get_singleton()->set_mouse_mode(Input::MOUSE_MODE_VISIBLE);
}

void CameraStateFixed::update(GameCamera *p_camera, float p_delta) {
	// 1. Position Calculation
	Vector3 raw_pivot = (p_camera->get_follow_target_node()) ? p_camera->get_follow_target_node()->get_global_position() : p_camera->get_global_position();
	
	// A standard top-down offset if none is specified
	Vector3 offset = Vector3(0, 12, 12); 
	if (p_camera->get_follow_offset().length_squared() > 0.001f) {
		offset = p_camera->get_follow_offset();
	}

	Vector3 ideal_pos = raw_pivot + offset;

	// 2. Smooth Position Follow
	p_camera->pos_spring.target = ideal_pos;
	if (p_camera->is_pos_smoothing_enabled()) {
		p_camera->pos_spring.step(p_delta, p_camera->get_frequency(), p_camera->get_damping(), p_camera->response);
		p_camera->set_global_position(p_camera->pos_spring.current);
	} else {
		p_camera->set_global_position(ideal_pos);
	}

	// 3. Static Look At (Link's Awakening Style)
	// We look at the 'smoothed' pivot implied by our smoothed position
	Vector3 look_at_pivot = p_camera->pos_spring.current - offset;
	p_camera->look_at(look_at_pivot, Vector3(0, 1, 0));
}

} // namespace godot
