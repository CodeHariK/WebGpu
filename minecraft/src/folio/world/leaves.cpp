#include "leaves.h"

#include <godot_cpp/classes/mesh.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/math.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>
#include <godot_cpp/variant/packed_vector2_array.hpp>
#include <godot_cpp/variant/packed_vector3_array.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include "../game.h"
#include "../ticker.h"
#include "../view/view.h"
#include "../cycles/year_cycles.h"

using namespace godot;

FolioLeaves::FolioLeaves() {}
FolioLeaves::~FolioLeaves() {}

// `count` leaf quads. Each vertex carries: VERTEX.xz = leaf origin in [0,1];
// UV = the quad corner in [-0.5,0.5] (rotated + scaled in the shader); UV2 =
// (rnd, rnd2) per-leaf randoms (timing/colour, phase/size).
Ref<ArrayMesh> FolioLeaves::_build_mesh() const {
	PackedVector3Array positions;
	PackedVector2Array corners;
	PackedVector2Array randoms;
	PackedInt32Array indices;
	positions.resize(count * 4);
	corners.resize(count * 4);
	randoms.resize(count * 4);
	indices.resize(count * 6);

	// Quad corners, centred so the shader can rotate them about the leaf origin.
	static const Vector2 CORNER[4] = {
		Vector2(0.5f, 0.5f),
		Vector2(0.5f, -0.5f),
		Vector2(-0.5f, -0.5f),
		Vector2(-0.5f, 0.5f)
	};

	for (int leaf = 0; leaf < count; leaf++) {
		const float x = (float)UtilityFunctions::randf();
		const float z = (float)UtilityFunctions::randf();
		const float rnd = (float)UtilityFunctions::randf();
		const float rnd2 = (float)UtilityFunctions::randf();
		for (int v = 0; v < 4; v++) {
			const int idx = leaf * 4 + v;
			positions[idx] = Vector3(x, 0.0f, z);
			corners[idx] = CORNER[v];
			randoms[idx] = Vector2(rnd, rnd2);
		}
		const int base = leaf * 4;
		const int i6 = leaf * 6;
		indices[i6 + 0] = base + 0;
		indices[i6 + 1] = base + 3;
		indices[i6 + 2] = base + 2;
		indices[i6 + 3] = base + 2;
		indices[i6 + 4] = base + 1;
		indices[i6 + 5] = base + 0;
	}

	Array arrays;
	arrays.resize(Mesh::ARRAY_MAX);
	arrays[Mesh::ARRAY_VERTEX] = positions;
	arrays[Mesh::ARRAY_TEX_UV] = corners;
	arrays[Mesh::ARRAY_TEX_UV2] = randoms;
	arrays[Mesh::ARRAY_INDEX] = indices;

	Ref<ArrayMesh> m;
	m.instantiate();
	m->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, arrays);
	return m;
}

void FolioLeaves::_ready() {
	if (ready_done) {
		return;
	}
	ready_done = true;

	mesh = _build_mesh();

	material.instantiate();
	Ref<Shader> shader = ResourceLoader::get_singleton()->load("res://material/shaders/folio/leaves.gdshader");
	if (shader.is_valid()) {
		material->set_shader(shader);
	}
	material->set_shader_parameter("elevation", (float)elevation);
	material->set_shader_parameter("amount", (float)amount);

	mesh_instance = memnew(MeshInstance3D);
	mesh_instance->set_name("LeavesMesh");
	mesh_instance->set_mesh(mesh);
	mesh_instance->set_material_override(material);
	mesh_instance->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
	// The vertex stage relocates every leaf, so never frustum-cull the mesh.
	mesh_instance->set_custom_aabb(AABB(Vector3(-1000, -1000, -1000), Vector3(2000, 2000, 2000)));
	add_child(mesh_instance);

	FolioTicker *ticker = FolioTicker::get_singleton();
	if (ticker) {
		ticker->connect_tick(callable_mp(this, &FolioLeaves::update), 10);
	}
}

void FolioLeaves::update() {
	if (material.is_null()) {
		return;
	}

	const bool visible = amount > 0.00001;
	if (mesh_instance) {
		mesh_instance->set_visible(visible);
	}
	if (!visible) {
		return;
	}

	// Field size + centre follow the view (folio size = radius * 2).
	FolioGame *game = FolioGame::get_singleton();
	FolioView *view = game ? game->get_view() : nullptr;

	// Density follows the season (folio YearCycles.leaves): autumn -> full, spring -> bare.
	if (game && game->get_year_cycles()) {
		set_amount(game->get_year_cycles()->get_leaves());
	}

	double size = 80.0;
	Vector2 center;
	if (view) {
		size = view->get_optimal_radius() * 2.0;
		const Vector3 c = view->get_optimal_area_position();
		center = Vector2(c.x, c.z);
	}

	material->set_shader_parameter("size", (float)size);
	material->set_shader_parameter("center", center);
}

void FolioLeaves::set_amount(double p_a) {
	amount = Math::clamp(p_a, 0.0, 1.0);
	if (material.is_valid()) {
		material->set_shader_parameter("amount", (float)amount);
	}
}

void FolioLeaves::_bind_methods() {
	ClassDB::bind_method(D_METHOD("update"), &FolioLeaves::update);
	ClassDB::bind_method(D_METHOD("set_count", "n"), &FolioLeaves::set_count);
	ClassDB::bind_method(D_METHOD("get_count"), &FolioLeaves::get_count);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "count"), "set_count", "get_count");

	ClassDB::bind_method(D_METHOD("set_amount", "a"), &FolioLeaves::set_amount);
	ClassDB::bind_method(D_METHOD("get_amount"), &FolioLeaves::get_amount);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "amount"), "set_amount", "get_amount");

	ClassDB::bind_method(D_METHOD("set_elevation", "e"), &FolioLeaves::set_elevation);
	ClassDB::bind_method(D_METHOD("get_elevation"), &FolioLeaves::get_elevation);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "elevation"), "set_elevation", "get_elevation");
}
