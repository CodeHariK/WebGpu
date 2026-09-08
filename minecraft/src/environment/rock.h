/**
 * @file rock.h
 * @brief Rock: a MeshInstance3D showing a RockMesh (or an authored mesh) with the prop material and collision.
 */
#ifndef ROCK_H
#define ROCK_H

#include "rock_mesh.h"
#include <godot_cpp/classes/collision_shape3d.hpp>
#include <godot_cpp/classes/material.hpp>
#include <godot_cpp/classes/mesh.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/shader_material.hpp>
#include <godot_cpp/classes/static_body3d.hpp>
#include <vector>

namespace godot {

/**
 * @class Rock
 * @brief Thin scene node around RockMesh: displays `rock_mesh` (created on first use) with the shared
 * prop material tuned matte (unless `material` is set) and keeps one convex collider per blob in sync.
 * `custom_mesh` swaps in a Blender-authored rock (its own convex hull for collision) while keeping the
 * node, material and placement identical — the same authored-or-procedural split as the trees.
 * Internal children and the generated mesh are never saved.
 */
class Rock : public MeshInstance3D {
	GDCLASS(Rock,
			MeshInstance3D)

private:
	Ref<RockMesh> rock_mesh;
	Ref<Mesh> custom_mesh;
	Ref<Material> material;
	bool collision_enabled = true;
	bool seed_from_position = false; // Re-seed the (unique) mesh from the world position; moved copies differ

	Ref<ShaderMaterial> _prop_material;
	int _applied_seed = 0x7fffffff;
	void _reseed_from_position();
	StaticBody3D *static_body = nullptr;
	std::vector<CollisionShape3D *> _shapes;

	void _apply_mesh();
	void _apply_material();
	void _update_collision();

protected:
	static void _bind_methods();
	void _notification(int p_what);
	void _validate_property(PropertyInfo &p_property) const;

public:
	Rock();
	~Rock();

	void set_rock_mesh(const Ref<RockMesh> &p_mesh);
	Ref<RockMesh> get_rock_mesh() const { return rock_mesh; }
	void set_custom_mesh(const Ref<Mesh> &p_mesh);
	Ref<Mesh> get_custom_mesh() const { return custom_mesh; }
	void set_material(const Ref<Material> &p_material);
	Ref<Material> get_material() const { return material; }
	void set_collision_enabled(bool p_enabled);
	bool get_collision_enabled() const { return collision_enabled; }
	void set_seed_from_position(bool p_enabled);
	bool get_seed_from_position() const { return seed_from_position; }

	void _on_rock_mesh_changed();
};

} // namespace godot

#endif // ROCK_H
