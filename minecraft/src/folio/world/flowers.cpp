#include "flowers.h"

#include <godot_cpp/classes/array_mesh.hpp>
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

FolioFlowers::FolioFlowers() {}

FolioFlowers::~FolioFlowers() {}

// A small tuft: a few tiny upright quads in a little cluster (folio setGeometry).
void FolioFlowers::_build_tuft_mesh() {
	const double half = quad_size * 0.5;

	Ref<RandomNumberGenerator> rng;
	rng.instantiate();
	rng->set_seed(777);

	PackedVector3Array verts;
	PackedVector3Array normals;
	PackedVector2Array uvs;
	PackedInt32Array indices;

	const Vector2 corner_uv[4] = { Vector2(0, 0), Vector2(1, 0), Vector2(1, 1), Vector2(0, 1) };

	for (int p = 0; p < quads_per_tuft; p++) {
		const double radius = 0.10 + rng->randf() * 0.14;
		const double theta = Math::TAU * rng->randf();
		const Vector3 offset(Math::cos(theta) * radius, 0.06 + rng->randf() * 0.28, Math::sin(theta) * radius);

		// Upright quad in the XY plane, yawed randomly.
		const double yaw = Math::TAU * rng->randf();
		const double cy = Math::cos(yaw);
		const double sy = Math::sin(yaw);
		const Vector3 corners[4] = { Vector3(-half, -half, 0), Vector3(half, -half, 0), Vector3(half, half, 0),
									 Vector3(-half, half, 0) };

		const int base = verts.size();
		for (int c = 0; c < 4; c++) {
			const Vector3 lc = corners[c];
			Vector3 v(lc.x * cy, lc.y, lc.x * sy); // yaw about Y (quad stands up)
			v += offset;
			verts.push_back(v);
			normals.push_back(Vector3(0, 1, 0)); // stylized: lit from above
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
	tuft_mesh = am;
}

void FolioFlowers::_build_material() {
	material.instantiate();
	Ref<Shader> shader = ResourceLoader::get_singleton()->load("res://material/shaders/folio/flowers.gdshader");
	if (shader.is_valid()) {
		material->set_shader(shader);
	}
	material->set_shader_parameter("flower_color", flower_color);
	material->set_shader_parameter("has_light_bounce", false);
	material->set_shader_parameter("has_water", false);
}

void FolioFlowers::scatter(const TypedArray<Transform3D> &p_transforms) {
	if (tuft_mesh.is_null()) {
		_build_tuft_mesh();
	}
	if (material.is_null()) {
		_build_material();
	}
	const int count = p_transforms.size();
	Ref<MultiMesh> mm;
	mm.instantiate();
	mm->set_transform_format(MultiMesh::TRANSFORM_3D);
	mm->set_mesh(tuft_mesh);
	mm->set_instance_count(count);
	for (int i = 0; i < count; i++) {
		mm->set_instance_transform(i, (Transform3D)p_transforms[i]);
	}
	if (!mmi) {
		mmi = memnew(MultiMeshInstance3D);
		mmi->set_name("FlowerInstances");
		add_child(mmi);
	}
	mmi->set_multimesh(mm);
	mmi->set_material_override(material);
}

void FolioFlowers::set_flower_color(const Color &p_c) {
	flower_color = p_c;
	if (material.is_valid()) {
		material->set_shader_parameter("flower_color", flower_color);
	}
}

void FolioFlowers::_bind_methods() {
	ClassDB::bind_method(D_METHOD("scatter", "transforms"), &FolioFlowers::scatter);
	ClassDB::bind_method(D_METHOD("set_flower_color", "c"), &FolioFlowers::set_flower_color);
	ClassDB::bind_method(D_METHOD("get_flower_color"), &FolioFlowers::get_flower_color);
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "flower_color"), "set_flower_color", "get_flower_color");
}
