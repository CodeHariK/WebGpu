/**
 * @file model_vehicle_visual.h
 * @brief ModelVehicleVisual: a car model (any glTF/glb scene) as an ArcadeVehicle's look. Put it
 * under the ArcadeVehicle. On its first frame (once the vehicle has loaded its preset) it instances
 * the model, turned (model_yaw_degrees) so its front faces −z (the vehicle's forward) and scaled
 * (model_scale); then it gathers the model's wheel meshes (those whose names contain one of
 * wheel_parts), gives each to the nearest physics wheel (the WheelConfig hardpoints), and hangs them
 * on a pivot at that wheel's middle. It lifts the body so the wheels sit where the suspension
 * settles under the car's weight. Every frame each wheel drops by its suspension's travel, rolls
 * with the car's speed, and the front ones (hardpoint z < 0) steer. With body_collider, a box round
 * the body is added to the vehicle, so a long car hits walls with its nose and tail, not just the
 * vehicle's chassis sphere.
 */
#ifndef MODEL_VEHICLE_VISUAL_H
#define MODEL_VEHICLE_VISUAL_H

#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/packed_scene.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <vector>

namespace godot {

class ArcadeVehicle;

class ModelVehicleVisual : public Node3D {
	GDCLASS(ModelVehicleVisual,
			Node3D)

private:
	struct Wheel {
		Node3D *pivot = nullptr;
		Vector3 middle; // where the wheel's middle is at rest (vehicle space)
		int physics = 0; // its WheelConfig
		float rest = 0.0f; // how far below its hardpoint the wheel hangs with the car standing still
		bool front = false;
	};

	Ref<PackedScene> model;
	float model_scale = 1.0f;
	float model_yaw_degrees = 180.0f; // glTF cars usually face +z
	PackedStringArray wheel_parts;
	float max_steer_degrees = 35.0f; // how far the front wheels turn at full lock
	bool body_collider = true;

	bool built = false;
	ArcadeVehicle *vehicle = nullptr;
	std::vector<Wheel> wheels;
	float radius = 0.4f; // metres: the physics wheels' radius, for rolling
	float spin = 0.0f; // radians the wheels have rolled

	void _build();
	bool _is_wheel_part(const String &p_name) const;
	void _add_body_collider(
			Node3D *p_body,
			const std::vector<MeshInstance3D *> &p_meshes
	);

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	void set_model(const Ref<PackedScene> &p_model);
	Ref<PackedScene> get_model() const;
	void set_model_scale(float p_scale);
	float get_model_scale() const;
	void set_model_yaw_degrees(float p_degrees);
	float get_model_yaw_degrees() const;
	void set_wheel_parts(const PackedStringArray &p_parts);
	PackedStringArray get_wheel_parts() const;
	void set_max_steer_degrees(float p_degrees);
	float get_max_steer_degrees() const;
	void set_body_collider(bool p_enabled);
	bool get_body_collider() const;

	void _process(double p_delta) override;
};

} // namespace godot

#endif // MODEL_VEHICLE_VISUAL_H
