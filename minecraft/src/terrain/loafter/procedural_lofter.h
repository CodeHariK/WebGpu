#ifndef PROCEDURAL_LOFTER_H
#define PROCEDURAL_LOFTER_H

#include <godot_cpp/classes/array_mesh.hpp>
#include <godot_cpp/classes/curve3d.hpp>
#include <godot_cpp/classes/material.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/path3d.hpp>
#include <godot_cpp/classes/resource.hpp>
#include <godot_cpp/variant/packed_vector2_array.hpp>
#include <godot_cpp/variant/transform3d.hpp>
#include <godot_cpp/variant/typed_array.hpp>
#include <vector>

namespace godot {

struct MeshData {
	PackedVector3Array vertices;
	PackedVector3Array normals;
	PackedInt32Array indices;
};

class ProceduralLofter : public Path3D {
	GDCLASS(ProceduralLofter, Path3D)

private:
	TypedArray<Curve3D> slices_array;
	Ref<Material> material;
	Ref<Curve3D> last_connected_curve;
	std::vector<Ref<Curve3D>> last_connected_slices;

	float blend_factor = 0.0f;
	int segments = 16;
	bool is_dirty = true;
	bool flat_shaded = false;

	MeshInstance3D *mesh_instance = nullptr;

public:
	virtual void update_loft();

protected:
	static void _bind_methods();

public:
	ProceduralLofter();
	~ProceduralLofter();

	virtual void _ready() override;
	virtual void _process(double delta) override;

	void set_slices_array(const TypedArray<Curve3D> &p_slices);
	TypedArray<Curve3D> get_slices_array() const;
	void set_material(const Ref<Material> &p_material);
	Ref<Material> get_material() const;
	void set_flat_shaded(bool p_flat);
	bool get_flat_shaded() const;
	void add_slice(Ref<Curve3D> p_slice, float p_position);
	void set_blend_factor(float p_factor);
	void set_segments(int p_segments);

	float get_blend_factor() const;
	int get_segments() const;

	// Helper that processes slices and transforms into an ArrayMesh
	Ref<ArrayMesh> generate_lofted_mesh(TypedArray<Curve3D> slices, TypedArray<Transform3D> transforms) const;

	// Generates a MeshInstance3D and attaches the generated ArrayMesh to it
	MeshInstance3D *bake() const;
};

} // namespace godot

#endif // PROCEDURAL_LOFTER_H
