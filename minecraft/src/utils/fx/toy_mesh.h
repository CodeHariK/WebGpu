#ifndef TOY_MESH_H
#define TOY_MESH_H

#include <godot_cpp/classes/box_mesh.hpp>
#include <godot_cpp/classes/capsule_mesh.hpp>
#include <godot_cpp/classes/cylinder_mesh.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/sphere_mesh.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>

namespace godot {

/**
 * ToyMesh — tiny helpers for building cartoon models out of primitives in code.
 * Low segment counts on purpose (mobile): the toon shading hides the facets.
 */
namespace ToyMesh {

/// Toon-shaded material with a soft rim light.
Ref<StandardMaterial3D> toon(const Color &p_color);
/// Unshaded, alpha-blended material (beams, glass, glows).
Ref<StandardMaterial3D> glow(const Color &p_color);

Ref<SphereMesh> sphere(
		float p_radius,
		int p_segments = 12
);
Ref<CylinderMesh> cylinder(
		float p_top_radius,
		float p_bottom_radius,
		float p_height,
		int p_segments = 10
);
Ref<BoxMesh> box(const Vector3 &p_size);
Ref<CapsuleMesh> capsule(
		float p_radius,
		float p_height
);

/// Adds a shadowless MeshInstance3D under `p_parent`.
MeshInstance3D *add(
		Node3D *p_parent,
		const Ref<Mesh> &p_mesh,
		const Ref<Material> &p_material,
		const Vector3 &p_position,
		const Vector3 &p_rotation = Vector3(),
		const Vector3 &p_scale = Vector3(1, 1, 1)
);

} // namespace ToyMesh

} // namespace godot

#endif // TOY_MESH_H
