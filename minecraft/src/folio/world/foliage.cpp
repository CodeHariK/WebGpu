#include "foliage.h"

#include <godot_cpp/classes/array_mesh.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/image_texture.hpp>
#include <godot_cpp/classes/mesh.hpp>
#include <godot_cpp/classes/multi_mesh.hpp>
#include <godot_cpp/classes/random_number_generator.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/math.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>
#include <godot_cpp/variant/packed_vector2_array.hpp>
#include <godot_cpp/variant/packed_vector3_array.hpp>

using namespace godot;

FolioFoliage::FolioFoliage() {}

FolioFoliage::~FolioFoliage() {}

// ~80 cross-quads scattered on a sphere, merged into one mesh (folio setGeometry).
void FolioFoliage::_build_cluster_mesh() {
	const double half = plane_size * 0.5;

	Ref<RandomNumberGenerator> rng;
	rng.instantiate();
	rng->set_seed(4242); // deterministic cluster (folio uses a fixed 'foliage' seed)

	PackedVector3Array verts;
	PackedVector3Array normals;
	PackedVector2Array uvs;
	PackedInt32Array indices;

	// Local quad corners (XY plane, facing +Z) + their UVs.
	const Vector3 corners[4] = { Vector3(-half, -half, 0), Vector3(half, -half, 0), Vector3(half, half, 0),
								 Vector3(-half, half, 0) };
	const Vector2 corner_uv[4] = { Vector2(0, 0), Vector2(1, 0), Vector2(1, 1), Vector2(0, 1) };

	for (int p = 0; p < planes_per_cluster; p++) {
		// Sphere position (three Spherical): denser toward the shell (1 - rng^3).
		const double radius = 1.0 - Math::pow((double)rng->randf(), 3.0);
		const double theta = Math::TAU * rng->randf();
		const double phi = Math::PI * rng->randf();
		const double sp = Math::sin(phi);
		const Vector3 pos(radius * sp * Math::sin(theta), radius * Math::cos(phi), radius * sp * Math::cos(theta));
		const Vector3 outward = pos.length() > 0.0001 ? pos.normalized() : Vector3(0, 1, 0);

		const double ang = rng->randf() * 6.28318530718;
		const double cs = Math::cos(ang);
		const double sn = Math::sin(ang);

		const int base = verts.size();
		for (int c = 0; c < 4; c++) {
			// rotateZ then translate to the sphere position.
			const Vector3 lc = corners[c];
			Vector3 v(lc.x * cs - lc.y * sn, lc.x * sn + lc.y * cs, 0.0);
			v += pos;
			verts.push_back(v);
			// Normal blended 85% toward the outward sphere direction (rounded blob).
			Vector3 n = v.lerp(outward, 0.85);
			normals.push_back(n.length() > 0.0001 ? n.normalized() : outward);
			uvs.push_back(corner_uv[c]);
		}
		indices.push_back(base + 0);
		indices.push_back(base + 1);
		indices.push_back(base + 2);
		indices.push_back(base + 0);
		indices.push_back(base + 2);
		indices.push_back(base + 3);
	}

	Array arrays;
	arrays.resize(Mesh::ARRAY_MAX);
	arrays[Mesh::ARRAY_VERTEX] = verts;
	arrays[Mesh::ARRAY_NORMAL] = normals;
	arrays[Mesh::ARRAY_TEX_UV] = uvs;
	arrays[Mesh::ARRAY_INDEX] = indices;

	Ref<ArrayMesh> am;
	am.instantiate();
	am->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, arrays);
	cluster_mesh = am;
}

void FolioFoliage::_build_material() {
	material.instantiate();
	Ref<Shader> shader = ResourceLoader::get_singleton()->load("res://material/shaders/folio/foliage.gdshader");
	if (shader.is_valid()) {
		material->set_shader(shader);
	}
	Ref<Image> tex = Image::load_from_file("res://material/textures/folio/foliage_sdf.png");
	if (tex.is_valid()) {
		material->set_shader_parameter("foliage_texture", ImageTexture::create_from_image(tex));
	}
	material->set_shader_parameter("color_a", color_a);
	material->set_shader_parameter("color_b", color_b);
	material->set_shader_parameter("has_water", false);
}

void FolioFoliage::scatter(const TypedArray<Transform3D> &p_transforms) {
	if (cluster_mesh.is_null()) {
		_build_cluster_mesh();
	}
	if (material.is_null()) {
		_build_material();
	}

	const int count = p_transforms.size();
	Ref<MultiMesh> mm;
	mm.instantiate();
	mm->set_transform_format(MultiMesh::TRANSFORM_3D);
	mm->set_mesh(cluster_mesh);
	mm->set_instance_count(count);
	for (int i = 0; i < count; i++) {
		mm->set_instance_transform(i, (Transform3D)p_transforms[i]);
	}

	if (!mmi) {
		mmi = memnew(MultiMeshInstance3D);
		mmi->set_name("FoliageInstances");
		add_child(mmi);
	}
	mmi->set_multimesh(mm);
	mmi->set_material_override(material);
}

void FolioFoliage::set_color_a(const Color &p_c) {
	color_a = p_c;
	if (material.is_valid()) {
		material->set_shader_parameter("color_a", color_a);
	}
}

void FolioFoliage::set_color_b(const Color &p_c) {
	color_b = p_c;
	if (material.is_valid()) {
		material->set_shader_parameter("color_b", color_b);
	}
}

void FolioFoliage::_bind_methods() {
	ClassDB::bind_method(D_METHOD("scatter", "transforms"), &FolioFoliage::scatter);
	ClassDB::bind_method(D_METHOD("set_color_a", "c"), &FolioFoliage::set_color_a);
	ClassDB::bind_method(D_METHOD("get_color_a"), &FolioFoliage::get_color_a);
	ClassDB::bind_method(D_METHOD("set_color_b", "c"), &FolioFoliage::set_color_b);
	ClassDB::bind_method(D_METHOD("get_color_b"), &FolioFoliage::get_color_b);
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "color_a"), "set_color_a", "get_color_a");
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "color_b"), "set_color_b", "get_color_b");
}
