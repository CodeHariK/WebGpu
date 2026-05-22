#ifndef ARCADE_VEHICLE_H
#define ARCADE_VEHICLE_H

#include "../utils/raycast/mc_raycast.h"
#include "config/vehicle_config.h"
#include <godot_cpp/classes/box_shape3d.hpp>
#include <godot_cpp/classes/collision_shape3d.hpp>
#include <godot_cpp/classes/csg_box3d.hpp>
#include <godot_cpp/classes/csg_sphere3d.hpp>
#include <godot_cpp/classes/physics_direct_body_state3d.hpp>
#include <godot_cpp/classes/rigid_body3d.hpp>

namespace godot {

class GameManager;
class VehicleState;
class GroundedState;
class AirborneState;
class DrivingState;
class GlidingState;
class PlayerInput;
class ArcadeVehicleUI;
class CUI;

class ArcadeVehicle : public RigidBody3D {
	GDCLASS(ArcadeVehicle, RigidBody3D)

private:
	GameManager *game_manager = nullptr;
	PlayerInput *player_input = nullptr;
	Ref<VehicleConfig> config;
	bool debug_visuals_enabled = true;

	// Internal state
	CollisionShape3D *chassis_collider = nullptr;
	BoxShape3D *chassis_shape = nullptr;
	CSGBox3D *chassis_mesh = nullptr;

	// Debug visualizers
	std::vector<CSGSphere3D *> wheel_visuals;

	struct VehicleInputState {
		float throttle = 0.0f;
		float brake = 0.0f;
		float steer = 0.0f;
		bool handbrake = false;
		bool nitro = false;
	} current_input;

	// Flip state
	Vector3 current_com_offset;
	bool is_on_ramp = false;

	bool was_on_ramp = false;
	bool ramp_spin_active = false;
	bool ramp_roll_active = false;
	float last_roll_tilt = 0.0f;
	float ramp_roll_direction = 1.0f;

	// Physics integration variables
	Vector3 velocity_nudge_accumulator = Vector3(0.0f, 0.0f, 0.0f);
	bool should_align_velocity = false;

	// Drift state
	bool is_drifting = false;

	// Speed Boost state
	bool is_boosting = false;
	float boost_speed_bonus = 0.0f;
	float nitro_fuel = 0.0f;

	void _setup_vehicle();
	void _update_visuals();
	float _calculate_suspension_force(Ref<WheelConfig> wheel, float hit_distance, float delta, Vector3 hardpoint_world, Vector3 local_up);

	void _process_inputs();
	void _apply_acceleration(float delta);
	void _apply_steering(float delta);
	void _apply_lateral_friction(float delta);
	void _apply_stability(float delta);
	void _update_debug_arrows();

	float _get_average_contact_patch_y() const;
	void _apply_lateral_force_with_roll(Vector3 p_force_global);
	void _apply_longitudinal_force_with_pitch(Vector3 p_force_global);
	void _handle_wall_collision_and_spin(int p_wheel_index, const MCRaycastHit &p_hit, Vector3 &r_force_dir, float &r_force_mag);

	// HSM States
	friend class VehicleState;
	friend class GroundedState;
	friend class AirborneState;
	friend class DrivingState;
	friend class GlidingState;

	VehicleState *current_state = nullptr;
	GroundedState *grounded_state = nullptr;
	AirborneState *airborne_state = nullptr;
	DrivingState *driving_state = nullptr;
	GlidingState *gliding_state = nullptr;

protected:
	static void _bind_methods();

public:
	ArcadeVehicle();
	~ArcadeVehicle();

	void _ready() override;
	void _exit_tree() override;
	void _physics_process(double p_delta) override;
	void _integrate_forces(PhysicsDirectBodyState3D *state) override;

	void change_state(VehicleState *new_state);

	void set_config(const Ref<VehicleConfig> &p_config);
	Ref<VehicleConfig> get_config() const;

	void set_debug_visuals_enabled(bool p_enabled);
	bool get_debug_visuals_enabled() const;

	// Accessors for states
	VehicleInputState &get_input() { return current_input; }
	Ref<VehicleConfig> get_vehicle_config() { return config; }

	void set_game_manager(GameManager *p_gm) { game_manager = p_gm; }
	void set_player_input(PlayerInput *p_input) { player_input = p_input; }

	// UI Logic
	void _on_ui_toggle();
	void _on_ui_slider_value_changed(double p_value, String p_property);
	void save_settings();
	void load_settings();
	float get_ui_var(const String &p_name) const;
	void set_ui_var(const String &p_name, float p_value);

	void debug_draw_trajectory(float p_delta);

private:
	ArcadeVehicleUI *ui_helper = nullptr;
	CUI *ui_root = nullptr;
};

} // namespace godot

#endif // ARCADE_VEHICLE_H
