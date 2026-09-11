/**
 * @file cliff.h
 * @brief Cliff: a MeshInstance3D showing a CliffMesh with prop material and concave collision.
 */
#ifndef CLIFF_H
#define CLIFF_H

#include "cliff_mesh.h"
#include <godot_cpp/classes/collision_shape3d.hpp>
#include <godot_cpp/classes/material.hpp>
#include <godot_cpp/classes/mesh.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/shader_material.hpp>
#include <godot_cpp/classes/static_body3d.hpp>

namespace godot {

/**
 * @class Cliff
 * @brief Scene node wrapping CliffMesh: automatically binds the matte stone shader material,
 * manages a StaticBody3D with a ConcavePolygonShape3D collider matching the displaced cliff geometry,
 * and supports optional position-based re-seeding.
 */
class Cliff : public MeshInstance3D {
	GDCLASS(Cliff, MeshInstance3D)

private:
	Ref<CliffMesh> cliff_mesh;
	Ref<Mesh> custom_mesh;
	Ref<Material> material;
	bool collision_enabled = true;
	bool seed_from_position = false;

	Ref<ShaderMaterial> _prop_material;
	int _applied_seed = 0x7fffffff;
	void _reseed_from_position();
	StaticBody3D *static_body = nullptr;
	CollisionShape3D *collision_shape = nullptr;

	void _apply_mesh();
	void _apply_material();
	void _update_collision();

protected:
	static void _bind_methods();
	void _notification(int p_what);
	void _validate_property(PropertyInfo &p_property) const;

public:
	Cliff();
	~Cliff();

	void set_cliff_mesh(const Ref<CliffMesh> &p_mesh);
	Ref<CliffMesh> get_cliff_mesh() const { return cliff_mesh; }
	void set_custom_mesh(const Ref<Mesh> &p_mesh);
	Ref<Mesh> get_custom_mesh() const { return custom_mesh; }
	void set_material(const Ref<Material> &p_material);
	Ref<Material> get_material() const { return material; }
	void set_collision_enabled(bool p_enabled);
	bool get_collision_enabled() const { return collision_enabled; }
	void set_seed_from_position(bool p_enabled);
	bool get_seed_from_position() const { return seed_from_position; }

	void _on_cliff_mesh_changed();
};

} // namespace godot

#endif // CLIFF_H
