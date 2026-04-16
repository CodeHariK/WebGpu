#ifndef VEHICLE_CONFIG_H
#define VEHICLE_CONFIG_H

#include <godot_cpp/classes/resource.hpp>
#include <godot_cpp/variant/typed_array.hpp>
#include <godot_cpp/variant/vector3.hpp>
#include "wheel_config.h"

namespace godot {

class VehicleConfig : public Resource {
	GDCLASS(VehicleConfig, Resource)

private:
	Vector3 chassis_size = Vector3(2.0f, 1.0f, 4.0f);
	Vector3 center_of_mass_offset = Vector3(0.0f, -0.5f, 0.0f);
	float mass = 1500.0f;
	
	// Arcade driving properties
	float max_speed = 30.0f;
	float max_accel_force = 8000.0f;
	float brake_decel = 12000.0f;
	float arcade_assist = 3.0f;
	float max_steer_angle_deg = 35.0f;
	float base_grip = 0.9f;
	float drift_grip = 0.3f;
	float drift_speed_threshold = 10.0f;
	float drift_steering_threshold = 0.5f;
	float downforce = 2000.0f;
	float angular_damping = 5.0f;
	float velocity_alignment = 2.0f;
	
	TypedArray<WheelConfig> wheel_configs;

protected:
	static void _bind_methods();

public:
	VehicleConfig();
	~VehicleConfig();

	void set_chassis_size(const Vector3 &p_size);
	Vector3 get_chassis_size() const;

	void set_center_of_mass_offset(const Vector3 &p_offset);
	Vector3 get_center_of_mass_offset() const;

	void set_mass(float p_mass);
	float get_mass() const;

	void set_max_speed(float p_val);
	float get_max_speed() const;

	void set_max_accel_force(float p_val);
	float get_max_accel_force() const;

	void set_brake_decel(float p_val);
	float get_brake_decel() const;

	void set_arcade_assist(float p_val);
	float get_arcade_assist() const;

	void set_max_steer_angle_deg(float p_val);
	float get_max_steer_angle_deg() const;

	void set_base_grip(float p_val);
	float get_base_grip() const;

	void set_drift_grip(float p_val);
	float get_drift_grip() const;

	void set_drift_speed_threshold(float p_val);
	float get_drift_speed_threshold() const;

	void set_drift_steering_threshold(float p_val);
	float get_drift_steering_threshold() const;

	void set_downforce(float p_val);
	float get_downforce() const;

	void set_angular_damping(float p_val);
	float get_angular_damping() const;

	void set_velocity_alignment(float p_val);
	float get_velocity_alignment() const;

	void set_wheel_configs(const TypedArray<WheelConfig> &p_configs);
	TypedArray<WheelConfig> get_wheel_configs() const;
};

} // namespace godot

#endif // VEHICLE_CONFIG_H
