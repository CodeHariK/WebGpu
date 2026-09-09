/**
 * @file stylized_terrain.h
 * @brief StylizedTerrain: a MeshInstance3D showing a StylizedTerrainMesh with the toon material + collision.
 */
#ifndef STYLIZED_TERRAIN_H
#define STYLIZED_TERRAIN_H

#include "stylized_terrain_mesh.h"
#include <godot_cpp/classes/collision_shape3d.hpp>
#include <godot_cpp/classes/material.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/shader_material.hpp>
#include <godot_cpp/classes/static_body3d.hpp>

namespace godot {

/**
 * @class StylizedTerrain
 * @brief Thin scene node around StylizedTerrainMesh: displays `terrain_mesh` (created on first use) with
 * the shared toon solid material (so the per-band vertex colours light with no textures) and keeps one
 * trimesh collider in sync. `material` overrides the default. The generated mesh and material are never
 * saved into the scene. This is the self-generated alternative to a Terrain3D region: full control over
 * the terraced / faceted look, no addon in the middle.
 */
class StylizedTerrain : public MeshInstance3D {
	GDCLASS(StylizedTerrain,
			MeshInstance3D)

private:
	Ref<StylizedTerrainMesh> terrain_mesh;
	Ref<Material> material;
	bool collision_enabled = true;

	Ref<ShaderMaterial> _toon_material;
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
	StylizedTerrain();
	~StylizedTerrain();

	void set_terrain_mesh(const Ref<StylizedTerrainMesh> &p_mesh);
	Ref<StylizedTerrainMesh> get_terrain_mesh() const { return terrain_mesh; }
	void set_material(const Ref<Material> &p_material);
	Ref<Material> get_material() const { return material; }
	void set_collision_enabled(bool p_enabled);
	bool get_collision_enabled() const { return collision_enabled; }

	void _on_terrain_mesh_changed();
};

} // namespace godot

#endif // STYLIZED_TERRAIN_H
