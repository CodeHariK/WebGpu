#ifndef FOLIO_FLOWERS_H
#define FOLIO_FLOWERS_H

#include <godot_cpp/classes/mesh.hpp>
#include <godot_cpp/classes/multi_mesh_instance3d.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/shader_material.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/transform3d.hpp>
#include <godot_cpp/variant/typed_array.hpp>

namespace godot {

/**
 * Folio port — FolioFlowers  (folio `World/Flowers.js`)
 * -----------------------------------------------
 * Tiny flower tufts (a few small solid-colour quads per cluster) instanced on the
 * grass, upper verts wind-swayed via `flowers.gdshader`. `scatter(transforms)`
 * instances the tuft in one MultiMesh draw.
 */
class FolioFlowers : public Node3D {
	GDCLASS(FolioFlowers,
			Node3D)

private:
	Color flower_color = Color(0.95f, 0.93f, 0.98f);
	int quads_per_tuft = 6;
	double quad_size = 0.12;

	Ref<Mesh> tuft_mesh;
	Ref<ShaderMaterial> material;
	MultiMeshInstance3D *mmi = nullptr;

	void _build_tuft_mesh();
	void _build_material();

protected:
	static void _bind_methods();

public:
	FolioFlowers();
	~FolioFlowers();

	void set_flower_color(const Color &p_c);
	Color get_flower_color() const { return flower_color; }

	void scatter(const TypedArray<Transform3D> &p_transforms);
};

} // namespace godot

#endif // FOLIO_FLOWERS_H
