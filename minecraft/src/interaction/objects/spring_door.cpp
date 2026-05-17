#include "spring_door.h"
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

void SpringDoor::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_stiffness", "stiffness"), &SpringDoor::set_stiffness);
	ClassDB::bind_method(D_METHOD("get_stiffness"), &SpringDoor::get_stiffness);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "stiffness"), "set_stiffness", "get_stiffness");

	ClassDB::bind_method(D_METHOD("set_damping", "damping"), &SpringDoor::set_damping);
	ClassDB::bind_method(D_METHOD("get_damping"), &SpringDoor::get_damping);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "damping"), "set_damping", "get_damping");

	ClassDB::bind_method(D_METHOD("set_hinge_offset", "offset"), &SpringDoor::set_hinge_offset);
	ClassDB::bind_method(D_METHOD("get_hinge_offset"), &SpringDoor::get_hinge_offset);
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "hinge_offset"), "set_hinge_offset", "get_hinge_offset");
}

SpringDoor::SpringDoor() {}
SpringDoor::~SpringDoor() {}

void SpringDoor::_ready() {
	rest_rotation_y = get_rotation().y;

	// Lock all position movement - This pins the Origin (0,0,0) in space
	set_axis_lock(PhysicsServer3D::BODY_AXIS_LINEAR_X, true);
	set_axis_lock(PhysicsServer3D::BODY_AXIS_LINEAR_Y, true);
	set_axis_lock(PhysicsServer3D::BODY_AXIS_LINEAR_Z, true);

	// Lock rotation on X and Z
	set_axis_lock(PhysicsServer3D::BODY_AXIS_ANGULAR_X, true);
	set_axis_lock(PhysicsServer3D::BODY_AXIS_ANGULAR_Z, true);

	// If a hinge offset is provided, we tell the physics engine that the 
	// center of mass is shifted. This makes it rotate around the Origin (the hinge).
	if (!hinge_offset.is_zero_approx()) {
		set_center_of_mass_mode(CENTER_OF_MASS_MODE_CUSTOM);
		set_center_of_mass(hinge_offset);
	}
}

void SpringDoor::_physics_process(double delta) {
	// 1. Calculate displacement from rest
	float current_rotation_y = get_rotation().y;
	float displacement = current_rotation_y - rest_rotation_y;

	// 2. Wrap angle to shortest path
	displacement = UtilityFunctions::wrapf(displacement, -Math_PI, Math_PI);

	// 3. Get current angular velocity (Y axis)
	float current_vel_y = get_angular_velocity().y;

	// 4. Calculate spring torque
	float spring_torque = -stiffness * displacement - damping * current_vel_y;

	// 5. Apply the torque to ourselves
	apply_torque(Vector3(0, spring_torque, 0));
}

void SpringDoor::set_stiffness(float p_stiffness) { stiffness = p_stiffness; }
float SpringDoor::get_stiffness() const { return stiffness; }

void SpringDoor::set_damping(float p_damping) { damping = p_damping; }
float SpringDoor::get_damping() const { return damping; }

void SpringDoor::set_hinge_offset(Vector3 p_offset) { hinge_offset = p_offset; }
Vector3 SpringDoor::get_hinge_offset() const { return hinge_offset; }

} // namespace godot
