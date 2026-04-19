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
	Vector3 pivot = (p_camera->get_follow_target_node()) ? p_camera->get_follow_target_node()->get_global_position() : p_camera->get_global_position();
	
	// A standard top-down offset if none is specified
	Vector3 offset = Vector3(0, 12, 12); 
	if (p_camera->get_follow_offset().length_squared() > 0.001f) {
		offset = p_camera->get_follow_offset();
	}

	// 2. Smooth Position Follow
	p_camera->pos_spring.target = pivot + offset;
	p_camera->pos_spring.step(p_delta, p_camera->get_frequency(), p_camera->get_damping(), p_camera->response);
	
	p_camera->set_global_position(p_camera->pos_spring.current);

	// 3. Static Look At (Link's Awakening Style)
	// We ensure the camera is looking exactly at the target.
	// Since the offset is constant, this results in a fixed rotation.
	p_camera->look_at(pivot, Vector3(0, 1, 0));

	// Synchronize internal yaw/pitch for mode transitions
	Vector3 rot = p_camera->get_rotation();
	p_camera->yaw = rot.y;
	p_camera->pitch = rot.x;

	// Reset springs so they don't 'swing' if we switch back to TPS
	p_camera->yaw_spring.reset(p_camera->yaw);
	p_camera->pitch_spring.reset(p_camera->pitch);
}

} // namespace godot
