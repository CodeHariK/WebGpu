#ifndef ARCADE_VEHICLE_H
#define ARCADE_VEHICLE_H

#include "config/vehicle_config.h"
#include <godot_cpp/classes/box_shape3d.hpp>
#include <godot_cpp/classes/collision_shape3d.hpp>
#include <godot_cpp/classes/csg_box3d.hpp>
#include <godot_cpp/classes/csg_sphere3d.hpp>
#include <godot_cpp/classes/rigid_body3d.hpp>

namespace godot {

class GameManager;
class VehicleState;
class GroundedState;
class AirborneState;
class DrivingState;
class StuntState;
class PlayerInput;

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
	} current_input;

	// Stunt / Flip state
	Vector3 current_com_offset;
	bool is_on_ramp = false;
	bool stunt_requested = false;
	bool in_stunt_rotation = false;


	void _setup_vehicle();
	void _update_visuals();
	float _calculate_suspension_force(Ref<WheelConfig> wheel, float hit_distance, float delta, Vector3 hardpoint_world, Vector3 local_up);

	void _process_inputs();
	void _apply_acceleration(float delta);
	void _apply_steering(float delta);
	void _apply_lateral_friction(float delta);
	void _apply_stability(float delta);
	void _update_debug_arrows();
	
	// HSM States
	friend class VehicleState;
	friend class GroundedState;
	friend class AirborneState;
	friend class DrivingState;
	friend class StuntState;

	VehicleState* current_state = nullptr;
	GroundedState* grounded_state = nullptr;
	AirborneState* airborne_state = nullptr;
	DrivingState* driving_state = nullptr;
	StuntState* stunt_state = nullptr;

protected:
	static void _bind_methods();

public:
	ArcadeVehicle();
	~ArcadeVehicle();

	void _ready() override;
	void _exit_tree() override;
	void _physics_process(double p_delta) override;

	void change_state(VehicleState* new_state);

	void set_config(const Ref<VehicleConfig> &p_config);
	Ref<VehicleConfig> get_config() const;

	void set_debug_visuals_enabled(bool p_enabled);
	bool get_debug_visuals_enabled() const;

	// Accessors for states
	VehicleInputState& get_input() { return current_input; }
	Ref<VehicleConfig> get_vehicle_config() { return config; }

	void set_game_manager(GameManager *p_gm) { game_manager = p_gm; }
	void set_player_input(PlayerInput *p_input) { player_input = p_input; }
};

} // namespace godot

#endif // ARCADE_VEHICLE_H
