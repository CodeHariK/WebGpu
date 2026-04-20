#include "debug_draw/debug_manager.h"
#include "../arcade_vehicle.h"
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

void ArcadeVehicle::_update_debug_arrows() {
	bool show = config.is_valid() && config->get_show_debug_velocity();

	String base_id = "veh_" + get_name() + "_";

	if (!show) {
		DebugManager *dm = DebugManager::get_singleton();
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

	DebugManager *dm = DebugManager::get_singleton();
	if (!dm) return;

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

} // namespace godot
