#ifndef VEHICLE_PROP_H
#define VEHICLE_PROP_H

#include <godot_cpp/classes/static_body3d.hpp>
#include <godot_cpp/classes/mesh.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/collision_shape3d.hpp>
#include <godot_cpp/classes/concave_polygon_shape3d.hpp>

namespace godot {

class VehicleProp : public StaticBody3D {
	GDCLASS(VehicleProp, StaticBody3D)

private:
	Ref<Mesh> mesh;
	MeshInstance3D *mesh_instance = nullptr;
	CollisionShape3D *collision_shape = nullptr;

	void _update_prop();

protected:
	static void _bind_methods();

public:
	VehicleProp();
	~VehicleProp();

	void _ready() override;

	void set_mesh(const Ref<Mesh> &p_mesh);
	Ref<Mesh> get_mesh() const;
};

} // namespace godot

#endif // VEHICLE_PROP_H
