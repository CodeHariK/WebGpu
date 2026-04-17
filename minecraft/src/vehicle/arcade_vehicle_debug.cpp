#include "arcade_vehicle.h"
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

void ArcadeVehicle::_update_debug_arrows() {
	bool show = config.is_valid() && config->get_show_debug_velocity();

	// 1. Initialization
	if (show && vel_x_arrow == nullptr) {
		vel_x_arrow = memnew(DebugLineQuad);
		vel_x_arrow->set_name("DebugVelX");
		add_child(vel_x_arrow);
		vel_x_arrow->set_color(Color(1, 0, 0)); // Red

		vel_y_arrow = memnew(DebugLineQuad);
		vel_y_arrow->set_name("DebugVelY");
		add_child(vel_y_arrow);
		vel_y_arrow->set_color(Color(0, 1, 0)); // Green

		vel_z_arrow = memnew(DebugLineQuad);
		vel_z_arrow->set_name("DebugVelZ");
		add_child(vel_z_arrow);
		vel_z_arrow->set_color(Color(0, 0, 1)); // Blue
	}

	// 2. Visibility Toggle
	if (vel_x_arrow)
		vel_x_arrow->set_visible(show);
	if (vel_y_arrow)
		vel_y_arrow->set_visible(show);
	if (vel_z_arrow)
		vel_z_arrow->set_visible(show);

	if (!show)
		return;

	// 3. Update Geometry
	Vector3 global_vel = get_linear_velocity();
	// Transform global velocity into local space
	Vector3 local_vel = get_global_transform().basis.inverse().xform(global_vel);

	float scale = config->get_debug_velocity_scale();
	float thickness = config->get_debug_velocity_width();

	// Origins for arrows (slight vertical offset to not be inside chassis)
	Vector3 origin = Vector3(0, 0.5f, 0);

	// Update each arrow
	// X-axis (Red)
	vel_x_arrow->set_line(origin, origin + Vector3(local_vel.x * scale, 0, 0), thickness);

	// Y-axis (Green)
	vel_y_arrow->set_line(origin, origin + Vector3(0, local_vel.y * scale, 0), thickness);

	// Z-axis (Blue) - usually forward/backward
	vel_z_arrow->set_line(origin, origin + Vector3(0, 0, local_vel.z * scale), thickness);
}

} // namespace godot
