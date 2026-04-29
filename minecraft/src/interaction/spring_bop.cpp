#include "spring_bop.h"
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

void SpringBop::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_stiffness", "stiffness"), &SpringBop::set_stiffness);
	ClassDB::bind_method(D_METHOD("get_stiffness"), &SpringBop::get_stiffness);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "stiffness"), "set_stiffness", "get_stiffness");

	ClassDB::bind_method(D_METHOD("set_damping", "damping"), &SpringBop::set_damping);
	ClassDB::bind_method(D_METHOD("get_damping"), &SpringBop::get_damping);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "damping"), "set_damping", "get_damping");

	ClassDB::bind_method(D_METHOD("set_lock_y_rotation", "lock"), &SpringBop::set_lock_y_rotation);
	ClassDB::bind_method(D_METHOD("get_lock_y_rotation"), &SpringBop::get_lock_y_rotation);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "lock_y_rotation"), "set_lock_y_rotation", "get_lock_y_rotation");

	ClassDB::bind_method(D_METHOD("set_hinge_offset", "offset"), &SpringBop::set_hinge_offset);
	ClassDB::bind_method(D_METHOD("get_hinge_offset"), &SpringBop::get_hinge_offset);
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "hinge_offset"), "set_hinge_offset", "get_hinge_offset");
}

SpringBop::SpringBop() {}
SpringBop::~SpringBop() {}

void SpringBop::_ready() {
	// 1. Lock all translation (The bag stays attached to the ceiling)
	set_axis_lock(PhysicsServer3D::BODY_AXIS_LINEAR_X, true);
	set_axis_lock(PhysicsServer3D::BODY_AXIS_LINEAR_Y, true);
	set_axis_lock(PhysicsServer3D::BODY_AXIS_LINEAR_Z, true);

	// 2. We do NOT lock Angular X/Z here, otherwise it can't wobble!
	if (lock_y_rotation) {
		set_axis_lock(PhysicsServer3D::BODY_AXIS_ANGULAR_Y, true);
	}

	// 3. Offset center of mass so it behaves like a heavy bag hanging down
	if (!hinge_offset.is_zero_approx()) {
		set_center_of_mass_mode(CENTER_OF_MASS_MODE_CUSTOM);
		set_center_of_mass(hinge_offset);
	}
}

void SpringBop::_physics_process(double delta) {
	// --- STABLE VECTOR SPRING LOGIC ---
	
	// Get our current local UP vector in global space
	Vector3 current_up = get_global_transform().basis.get_column(1).normalized();
	
	// Target is world UP (hanging perfectly straight)
	Vector3 target_up = Vector3(0, 1, 0);
	
	// The cross product gives us the axis and magnitude of rotation needed to return to center
	Vector3 error = current_up.cross(target_up);
	
	// Calculate spring torque
	Vector3 spring_torque = error * stiffness;
	
	// Apply damping based on angular velocity
	Vector3 current_ang_vel = get_angular_velocity();
	spring_torque -= current_ang_vel * damping;

	// Apply the corrective torque
	apply_torque(spring_torque);
}

void SpringBop::set_stiffness(float p_stiffness) { stiffness = p_stiffness; }
float SpringBop::get_stiffness() const { return stiffness; }

void SpringBop::set_damping(float p_damping) { damping = p_damping; }
float SpringBop::get_damping() const { return damping; }

void SpringBop::set_lock_y_rotation(bool p_lock) { lock_y_rotation = p_lock; }
bool SpringBop::get_lock_y_rotation() const { return lock_y_rotation; }

void SpringBop::set_hinge_offset(Vector3 p_offset) { hinge_offset = p_offset; }
Vector3 SpringBop::get_hinge_offset() const { return hinge_offset; }

} // namespace godot
