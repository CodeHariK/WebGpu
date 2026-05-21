#ifndef VEHICLE_CONFIG_H
#define VEHICLE_CONFIG_H

#include "wheel_config.h"
#include <godot_cpp/classes/resource.hpp>
#include <godot_cpp/variant/typed_array.hpp>
#include <godot_cpp/variant/vector3.hpp>

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

	// Stunt properties
	Vector3 stunt_com_offset = Vector3(0.0f, 0.0f, 0.0f);
	float stunt_torque_strength = 8000.0f;
	float stunt_com_interpolation_speed = 5.0f;
	float ramp_detection_threshold = 0.9f;
	float stunt_recovery_height = 2.0f;

	bool show_debug_velocity = false;
	float debug_velocity_scale = 0.5f;
	float debug_velocity_width = 0.3f;

	// Advanced Drift properties
	float drift_steer_torque_multiplier = 1.8f;
	float drift_slowdown_factor = 0.75f;
	float drift_boost_speed_coefficient = 6.0f;
	float drift_boost_duration_coefficient = 1.0f;
	float drift_boost_max_speed_bonus = 20.0f;
	float drift_boost_max_duration = 3.0f;
	float drift_chain_window = 1.5f;
	float drift_chain_bonus_multiplier = 0.25f;

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

	void set_stunt_com_offset(const Vector3 &p_offset);
	Vector3 get_stunt_com_offset() const;

	void set_stunt_torque_strength(float p_val);
	float get_stunt_torque_strength() const;

	void set_stunt_com_interpolation_speed(float p_val);
	float get_stunt_com_interpolation_speed() const;

	void set_ramp_detection_threshold(float p_val);
	float get_ramp_detection_threshold() const;

	void set_stunt_recovery_height(float p_val);
	float get_stunt_recovery_height() const;

	void set_show_debug_velocity(bool p_val);
	bool get_show_debug_velocity() const;

	void set_debug_velocity_scale(float p_val);
	float get_debug_velocity_scale() const;

	void set_debug_velocity_width(float p_val);
	float get_debug_velocity_width() const;

	void set_drift_steer_torque_multiplier(float p_val);
	float get_drift_steer_torque_multiplier() const;

	void set_drift_slowdown_factor(float p_val);
	float get_drift_slowdown_factor() const;

	void set_drift_boost_speed_coefficient(float p_val);
	float get_drift_boost_speed_coefficient() const;

	void set_drift_boost_duration_coefficient(float p_val);
	float get_drift_boost_duration_coefficient() const;

	void set_drift_boost_max_speed_bonus(float p_val);
	float get_drift_boost_max_speed_bonus() const;

	void set_drift_boost_max_duration(float p_val);
	float get_drift_boost_max_duration() const;

	void set_drift_chain_window(float p_val);
	float get_drift_chain_window() const;

	void set_drift_chain_bonus_multiplier(float p_val);
	float get_drift_chain_bonus_multiplier() const;

	void set_wheel_configs(const TypedArray<WheelConfig> &p_configs);
	TypedArray<WheelConfig> get_wheel_configs() const;
};

} // namespace godot

#endif // VEHICLE_CONFIG_H
