#ifndef MCCAMERA_H
#define MCCAMERA_H

#include <godot_cpp/classes/camera3d.hpp>
#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/variant/node_path.hpp>
#include <godot_cpp/variant/vector3.hpp>

namespace godot {

class MCCamera : public Camera3D {
	GDCLASS(MCCamera, Camera3D)

public:
	enum CameraMode {
		MODE_FLY,
		MODE_CAR,
		MODE_CHARACTER,
		MODE_ORBIT
	};

	struct SpringState {
		Vector3 current;
		Vector3 velocity;
		Vector3 target;

		void step(float p_delta, float p_frequency, float p_damping, float p_response);
	};

	struct FloatSpring {
		float current = 0.0f;
		float velocity = 0.0f;
		float target = 0.0f;

		void step(float p_delta, float p_frequency, float p_damping, float p_response);
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
	SpringState pos_spring;
	FloatSpring dist_spring;
	FloatSpring yaw_spring;
	FloatSpring pitch_spring;

	// Interaction / Orbit
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
	void _unhandled_input(const Ref<InputEvent> &p_event) override;

	// Getters/Setters
	void set_mode(CameraMode p_mode);
	CameraMode get_mode() const;

	void set_follow_target_path(const NodePath &p_path);
	NodePath get_follow_target_path() const;

	void set_frequency(float p_freq) { frequency = p_freq; }
	float get_frequency() const { return frequency; }

	void set_damping(float p_damping) { damping = p_damping; }
	float get_damping() const { return damping; }

	void set_pan_speed(float p_speed) { pan_speed = p_speed; }
	float get_pan_speed() const { return pan_speed; }

	void set_zoom_speed(float p_speed) { zoom_speed = p_speed; }
	float get_zoom_speed() const { return zoom_speed; }

	void set_collision_enabled(bool p_enabled) { collision_enabled = p_enabled; }
	bool is_collision_enabled() const { return collision_enabled; }
};

} // namespace godot

VARIANT_ENUM_CAST(godot::MCCamera::CameraMode);

#endif // MCCAMERA_H
