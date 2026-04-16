#ifndef ARCADE_VEHICLE_H
#define ARCADE_VEHICLE_H

#include <godot_cpp/classes/rigid_body3d.hpp>
#include <godot_cpp/classes/collision_shape3d.hpp>
#include <godot_cpp/classes/box_shape3d.hpp>
#include <godot_cpp/classes/csg_box3d.hpp>
#include <godot_cpp/classes/csg_sphere3d.hpp>
#include "vehicle_config.h"

namespace godot {

class ArcadeVehicle : public RigidBody3D {
	GDCLASS(ArcadeVehicle, RigidBody3D)

private:
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

	void _setup_vehicle();
	void _update_visuals();
	float _calculate_suspension_force(Ref<WheelConfig> wheel, float hit_distance, float delta, Vector3 hardpoint_world, Vector3 local_up);
	
	void _process_inputs();
	void _apply_acceleration(float delta);
	void _apply_steering(float delta);
	void _apply_lateral_friction(float delta);
	void _apply_stability(float delta);

protected:
	static void _bind_methods();

public:
	ArcadeVehicle();
	~ArcadeVehicle();

	void _ready() override;
	void _physics_process(double p_delta) override;

	void set_config(const Ref<VehicleConfig> &p_config);
	Ref<VehicleConfig> get_config() const;

	void set_debug_visuals_enabled(bool p_enabled);
	bool get_debug_visuals_enabled() const;
};

} // namespace godot

#endif // ARCADE_VEHICLE_H
