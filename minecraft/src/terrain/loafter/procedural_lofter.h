#ifndef PROCEDURAL_LOFTER_H
#define PROCEDURAL_LOFTER_H

#include "utils/spline3d/procedural_spline3d.h"
#include <godot_cpp/classes/array_mesh.hpp>
#include <godot_cpp/classes/curve.hpp> // <-- Added for 1D Scaling Curve
#include <godot_cpp/classes/curve3d.hpp>
#include <godot_cpp/classes/material.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
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

class ProceduralLofter : public ProceduralSpline3D {
	GDCLASS(ProceduralLofter, ProceduralSpline3D)

private:
	TypedArray<Curve3D> slices_array;
	Ref<Material> material;
	std::vector<Ref<Curve3D>> last_connected_slices;

	// DEFORMATION PROPERTIES
	Ref<Curve> scale_curve;
	float wave_amplitude = 0.0f;
	float wave_frequency = 1.0f;

	float blend_factor = 0.0f;
	int slice_resolution = 16;
	bool mesh_dirty = true;
	bool flat_shaded = false;

	MeshInstance3D *mesh_instance = nullptr;
	Ref<Curve3D> last_connected_curve;
	void ensure_curve_connection();

public:
	virtual void update_loft();
	void _on_parent_spline_changed();

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
	float get_blend_factor() const;

	// DEFORMATION GETTERS/SETTERS
	void set_scale_curve(const Ref<Curve> &p_curve);
	Ref<Curve> get_scale_curve() const;
	void set_wave_amplitude(float p_amp);
	float get_wave_amplitude() const;
	void set_wave_frequency(float p_freq);
	float get_wave_frequency() const;

	void set_slice_resolution(int p_res) {
		slice_resolution = MAX(3, p_res);
		mesh_dirty = true;
		update_loft();
	}
	int get_slice_resolution() const { return slice_resolution; }

	Ref<ArrayMesh> generate_lofted_mesh(const std::vector<PackedVector3Array> &rings, const std::vector<Transform3D> &transforms) const;
	MeshInstance3D *bake() const;
};

} // namespace godot

#endif // PROCEDURAL_LOFTER_H
