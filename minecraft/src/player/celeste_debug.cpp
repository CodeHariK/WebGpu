
#include "../debug_draw/debug_manager.h"
#include "celeste_controller.h"
#include "celeste_state.h"
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#if DEBUG
namespace godot {

void CelesteController::debug_draw_trajectory(float p_delta) {
	if (Engine::get_singleton()->is_editor_hint()) {
		return;
	}
	// 3. Trajectory Tracking
	trajectory_timer += p_delta;
	if (trajectory_timer >= trajectory_interval) {
		trajectory_timer = 0.0f;
		trajectory_points.push_back(get_global_position());
		if (trajectory_points.size() > max_trajectory_points) {
			trajectory_points.erase(trajectory_points.begin());
		}
	}

	// Draw Trajectory
	DebugManager *dm = DebugManager::get_singleton();
	if (dm && trajectory_points.size() > 1) {
		for (size_t i = 0; i < trajectory_points.size() - 1; ++i) {
			String traj_id = "traj_" + String::num_int64(i);
			dm->draw_line(traj_id, trajectory_points[i], trajectory_points[i + 1], 0.2f, Color(0.2f, 0.8f, 1.0f, 0.6f), 0.1f);
		}
		// Connection to current position
		dm->draw_line("traj_head", trajectory_points.back(), get_global_position(), 0.02f, Color(1, 1, 0, 0.8f), 0.1f);
	}
}

void CelesteController::debug_draw_label() {
	if (current_state) {
		String id = "state_" + get_name();
		Vector3 v = get_velocity();
		float speed = v.length();
		float h_speed = Vector3(v.x, 0, v.z).length();

		String label_text = String(current_state->get_name()) +
				"\nVel: (" + String::num(v.x, 1) + ", " + String::num(v.y, 1) + ", " + String::num(v.z, 1) + ")" +
				"\nSpeed: " + String::num(speed, 1) + " (H: " + String::num(h_speed, 1) + ")" +
				"\nHover: " + (is_hovering ? "YES" : "NO") + " Err: " + String::num(last_spring_error, 2);

		DebugManager::get_singleton()->draw_text(id, label_text, get_global_position() + Vector3(0, 2.5f, 0), 0.001f, Color(0, 1, 0));
	}
}

} //namespace godot
#endif
