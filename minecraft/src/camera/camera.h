#ifndef MCCAMERA_H
#define MCCAMERA_H

#include "../utils/spring/spring_dynamics.h"
#include <godot_cpp/classes/camera3d.hpp>
#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/variant/node_path.hpp>
#include <godot_cpp/variant/vector3.hpp>

namespace godot {

class MCCamera : public Camera3D {
	GDCLASS(MCCamera, Camera3D)
	
	friend class GameManager;

public:
	enum CameraMode {
		MODE_FLY,
		MODE_CAR,
		MODE_CHARACTER,
		MODE_ORBIT
	};

private:
	// Mode & Targeting
	CameraMode mode = MODE_FLY;
	NodePath follow_target_path;
	Node3D *follow_target_node = nullptr;

	// Orientation (Yaw/Pitch)
	float yaw = 0.0f;
	float pitch = 0.0f;

	// Spring Parameters
	float frequency = 2.0f;
	float damping = 1.0f;
	float response = 0.0f;

	// States for smoothing
	SpringDynamics<Vector3> pos_spring;
	SpringDynamics<float> dist_spring;
	SpringDynamics<float> yaw_spring;
	SpringDynamics<float> pitch_spring;

	// Interaction / Orbit
	Vector3 follow_offset = Vector3(0, 2, 8);
	float target_distance = 10.0f;
	float min_distance = 2.0f;
	float max_distance = 50.0f;
	float pan_speed = 0.05f;
	float zoom_speed = 1.0f;
	float orbit_sensitivity = 0.005f;
	bool is_orbiting = false;

	// Collision
	bool collision_enabled = true;
	uint32_t collision_mask = 1;
	float collision_margin = 0.2f;

	// Stability Lock (from user suggestion)
	bool stability_lock_enabled = true;
	float stability_threshold = 2.0f;

	void _update_follow_node();
	Vector3 _calculate_ideal_position();
	float _solve_collision(const Vector3 &p_from, const Vector3 &p_to);

	void _process_fly_mode(float p_delta);
	void _process_car_mode(float p_delta);
	void _process_follow_modes(float p_delta);

protected:
	static void _bind_methods();

public:
	MCCamera();
	~MCCamera();

	void _ready() override;
	void _physics_process(double p_delta) override;
	void _unhandled_input(const Ref<InputEvent> &p_event) override;

	// Getters/Setters
	void set_mode(CameraMode p_mode);
	CameraMode get_mode() const;

	void set_follow_target_path(const NodePath &p_path);
	NodePath get_follow_target_path() const;

	void set_follow_target_node(Node3D *p_node);
	Node3D *get_follow_target_node() const { return follow_target_node; }

	void set_frequency(float p_freq) { frequency = p_freq; }
	float get_frequency() const { return frequency; }

	void set_damping(float p_damping) { damping = p_damping; }
	float get_damping() const { return damping; }

	void set_pan_speed(float p_speed) { pan_speed = p_speed; }
	float get_pan_speed() const { return pan_speed; }

	void set_zoom_speed(float p_speed) { zoom_speed = p_speed; }
	float get_zoom_speed() const { return zoom_speed; }

	void set_follow_offset(const Vector3 &p_offset);
	Vector3 get_follow_offset() const { return follow_offset; }

	void set_min_distance(float p_dist) { min_distance = p_dist; }
	float get_min_distance() const { return min_distance; }

	void set_max_distance(float p_dist) { max_distance = p_dist; }
	float get_max_distance() const { return max_distance; }

	void set_collision_enabled(bool p_enabled) { collision_enabled = p_enabled; }
	bool is_collision_enabled() const { return collision_enabled; }

	void set_stability_lock_enabled(bool p_enabled) { stability_lock_enabled = p_enabled; }
	bool is_stability_lock_enabled() const { return stability_lock_enabled; }

	void set_stability_threshold(float p_threshold) { stability_threshold = p_threshold; }
	float get_stability_threshold() const { return stability_threshold; }

};

} // namespace godot

VARIANT_ENUM_CAST(godot::MCCamera::CameraMode);

#endif // MCCAMERA_H
