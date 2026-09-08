/**
 * @file tree.h
 * @brief FoliageTree: a tree node — procedural trunk/shell mesh plus MultiMesh-instanced leaves and fruit.
 */
#ifndef TREE_H
#define TREE_H

#include "tree_mesh.h"
#include <godot_cpp/classes/collision_shape3d.hpp>
#include <godot_cpp/classes/material.hpp>
#include <godot_cpp/classes/mesh.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/multi_mesh_instance3d.hpp>
#include <godot_cpp/classes/shader_material.hpp>
#include <godot_cpp/classes/static_body3d.hpp>

namespace godot {

/**
 * @class FoliageTree
 * @brief Shows a FoliageTreeMesh (trunk + buttress roots + dark canopy shell) and scatters `leaf_mesh`
 * and `fruit_mesh` densely over the canopy as two internal MultiMeshInstance3D children — the "real
 * leaves" AC look, GPU-instanced. Per-instance colour carries the gradient / blossom mix, so a bare
 * leaf or quad mesh comes out correctly tinted. When a mesh slot is empty a placeholder (a quad leaf,
 * a sphere fruit) is used, so trees look right before the authored meshes exist; drop a Blender mesh
 * into `leaf_mesh` / `fruit_mesh` and nothing else changes. Foliage uses the shared prop material
 * (two-sided) unless `foliage_material` is set. Trunk keeps a cylinder collider. Nothing generated is
 * saved — only the parameters and the assigned mesh/material resources.
 */
class FoliageTree : public MeshInstance3D {
	GDCLASS(FoliageTree,
			MeshInstance3D)

private:
	Ref<FoliageTreeMesh> tree_mesh;
	Ref<Mesh> leaf_mesh; // Empty → placeholder quad
	Ref<Mesh> fruit_mesh; // Empty → placeholder sphere
	Ref<Material> trunk_material; // Empty → prop material
	Ref<Material> foliage_material; // Empty → two-sided prop material
	float glow = 0.25f;
	float transmission = 0.7f;
	bool collision_enabled = true;
	bool seed_from_position = false; // Re-seed the (unique) mesh from the world position; moved copies differ

	Ref<ShaderMaterial> _trunk_prop, _foliage_prop;
	int _applied_seed = 0x7fffffff;
	void _reseed_from_position();
	MultiMeshInstance3D *_leaves = nullptr;
	MultiMeshInstance3D *_fruit = nullptr;
	StaticBody3D *static_body = nullptr;
	CollisionShape3D *collision_shape = nullptr;

	void _apply_mesh();
	void _apply_materials();
	void _rebuild_instances();
	void _update_collision();
	Ref<Mesh> _placeholder_leaf();
	Ref<Mesh> _placeholder_fruit();
	MultiMeshInstance3D *_ensure_mm(
			MultiMeshInstance3D *&p_slot,
			const char *p_name
	);

protected:
	static void _bind_methods();
	void _notification(int p_what);
	void _validate_property(PropertyInfo &p_property) const;

public:
	FoliageTree();
	~FoliageTree();

	void set_tree_mesh(const Ref<FoliageTreeMesh> &p_mesh);
	Ref<FoliageTreeMesh> get_tree_mesh() const { return tree_mesh; }
	void set_leaf_mesh(const Ref<Mesh> &p_mesh);
	Ref<Mesh> get_leaf_mesh() const { return leaf_mesh; }
	void set_fruit_mesh(const Ref<Mesh> &p_mesh);
	Ref<Mesh> get_fruit_mesh() const { return fruit_mesh; }
	void set_trunk_material(const Ref<Material> &p_material);
	Ref<Material> get_trunk_material() const { return trunk_material; }
	void set_foliage_material(const Ref<Material> &p_material);
	Ref<Material> get_foliage_material() const { return foliage_material; }
	void set_glow(float p_glow);
	float get_glow() const { return glow; }
	void set_transmission(float p_amount);
	float get_transmission() const { return transmission; }
	void set_collision_enabled(bool p_enabled);
	bool get_collision_enabled() const { return collision_enabled; }
	void set_seed_from_position(bool p_enabled);
	bool get_seed_from_position() const { return seed_from_position; }

	void _on_tree_mesh_changed();
};

} // namespace godot

#endif // TREE_H
