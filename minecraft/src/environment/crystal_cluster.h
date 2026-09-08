/**
 * @file crystal_cluster.h
 * @brief CrystalCluster: a MeshInstance3D showing a CrystalClusterMesh with the prop material and collision.
 */
#ifndef CRYSTAL_CLUSTER_H
#define CRYSTAL_CLUSTER_H

#include "crystal_cluster_mesh.h"
#include <godot_cpp/classes/collision_shape3d.hpp>
#include <godot_cpp/classes/material.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/shader_material.hpp>
#include <godot_cpp/classes/static_body3d.hpp>

namespace godot {

/**
 * @class CrystalCluster
 * @brief Thin scene node around CrystalClusterMesh: displays `crystal_mesh` (created on first use),
 * applies the shared prop material with `glow` / `transmission` unless `material` is set, and keeps a
 * StaticBody3D with the main crystal's convex hull in sync when `collision_enabled`. Internal children
 * and the generated mesh are never saved — only the mesh resource's parameters are.
 */
class CrystalCluster : public MeshInstance3D {
	GDCLASS(CrystalCluster,
			MeshInstance3D)

private:
	Ref<CrystalClusterMesh> crystal_mesh;
	Ref<Material> material;
	float glow = 0.6f; // Emission at the tips (prop shader `glow`)
	float transmission = 0.5f;
	bool collision_enabled = true;

	Ref<ShaderMaterial> _prop_material;
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
	CrystalCluster();
	~CrystalCluster();

	void set_crystal_mesh(const Ref<CrystalClusterMesh> &p_mesh);
	Ref<CrystalClusterMesh> get_crystal_mesh() const { return crystal_mesh; }
	void set_material(const Ref<Material> &p_material);
	Ref<Material> get_material() const { return material; }
	void set_glow(float p_glow);
	float get_glow() const { return glow; }
	void set_transmission(float p_amount);
	float get_transmission() const { return transmission; }
	void set_collision_enabled(bool p_enabled);
	bool get_collision_enabled() const { return collision_enabled; }

	void _on_crystal_mesh_changed();
};

} // namespace godot

#endif // CRYSTAL_CLUSTER_H
