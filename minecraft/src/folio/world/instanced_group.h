#ifndef FOLIO_INSTANCED_GROUP_H
#define FOLIO_INSTANCED_GROUP_H

#include <godot_cpp/classes/mesh.hpp>
#include <godot_cpp/classes/multi_mesh_instance3d.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/variant/typed_array.hpp>

namespace godot {

/**
 * Folio port — FolioInstancedGroup  (folio `Game/InstancedGroup.js`)
 * -----------------------------------------------
 * Reusable GPU-instancing helper: render one mesh at many transforms in a single
 * draw. folio builds a THREE.InstancedMesh per mesh in a source group; the Godot
 * equivalent is a `MultiMesh` (one draw call for all instances). This wraps that:
 * `build(mesh, transforms)` creates a `MultiMeshInstance3D` child with the mesh
 * placed at every transform. It is the foundation for Trees / Bushes / Flowers.
 *
 * Multi-surface source models (folio traverses a group of meshes) are deferred —
 * this handles a single mesh; call build() once per source mesh for a group.
 */
class FolioInstancedGroup : public Node3D {
	GDCLASS(FolioInstancedGroup,
			Node3D)

private:
	MultiMeshInstance3D *mmi = nullptr;

protected:
	static void _bind_methods();

public:
	FolioInstancedGroup();
	~FolioInstancedGroup();

	// Place `mesh` at each transform via one MultiMesh (one draw call).
	void
	build(const Ref<Mesh> &p_mesh,
		  const TypedArray<Transform3D> &p_transforms);
	int get_count() const;
};

} // namespace godot

#endif // FOLIO_INSTANCED_GROUP_H
