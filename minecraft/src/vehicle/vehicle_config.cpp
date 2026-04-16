#include "vehicle_config.h"
#include <godot_cpp/core/class_db.hpp>

namespace godot {

void VehicleConfig::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_chassis_size", "size"), &VehicleConfig::set_chassis_size);
	ClassDB::bind_method(D_METHOD("get_chassis_size"), &VehicleConfig::get_chassis_size);
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "chassis_size"), "set_chassis_size", "get_chassis_size");

	ClassDB::bind_method(D_METHOD("set_center_of_mass_offset", "offset"), &VehicleConfig::set_center_of_mass_offset);
	ClassDB::bind_method(D_METHOD("get_center_of_mass_offset"), &VehicleConfig::get_center_of_mass_offset);
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "center_of_mass_offset"), "set_center_of_mass_offset", "get_center_of_mass_offset");

	ClassDB::bind_method(D_METHOD("set_mass", "mass"), &VehicleConfig::set_mass);
	ClassDB::bind_method(D_METHOD("get_mass"), &VehicleConfig::get_mass);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "mass"), "set_mass", "get_mass");

	ClassDB::bind_method(D_METHOD("set_max_speed", "speed"), &VehicleConfig::set_max_speed);
	ClassDB::bind_method(D_METHOD("get_max_speed"), &VehicleConfig::get_max_speed);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "max_speed"), "set_max_speed", "get_max_speed");

	ClassDB::bind_method(D_METHOD("set_max_accel_force", "force"), &VehicleConfig::set_max_accel_force);
	ClassDB::bind_method(D_METHOD("get_max_accel_force"), &VehicleConfig::get_max_accel_force);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "max_accel_force"), "set_max_accel_force", "get_max_accel_force");

	ClassDB::bind_method(D_METHOD("set_brake_decel", "decel"), &VehicleConfig::set_brake_decel);
	ClassDB::bind_method(D_METHOD("get_brake_decel"), &VehicleConfig::get_brake_decel);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "brake_decel"), "set_brake_decel", "get_brake_decel");

	ClassDB::bind_method(D_METHOD("set_arcade_assist", "assist"), &VehicleConfig::set_arcade_assist);
	ClassDB::bind_method(D_METHOD("get_arcade_assist"), &VehicleConfig::get_arcade_assist);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "arcade_assist"), "set_arcade_assist", "get_arcade_assist");

	ClassDB::bind_method(D_METHOD("set_max_steer_angle_deg", "angle"), &VehicleConfig::set_max_steer_angle_deg);
	ClassDB::bind_method(D_METHOD("get_max_steer_angle_deg"), &VehicleConfig::get_max_steer_angle_deg);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "max_steer_angle_deg"), "set_max_steer_angle_deg", "get_max_steer_angle_deg");

	ClassDB::bind_method(D_METHOD("set_base_grip", "grip"), &VehicleConfig::set_base_grip);
	ClassDB::bind_method(D_METHOD("get_base_grip"), &VehicleConfig::get_base_grip);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "base_grip"), "set_base_grip", "get_base_grip");

	ClassDB::bind_method(D_METHOD("set_drift_grip", "grip"), &VehicleConfig::set_drift_grip);
	ClassDB::bind_method(D_METHOD("get_drift_grip"), &VehicleConfig::get_drift_grip);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "drift_grip"), "set_drift_grip", "get_drift_grip");

	ClassDB::bind_method(D_METHOD("set_drift_speed_threshold", "speed"), &VehicleConfig::set_drift_speed_threshold);
	ClassDB::bind_method(D_METHOD("get_drift_speed_threshold"), &VehicleConfig::get_drift_speed_threshold);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "drift_speed_threshold"), "set_drift_speed_threshold", "get_drift_speed_threshold");

	ClassDB::bind_method(D_METHOD("set_drift_steering_threshold", "steer"), &VehicleConfig::set_drift_steering_threshold);
	ClassDB::bind_method(D_METHOD("get_drift_steering_threshold"), &VehicleConfig::get_drift_steering_threshold);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "drift_steering_threshold"), "set_drift_steering_threshold", "get_drift_steering_threshold");

	ClassDB::bind_method(D_METHOD("set_downforce", "force"), &VehicleConfig::set_downforce);
	ClassDB::bind_method(D_METHOD("get_downforce"), &VehicleConfig::get_downforce);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "downforce"), "set_downforce", "get_downforce");

	ClassDB::bind_method(D_METHOD("set_angular_damping", "damping"), &VehicleConfig::set_angular_damping);
	ClassDB::bind_method(D_METHOD("get_angular_damping"), &VehicleConfig::get_angular_damping);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "angular_damping"), "set_angular_damping", "get_angular_damping");

	ClassDB::bind_method(D_METHOD("set_velocity_alignment", "alignment"), &VehicleConfig::set_velocity_alignment);
	ClassDB::bind_method(D_METHOD("get_velocity_alignment"), &VehicleConfig::get_velocity_alignment);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "velocity_alignment"), "set_velocity_alignment", "get_velocity_alignment");

	ClassDB::bind_method(D_METHOD("set_wheel_configs", "configs"), &VehicleConfig::set_wheel_configs);
	ClassDB::bind_method(D_METHOD("get_wheel_configs"), &VehicleConfig::get_wheel_configs);
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "wheel_configs", PROPERTY_HINT_ARRAY_TYPE, "WheelConfig"), "set_wheel_configs", "get_wheel_configs");
}

VehicleConfig::VehicleConfig() {
	// Setup 4 default wheels when the resource is created
	Ref<WheelConfig> fl_wheel = memnew(WheelConfig);
	fl_wheel->set_hardpoint_offset(Vector3(1.0f, 0.0f, 1.5f));

	Ref<WheelConfig> fr_wheel = memnew(WheelConfig);
	fr_wheel->set_hardpoint_offset(Vector3(-1.0f, 0.0f, 1.5f));

	Ref<WheelConfig> rl_wheel = memnew(WheelConfig);
	rl_wheel->set_hardpoint_offset(Vector3(1.0f, 0.0f, -1.5f));

	Ref<WheelConfig> rr_wheel = memnew(WheelConfig);
	rr_wheel->set_hardpoint_offset(Vector3(-1.0f, 0.0f, -1.5f));

	wheel_configs.push_back(fl_wheel);
	wheel_configs.push_back(fr_wheel);
	wheel_configs.push_back(rl_wheel);
	wheel_configs.push_back(rr_wheel);
}

VehicleConfig::~VehicleConfig() {
}

void VehicleConfig::set_chassis_size(const Vector3 &p_size) {
	chassis_size = p_size;
}

Vector3 VehicleConfig::get_chassis_size() const {
	return chassis_size;
}

void VehicleConfig::set_center_of_mass_offset(const Vector3 &p_offset) {
	center_of_mass_offset = p_offset;
}

Vector3 VehicleConfig::get_center_of_mass_offset() const {
	return center_of_mass_offset;
}

void VehicleConfig::set_mass(float p_mass) {
	mass = p_mass;
}

float VehicleConfig::get_mass() const {
	return mass;
}

void VehicleConfig::set_max_speed(float p_val) { max_speed = p_val; }
float VehicleConfig::get_max_speed() const { return max_speed; }

void VehicleConfig::set_max_accel_force(float p_val) { max_accel_force = p_val; }
float VehicleConfig::get_max_accel_force() const { return max_accel_force; }

void VehicleConfig::set_brake_decel(float p_val) { brake_decel = p_val; }
float VehicleConfig::get_brake_decel() const { return brake_decel; }

void VehicleConfig::set_arcade_assist(float p_val) { arcade_assist = p_val; }
float VehicleConfig::get_arcade_assist() const { return arcade_assist; }

void VehicleConfig::set_max_steer_angle_deg(float p_val) { max_steer_angle_deg = p_val; }
float VehicleConfig::get_max_steer_angle_deg() const { return max_steer_angle_deg; }

void VehicleConfig::set_base_grip(float p_val) { base_grip = p_val; }
float VehicleConfig::get_base_grip() const { return base_grip; }

void VehicleConfig::set_drift_grip(float p_val) { drift_grip = p_val; }
float VehicleConfig::get_drift_grip() const { return drift_grip; }

void VehicleConfig::set_drift_speed_threshold(float p_val) { drift_speed_threshold = p_val; }
float VehicleConfig::get_drift_speed_threshold() const { return drift_speed_threshold; }

void VehicleConfig::set_drift_steering_threshold(float p_val) { drift_steering_threshold = p_val; }
float VehicleConfig::get_drift_steering_threshold() const { return drift_steering_threshold; }

void VehicleConfig::set_downforce(float p_val) { downforce = p_val; }
float VehicleConfig::get_downforce() const { return downforce; }

void VehicleConfig::set_angular_damping(float p_val) { angular_damping = p_val; }
float VehicleConfig::get_angular_damping() const { return angular_damping; }

void VehicleConfig::set_velocity_alignment(float p_val) { velocity_alignment = p_val; }
float VehicleConfig::get_velocity_alignment() const { return velocity_alignment; }

void VehicleConfig::set_wheel_configs(const TypedArray<WheelConfig> &p_configs) {
	wheel_configs = p_configs;
}

TypedArray<WheelConfig> VehicleConfig::get_wheel_configs() const {
	return wheel_configs;
}

} // namespace godot
