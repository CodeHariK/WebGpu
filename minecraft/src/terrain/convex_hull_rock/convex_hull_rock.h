/**
 * @file convex_hull_rock.h
 * @brief ConvexHullRock: a MeshInstance3D showing a ConvexHullRockMesh with optional static collision.
 */
#ifndef CONVEX_HULL_ROCK_H
#define CONVEX_HULL_ROCK_H

#include "convex_hull_rock_mesh.h"
#include <godot_cpp/classes/collision_shape3d.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/static_body3d.hpp>

namespace godot {

/**
 * @class ConvexHullRock
 * @brief Thin scene node around ConvexHullRockMesh. Assign or create the rock in `rock_mesh`; the node
 * displays it and, when `collision_enabled`, keeps a StaticBody3D/CollisionShape3D child in sync with
 * the mesh's convex hull. The helper nodes are not saved with the scene, and neither is the generated
 * mesh - only `rock_mesh`'s parameters are.
 */
class ConvexHullRock : public MeshInstance3D {
	GDCLASS(ConvexHullRock,
			MeshInstance3D)

private:
	Ref<ConvexHullRockMesh> rock_mesh;
	bool collision_enabled = true;
	StaticBody3D *static_body = nullptr;
	CollisionShape3D *collision_shape = nullptr;

	void _apply_mesh();
	void _update_collision();
	void _ensure_collision_nodes();
	void _free_collision_nodes();

protected:
	static void _bind_methods();
	void _notification(int p_what);
	void _validate_property(PropertyInfo &p_property) const;

public:
	ConvexHullRock();
	~ConvexHullRock();

	void set_rock_mesh(const Ref<ConvexHullRockMesh> &p_mesh);
	Ref<ConvexHullRockMesh> get_rock_mesh() const { return rock_mesh; }
	void set_collision_enabled(bool p_enabled);
	bool get_collision_enabled() const { return collision_enabled; }

	/// Called when rock_mesh changes its geometry.
	void _on_rock_mesh_changed();
};

} // namespace godot

#endif // CONVEX_HULL_ROCK_H
