#ifndef FOLIO_FOLIAGE_H
#define FOLIO_FOLIAGE_H

#include <godot_cpp/classes/mesh.hpp>
#include <godot_cpp/classes/multi_mesh_instance3d.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/shader_material.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/transform3d.hpp>
#include <godot_cpp/variant/typed_array.hpp>

namespace godot {

/**
 * Folio port — FolioFoliage  (folio `World/Foliage.js`)
 * -----------------------------------------------
 * A leaf-cluster billboard cloud used for bushes / tree crowns. The cluster mesh
 * is ~80 small cross-quads scattered on a sphere (normals blended outward so it
 * shades like a rounded blob), built once on the CPU. Each quad alpha-cuts a leaf
 * shape from a shared mask; `foliage.gdshader` 2-tones by sun facing and shimmers
 * the leaf UVs with the wind field. `scatter(transforms)` instances the cluster
 * (one MultiMesh draw) at the given transforms.
 *
 * Deferred vs folio: the near-vehicle see-through fade and per-cluster camera
 * facing.
 */
class FolioFoliage : public Node3D {
	GDCLASS(FolioFoliage,
			Node3D)

private:
	Color color_a = Color(0.706f, 0.71f, 0.212f); // #b4b536
	Color color_b = Color(0.847f, 0.812f, 0.231f); // #d8cf3b
	int planes_per_cluster = 80;
	double plane_size = 0.8;

	Ref<Mesh> cluster_mesh;
	Ref<ShaderMaterial> material;
	MultiMeshInstance3D *mmi = nullptr;

	void _build_cluster_mesh();
	void _build_material();

protected:
	static void _bind_methods();

public:
	FolioFoliage();
	~FolioFoliage();

	void set_color_a(const Color &p_c);
	Color get_color_a() const { return color_a; }
	void set_color_b(const Color &p_c);
	Color get_color_b() const { return color_b; }

	// Instance the leaf cluster at each transform (one MultiMesh draw).
	void scatter(const TypedArray<Transform3D> &p_transforms);
};

} // namespace godot

#endif // FOLIO_FOLIAGE_H
