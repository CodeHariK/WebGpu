#include "toy_mesh.h"

#include <godot_cpp/classes/base_material3d.hpp>
#include <godot_cpp/classes/geometry_instance3d.hpp>

namespace godot {

namespace ToyMesh {

Ref<StandardMaterial3D> toon(const Color &p_color) {
	Ref<StandardMaterial3D> m;
	m.instantiate();
	m->set_albedo(p_color);
	m->set_diffuse_mode(BaseMaterial3D::DIFFUSE_TOON);
	m->set_specular_mode(BaseMaterial3D::SPECULAR_TOON);
	m->set_roughness(0.5f);
	m->set_feature(BaseMaterial3D::FEATURE_RIM, true);
	m->set_rim(0.4f);
	return m;
}

Ref<StandardMaterial3D> glow(const Color &p_color) {
	Ref<StandardMaterial3D> m;
	m.instantiate();
	m->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
	m->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
	m->set_albedo(p_color);
	return m;
}

Ref<SphereMesh> sphere(
		float p_radius,
		int p_segments
) {
	Ref<SphereMesh> m;
	m.instantiate();
	m->set_radius(p_radius);
	m->set_height(p_radius * 2.0f);
	m->set_radial_segments(p_segments);
	m->set_rings(MAX(3, p_segments / 2));
	return m;
}

Ref<CylinderMesh> cylinder(
		float p_top_radius,
		float p_bottom_radius,
		float p_height,
		int p_segments
) {
	Ref<CylinderMesh> m;
	m.instantiate();
	m->set_top_radius(p_top_radius);
	m->set_bottom_radius(p_bottom_radius);
	m->set_height(p_height);
	m->set_radial_segments(p_segments);
	m->set_rings(1);
	return m;
}

Ref<BoxMesh> box(const Vector3 &p_size) {
	Ref<BoxMesh> m;
	m.instantiate();
	m->set_size(p_size);
	return m;
}

Ref<CapsuleMesh> capsule(
		float p_radius,
		float p_height
) {
	Ref<CapsuleMesh> m;
	m.instantiate();
	m->set_radius(p_radius);
	m->set_height(p_height);
	m->set_radial_segments(12);
	m->set_rings(4);
	return m;
}

MeshInstance3D *add(
		Node3D *p_parent,
		const Ref<Mesh> &p_mesh,
		const Ref<Material> &p_material,
		const Vector3 &p_position,
		const Vector3 &p_rotation,
		const Vector3 &p_scale
) {
	MeshInstance3D *mi = memnew(MeshInstance3D);
	mi->set_mesh(p_mesh);
	mi->set_material_override(p_material);
	mi->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
	mi->set_position(p_position);
	mi->set_rotation(p_rotation);
	mi->set_scale(p_scale);
	p_parent->add_child(mi);
	return mi;
}

} // namespace ToyMesh

} // namespace godot
