#ifndef MCCAMERA_H
#define MCCAMERA_H

#include "../utils/spring/spring_dynamics.h"
#include "modes/camera_mode.h"
#include "../utils/raycast/mc_raycast.h"
#include <godot_cpp/classes/camera3d.hpp>
#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/variant/node_path.hpp>
#include <godot_cpp/variant/vector3.hpp>

namespace godot {
class PlayerInput;
class GameManager;

class MCCamera : public Camera3D {
	GDCLASS(MCCamera, Camera3D)

	friend class GameManager;

	friend class CameraMode;
	friend class MCFlyCameraMode;
	friend class MCCarCameraMode;
	friend class MCTPSCameraMode;

public:
	enum CameraState {
		MODE_FLY,
		MODE_CAR,
		MODE_TPS
	};

private:
	// Mode & Targeting
	CameraState mode = MODE_FLY;
	CameraMode *current_mode_instance = nullptr;

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

	PlayerInput *player_input = nullptr;

	void _update_follow_node();
	Vector3 _calculate_ideal_position();
	float _solve_collision(const Vector3 &p_from, const Vector3 &p_to);

protected:
	static void _bind_methods();

public:
	MCCamera();
	~MCCamera();

	void _ready() override;
	void _physics_process(double p_delta) override;

	// Getters/Setters
	void set_mode(CameraState p_mode);
	CameraState get_mode() const;

	float get_yaw() const { return yaw; }
	float get_pitch() const { return pitch; }

	void set_follow_target_path(const NodePath &p_path);
	NodePath get_follow_target_path() const;

	void set_follow_target_node(Node3D *p_node);
	Node3D *get_follow_target_node() const { return follow_target_node; }

	void set_player_input(PlayerInput *p_input) { player_input = p_input; }
	PlayerInput *get_player_input() const { return player_input; }

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

	// Raycasting
	MCRaycastHit get_center_raycast_hit(uint32_t p_mask = 0xFFFFFFFF, float p_dist = 1000.0f);
};

} // namespace godot

VARIANT_ENUM_CAST(godot::MCCamera::CameraState);

#endif // MCCAMERA_H
