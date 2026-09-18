#include "grass.h"

#include <godot_cpp/classes/array_mesh.hpp>
#include <godot_cpp/classes/mesh.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/packed_vector2_array.hpp>
#include <godot_cpp/variant/packed_vector3_array.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include "../game.h"
#include "../quality.h"
#include "../ticker.h"
#include "../view/view.h"

using namespace godot;

FolioGrass::FolioGrass() {}

FolioGrass::~FolioGrass() {}

void FolioGrass::_ready() {
	_build();
	FolioTicker *ticker = FolioTicker::get_singleton();
	if (ticker) {
		ticker->connect_tick(callable_mp(this, &FolioGrass::update), 10);
	}
}

void FolioGrass::_build() {
	// Blade soup: subdiv² blades, 3 verts each. Each blade's ground XZ is shared
	// by its 3 verts (y=0); per-vertex height randomness rides in UV.x. The loop
	// index (tip/left/right) is derived in-shader from VERTEX_ID % 3.
	int subdiv = subdivisions;
	if (scale_with_quality) {
		FolioGame *game = FolioGame::get_singleton();
		if (game && game->get_quality() && game->get_quality()->get_level() >= 1) {
			subdiv = (int)((double)subdivisions * 0.6); // low tier: ~36% fewer blades
		}
	}
	if (subdiv < 1) {
		subdiv = 1;
	}
	const int blades = subdiv * subdiv;
	const double fragment_size = size / (double)subdiv;

	PackedVector3Array verts;
	PackedVector2Array uvs;
	verts.resize(blades * 3);
	uvs.resize(blades * 3);

	int v = 0;
	for (int ix = 0; ix < subdiv; ix++) {
		const double fx = ((double)ix / (double)subdiv - 0.5) * size + fragment_size * 0.5;
		for (int iz = 0; iz < subdiv; iz++) {
			const double fz = ((double)iz / (double)subdiv - 0.5) * size + fragment_size * 0.5;
			const double px = fx + (UtilityFunctions::randf() - 0.5) * fragment_size;
			const double pz = fz + (UtilityFunctions::randf() - 0.5) * fragment_size;
			const Vector3 ground((float)px, 0.0f, (float)pz);
			for (int k = 0; k < 3; k++) {
				verts[v] = ground;
				uvs[v] = Vector2((float)UtilityFunctions::randf(), 0.0f); // heightRandomness
				v++;
			}
		}
	}

	Array arrays;
	arrays.resize(Mesh::ARRAY_MAX);
	arrays[Mesh::ARRAY_VERTEX] = verts;
	arrays[Mesh::ARRAY_TEX_UV] = uvs;

	Ref<ArrayMesh> array_mesh;
	array_mesh.instantiate();
	array_mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, arrays);

	material.instantiate();
	Ref<Shader> shader = ResourceLoader::get_singleton()->load("res://material/shaders/folio/grass.gdshader");
	if (shader.is_valid()) {
		material->set_shader(shader);
	}
	material->set_shader_parameter("has_light_bounce", false);
	material->set_shader_parameter("has_water", false);
	material->set_shader_parameter("grass_size", size);
	material->set_shader_parameter("blade_width", blade_width);
	material->set_shader_parameter("blade_height", blade_height);

	mesh = memnew(MeshInstance3D);
	mesh->set_name("GrassMesh");
	mesh->set_mesh(array_mesh);
	mesh->set_material_override(material);
	// Follows the camera + hides blades by pushing them up, so never frustum-cull.
	mesh->set_custom_aabb(AABB(Vector3(-1e5, -1e5, -1e5), Vector3(2e5, 2e5, 2e5)));
	add_child(mesh);
}

void FolioGrass::update() {
	if (material.is_null()) {
		return;
	}
	FolioGame *game = FolioGame::get_singleton();
	if (game && game->get_view()) {
		const Vector3 c = game->get_view()->get_optimal_area_position();
		material->set_shader_parameter("grass_center", Vector2(c.x, c.z));
	}
}

void FolioGrass::_rebuild() {
	if (mesh) {
		mesh->queue_free();
		mesh = nullptr;
	}
	_build();
}

void FolioGrass::set_subdivisions(int p_v) {
	subdivisions = p_v < 1 ? 1 : p_v;
	if (is_inside_tree()) {
		_rebuild();
	}
}

void FolioGrass::set_field_size(double p_v) {
	size = p_v;
	if (is_inside_tree()) {
		_rebuild();
	}
}

void FolioGrass::set_blade_width(double p_v) {
	blade_width = p_v;
	if (material.is_valid()) {
		material->set_shader_parameter("blade_width", blade_width);
	}
}

void FolioGrass::set_blade_height(double p_v) {
	blade_height = p_v;
	if (material.is_valid()) {
		material->set_shader_parameter("blade_height", blade_height);
	}
}

void FolioGrass::_bind_methods() {
	ClassDB::bind_method(D_METHOD("update"), &FolioGrass::update);

	ClassDB::bind_method(D_METHOD("set_subdivisions", "v"), &FolioGrass::set_subdivisions);
	ClassDB::bind_method(D_METHOD("get_subdivisions"), &FolioGrass::get_subdivisions);
	ClassDB::bind_method(D_METHOD("set_field_size", "v"), &FolioGrass::set_field_size);
	ClassDB::bind_method(D_METHOD("get_field_size"), &FolioGrass::get_field_size);
	ClassDB::bind_method(D_METHOD("set_blade_width", "v"), &FolioGrass::set_blade_width);
	ClassDB::bind_method(D_METHOD("get_blade_width"), &FolioGrass::get_blade_width);
	ClassDB::bind_method(D_METHOD("set_blade_height", "v"), &FolioGrass::set_blade_height);
	ClassDB::bind_method(D_METHOD("get_blade_height"), &FolioGrass::get_blade_height);
	ClassDB::bind_method(D_METHOD("set_scale_with_quality", "v"), &FolioGrass::set_scale_with_quality);
	ClassDB::bind_method(D_METHOD("get_scale_with_quality"), &FolioGrass::get_scale_with_quality);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "subdivisions"), "set_subdivisions", "get_subdivisions");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "field_size"), "set_field_size", "get_field_size");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "blade_width"), "set_blade_width", "get_blade_width");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "blade_height"), "set_blade_height", "get_blade_height");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "scale_with_quality"), "set_scale_with_quality", "get_scale_with_quality");
}
