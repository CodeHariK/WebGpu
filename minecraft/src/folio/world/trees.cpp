#include "trees.h"

#include <godot_cpp/classes/collision_shape3d.hpp>
#include <godot_cpp/classes/cylinder_mesh.hpp>
#include <godot_cpp/classes/cylinder_shape3d.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/static_body3d.hpp>
#include <godot_cpp/core/class_db.hpp>

using namespace godot;

FolioTrees::FolioTrees() {}

FolioTrees::~FolioTrees() {}

void FolioTrees::_build_trunk_mesh() {
	Ref<CylinderMesh> cyl;
	cyl.instantiate();
	cyl->set_top_radius((float)(trunk_radius * 0.5)); // tapered
	cyl->set_bottom_radius((float)trunk_radius);
	cyl->set_height((float)trunk_height);
	cyl->set_radial_segments(7);
	cyl->set_rings(1);

	Ref<ShaderMaterial> mat;
	mat.instantiate();
	Ref<Shader> shader = ResourceLoader::get_singleton()->load("res://material/shaders/folio/mesh_default.gdshader");
	if (shader.is_valid()) {
		mat->set_shader(shader);
	}
	mat->set_shader_parameter("base_color", trunk_color);
	cyl->set_material(mat);

	trunk_mesh = cyl;
}

void FolioTrees::scatter(const TypedArray<Transform3D> &p_transforms) {
	if (trunk_mesh.is_null()) {
		_build_trunk_mesh();
	}
	if (!trunks) {
		trunks = memnew(FolioInstancedGroup);
		trunks->set_name("Trunks");
		add_child(trunks);
	}
	if (!crowns) {
		crowns = memnew(FolioFoliage);
		crowns->set_name("Crowns");
		crowns->set_color_a(crown_color_a);
		crowns->set_color_b(crown_color_b);
		add_child(crowns);
	}

	// Trunks: place the cylinder so its base sits on the ground (cylinder origin
	// is its centre, so lift by half the height along each tree's up axis).
	TypedArray<Transform3D> trunk_transforms;
	TypedArray<Transform3D> crown_transforms;
	const int count = p_transforms.size();
	for (int i = 0; i < count; i++) {
		const Transform3D t = (Transform3D)p_transforms[i];
		const Vector3 up = t.basis.get_column(1).normalized();

		Transform3D trunk_t = t;
		trunk_t.origin += up * (trunk_height * 0.5);
		trunk_transforms.push_back(trunk_t);

		Transform3D crown_t = t;
		crown_t.origin += up * trunk_height;
		crown_t.basis = crown_t.basis.scaled(Vector3((float)crown_scale, (float)crown_scale, (float)crown_scale));
		crown_transforms.push_back(crown_t);
	}

	trunks->build(trunk_mesh, trunk_transforms);
	crowns->scatter(crown_transforms);

	// Static trunk colliders (folio adds a cylinder body per tree).
	if (collide) {
		StaticBody3D *bodies = memnew(StaticBody3D);
		bodies->set_name("TrunkColliders");
		add_child(bodies);
		for (int i = 0; i < trunk_transforms.size(); i++) {
			Ref<CylinderShape3D> shape;
			shape.instantiate();
			shape->set_radius((float)trunk_radius);
			shape->set_height((float)trunk_height);
			CollisionShape3D *cs = memnew(CollisionShape3D);
			cs->set_shape(shape);
			cs->set_transform((Transform3D)trunk_transforms[i]);
			bodies->add_child(cs);
		}
	}
}

void FolioTrees::_bind_methods() { ClassDB::bind_method(D_METHOD("scatter", "transforms"), &FolioTrees::scatter); }
