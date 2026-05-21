#include "../arcade_vehicle.h"
#include "debug_draw/debug_manager.h"
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

void ArcadeVehicle::_update_debug_arrows() {
	DebugManager *dm = DebugManager::get_singleton();
	String base_id = "veh_" + get_name() + "_";

	Vector3 current_vel = get_linear_velocity();

	if (dm) {
		Vector3 text_pos = get_global_transform().xform(Vector3(0.0f, 1.5f, 0.0f));
		String txt = "(" + String::num(current_vel.x, 1) + " | " + String::num(current_vel.y, 1) + " | " + String::num(current_vel.z, 1) + ") ";

		if (is_drifting) {
			txt += "DRIFT: " + String::num(drift_timer, 1) + "s";
			Color col = Color(1.0f, 0.8f, 0.0f); // Default Orange

			if (drift_chain_count > 0) {
				txt += " (Chain x" + String::num_int64(drift_chain_count + 1) + ")";
				if (drift_chain_count == 1) {
					col = Color(0.0f, 0.9f, 1.0f); // Cyan
				} else {
					col = Color(0.9f, 0.1f, 1.0f); // Magenta/Purple
				}
			}
			dm->draw_text(base_id + "drift", txt, text_pos, 0.002f, col);
		} else if (is_boosting) {
			txt += "BOOST! " + String::num(boost_timer, 1) + "s (+" + String::num(boost_speed_bonus, 1) + " m/s)";

			dm->draw_text(base_id + "drift", txt, text_pos, 0.002f, Color(0.2f, 1.0f, 0.2f));
		} else {
			dm->draw_text(base_id + "drift", txt, text_pos, 0.002f, Color(0.5f, 1.0f, 0.2f));
		}
	}

	bool show = config.is_valid() && config->get_show_debug_velocity();

	if (!show) {
		if (dm) {
			dm->clear_line(base_id + "vel_x");
			dm->clear_line(base_id + "vel_y");
			dm->clear_line(base_id + "vel_z");
		}
		return;
	}

	// 3. Update Geometry
	Vector3 global_vel = get_linear_velocity();
	// Transform global velocity into local space
	Vector3 local_vel = get_global_transform().basis.inverse().xform(global_vel);

	float scale = config->get_debug_velocity_scale();
	float thickness = config->get_debug_velocity_width();

	// Origins for arrows (slight vertical offset to not be inside chassis)
	Vector3 origin = get_global_transform().xform(Vector3(0, 0.5f, 0));

	if (!dm)
		return;

	// Update each arrow using world space coordinates
	// X-axis (Red)
	Vector3 x_end = get_global_transform().xform(Vector3(0, 0.5f, 0) + Vector3(local_vel.x * scale, 0, 0));
	dm->draw_line(base_id + "vel_x", origin, x_end, thickness, Color(1, 0, 0));

	// Y-axis (Green)
	Vector3 y_end = get_global_transform().xform(Vector3(0, 0.5f, 0) + Vector3(0, local_vel.y * scale, 0));
	dm->draw_line(base_id + "vel_y", origin, y_end, thickness, Color(0, 1, 0));

	// Z-axis (Blue) - usually forward/backward
	Vector3 z_end = get_global_transform().xform(Vector3(0, 0.5f, 0) + Vector3(0, 0, local_vel.z * scale));
	dm->draw_line(base_id + "vel_z", origin, z_end, thickness, Color(0, 0, 1));
}

void ArcadeVehicle::debug_draw_trajectory(float p_delta) {
	if (Engine::get_singleton()->is_editor_hint()) {
		return;
	}

	DebugManager *dm = DebugManager::get_singleton();
	if (!dm) {
		return;
	}

	String id = "traj_" + get_name();

	if (debug_visuals_enabled) {
		dm->draw_trajectory(id, get_global_transform().origin, p_delta, 0.1f, 200, Color(0.1f, 0.9f, 0.9f));
	} else {
		dm->clear_trajectory(id);
	}
}

} // namespace godot
