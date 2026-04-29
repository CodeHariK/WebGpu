
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

	DebugManager *dm = DebugManager::get_singleton();
	if (dm) {
		String id = "traj_" + get_name();
		dm->draw_trajectory(id, get_global_position(), p_delta, 0.1f, 200, Color(0.9f, 0.1f, 0.1f));
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
				"\nHover: " + (is_hovering ? "YES" : "NO") + " Err: " + String::num(last_spring_error, 2) +
				"\nRideHeight: " + String::num(ride_height, 2) + " [" + String::num(min_ride_height, 2) + " - " + String::num(max_ride_height, 2) + "]";

		DebugManager::get_singleton()->draw_text(id, label_text, get_global_position() + Vector3(0, 2.5f, 0), 0.001f, Color(0, 1, 0));
	}
}

void CelesteController::debug_draw_bottom() {
	DebugManager *dm = DebugManager::get_singleton();
	if (dm) {
		Vector3 pos = get_global_position();
		Vector3 bottom = pos - Vector3(0, half_height, 0);
		String id = "controller_bottom_" + get_name();

		// Draw the forward ray range (1.5m as used in physics_process)
		Vector3 forward = -get_global_transform().basis.get_column(2).normalized();
		Vector3 f_start = bottom + Vector3(0, 0.2f, 0);
		Vector3 f_end = f_start + forward * 1.5f;

		// Draw the "ghost" ray to show full range
		dm->draw_line(id + "_forward_range", f_start, f_end, 0.02f, Color(1, 1, 0, 0.3f), 0.05f);
	}
}

} //namespace godot
#endif
