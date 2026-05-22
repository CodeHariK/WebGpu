#include "../ai/vehicle_state.h"
#include "../arcade_vehicle.h"
#include "debug_draw/debug_manager.h"
#include "game_manager/game_manager.h"
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

void ArcadeVehicle::_update_debug_arrows() {
	DebugManager *dm = DebugManager::get_singleton();
	String base_id = "veh_" + get_name() + "_";

	Vector3 current_vel = get_linear_velocity();
	Transform3D trans = get_global_transform();

	if (!dm || !game_manager || !game_manager->is_active(this)) {
		return;
	}

	Vector3 text_pos = trans.xform(Vector3(0.0f, 1.5f, 0.0f));
	String txt = String::num(get_input().steering) + " + " + current_state->state_name + " : (" + String::num(current_vel.x, 1) + " | " + String::num(current_vel.y, 1) + " | " + String::num(current_vel.z, 1) + ") ";

	if (is_drifting) {
		txt += "DRIFT NITRO: " + String::num(nitro_fuel, 1) + "%";
	} else if (is_boosting) {
		txt += "BOOST! NITRO: " + String::num(nitro_fuel, 1) + "% (+" + String::num(boost_speed_bonus, 1) + " m/s)";
	} else {
		txt += "NITRO: " + String::num(nitro_fuel, 1) + "%";
	}

	bool show = config.is_valid() && config->get_show_debug_velocity();

	if (!show) {
		dm->clear_line(base_id + "vel_x");
		dm->clear_line(base_id + "vel_y");
		dm->clear_line(base_id + "vel_z");
		dm->clear_sphere(base_id + "com");
	}

	// 3. Update Geometry
	Vector3 global_vel = get_linear_velocity();
	// Transform global velocity into local space
	Vector3 local_vel = trans.basis.inverse().xform(global_vel);

	float scale = config->get_debug_velocity_scale();
	float thickness = config->get_debug_velocity_width();

	// Origins for arrows (slight vertical offset to not be inside chassis)
	Vector3 origin = trans.xform(Vector3(0, 0.5f, 0));

	Vector3 local_x = trans.basis.get_column(0).normalized();
	last_roll_tilt = local_x.y;
	txt += "last_roll_tilt " + String::num(last_roll_tilt, 1);
	// dm->draw_line("avg_normal", text_pos, text_pos + avg_normal * 10, 0.3f, Color(0.5f, 1.0f, 0.2f));
	// dm->draw_line("local_x", text_pos, text_pos + local_x * 10, 0.3f, Color(1.0f, 0.0f, 0.0f));

	dm->draw_text("vehicle", txt, text_pos, 0.001f, Color(0.5f, 1.0f, 0.2f));

	// Draw Center of Mass sphere
	dm->draw_sphere(base_id + "com", trans.xform(current_com_offset), 0.25f, Color(1, 0, 0));

	// Update each arrow using world space coordinates
	// X-axis (Red)
	Vector3 x_end = trans.xform(Vector3(0, 0.5f, 0) + Vector3(local_vel.x * scale, 0, 0));
	dm->draw_line(base_id + "vel_x", origin, x_end, thickness, Color(1, 0, 0));

	// Y-axis (Green)
	Vector3 y_end = trans.xform(Vector3(0, 0.5f, 0) + Vector3(0, local_vel.y * scale, 0));
	dm->draw_line(base_id + "vel_y", origin, y_end, thickness, Color(0, 1, 0));

	// Z-axis (Blue) - usually forward/backward
	Vector3 z_end = trans.xform(Vector3(0, 0.5f, 0) + Vector3(0, 0, local_vel.z * scale));
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
