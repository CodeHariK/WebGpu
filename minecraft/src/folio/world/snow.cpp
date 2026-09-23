#include "snow.h"

#include <godot_cpp/classes/mesh.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/math.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>
#include <godot_cpp/variant/packed_vector3_array.hpp>

#include "../../utils/spring/spring_dynamics.h"
#include "../game.h"
#include "../ticker.h"
#include "../view/view.h"
#include "../weather.h"

using namespace godot;

FolioSnow::FolioSnow() {}
FolioSnow::~FolioSnow() {}

static double remap_clamp(double v, double in_min, double in_max, double out_min, double out_max) {
	double t = (in_max - in_min);
	double r = (t == 0.0) ? out_min : (out_min + (out_max - out_min) * (v - in_min) / t);
	return Math::clamp(r, Math::min(out_min, out_max), Math::max(out_min, out_max));
}

// A flat subdivided grid centred on the origin (local space). The vertex shader
// displaces it into snow lumps; the node moves it to follow the camera.
Ref<ArrayMesh> FolioSnow::_build_mesh() const {
	const int n = subdivisions;
	const int verts_per_row = n + 1;
	const double half = size * 0.5;
	const double step = size / (double)n;

	PackedVector3Array positions;
	PackedInt32Array indices;
	positions.resize(verts_per_row * verts_per_row);
	indices.resize(n * n * 6);

	for (int z = 0; z <= n; z++) {
		for (int x = 0; x <= n; x++) {
			const int idx = z * verts_per_row + x;
			positions[idx] = Vector3((float)(-half + x * step), 0.0f, (float)(-half + z * step));
		}
	}

	int i = 0;
	for (int z = 0; z < n; z++) {
		for (int x = 0; x < n; x++) {
			const int a = z * verts_per_row + x;
			const int b = a + 1;
			const int c = a + verts_per_row;
			const int d = c + 1;
			indices[i++] = a;
			indices[i++] = c;
			indices[i++] = b;
			indices[i++] = b;
			indices[i++] = c;
			indices[i++] = d;
		}
	}

	Array arrays;
	arrays.resize(Mesh::ARRAY_MAX);
	arrays[Mesh::ARRAY_VERTEX] = positions;
	arrays[Mesh::ARRAY_INDEX] = indices;

	Ref<ArrayMesh> m;
	m.instantiate();
	m->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, arrays);
	return m;
}

void FolioSnow::_ready() {
	if (ready_done) {
		return;
	}
	ready_done = true;

	mesh = _build_mesh();

	material.instantiate();
	Ref<Shader> shader = ResourceLoader::get_singleton()->load("res://material/shaders/folio/snow_ground.gdshader");
	if (shader.is_valid()) {
		material->set_shader(shader);
	}
	material->set_shader_parameter("coverage", 0.0f);
	material->set_shader_parameter("half_size", (float)(size * 0.5));

	mesh_instance = memnew(MeshInstance3D);
	mesh_instance->set_name("SnowMesh");
	mesh_instance->set_mesh(mesh);
	mesh_instance->set_material_override(material);
	mesh_instance->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
	mesh_instance->set_position(Vector3(0.0, surface_y, 0.0));
	// The plane roams to follow the camera; never frustum-cull it.
	mesh_instance->set_custom_aabb(AABB(Vector3(-1000, -1000, -1000), Vector3(2000, 2000, 2000)));
	mesh_instance->set_visible(false);
	add_child(mesh_instance);

	FolioTicker *ticker = FolioTicker::get_singleton();
	if (ticker) {
		ticker->connect_tick(callable_mp(this, &FolioSnow::update), 10);
	}
}

void FolioSnow::update() {
	if (material.is_null()) {
		return;
	}

	FolioGame *game = FolioGame::get_singleton();
	FolioWeather *weather = game ? game->get_weather() : nullptr;
	FolioView *view = game ? game->get_view() : nullptr;
	FolioTicker *ticker = FolioTicker::get_singleton();

	const double rain = weather ? weather->get_rain() : 0.0;
	const double temp = weather ? weather->get_temperature() : 15.0;
	const double delta_scaled = ticker ? ticker->get_delta_scaled() : (1.0 / 30.0);

	// folio: cold + wet accumulates, warm melts -> a target snow depth in [-1, 0.5].
	const double rain_ratio = remap_clamp(rain, 0.05, 0.3, 0.0, 1.0) * remap_clamp(temp, 0.0, -5.0, 0.0, 1.0);
	const double melt_ratio = remap_clamp(temp, 0.0, 10.0, 0.0, -1.0);
	const double target = remap_clamp(rain_ratio + melt_ratio, -1.0, 1.0, -1.0, 0.5);

	// Ease the accumulated depth toward the target (build up / thaw over time).
	if (first_update) {
		elevation = target; // seed so the first frame already reads correctly
		first_update = false;
	} else {
		elevation = Math::lerp(elevation, target, (double)spring_damp_factor((float)accum_rate, (float)delta_scaled));
	}
	elevation = Math::clamp(elevation, -1.0, 0.5);

	// Depth -> coverage. Smoothstep floored ABOVE the melt level so mild weather
	// (a slightly-negative "not fully melted" elevation) shows NO snow at all --
	// only genuine cold+wet accumulation ramps coverage up. This is what stops a
	// ghost snow film (and its glitter) lingering at, say, +6 C.
	double ct = Math::clamp((elevation - (-0.1)) / (0.5 - (-0.1)), 0.0, 1.0);
	const double coverage = ct * ct * (3.0 - 2.0 * ct); // smoothstep

	// Hidden when there is effectively no snow (skips the draw entirely).
	const bool visible = coverage > 0.002;
	if (mesh_instance) {
		mesh_instance->set_visible(visible);
	}
	if (!visible) {
		return;
	}

	// Drift the sparkle.
	glitter_variation += delta_scaled * glitter_time_mult;

	// Follow the camera, snapped to the grid so the world-locked lumps don't shimmer.
	const double cell = size / (double)subdivisions;
	if (view && mesh_instance) {
		const Vector3 c = view->get_optimal_area_position();
		const double rx = Math::round(c.x / cell) * cell;
		const double rz = Math::round(c.z / cell) * cell;
		mesh_instance->set_position(Vector3((float)rx, (float)surface_y, (float)rz));
	}

	material->set_shader_parameter("coverage", (float)coverage);
	material->set_shader_parameter("glitter_variation", (float)glitter_variation);
}

void FolioSnow::_bind_methods() {
	ClassDB::bind_method(D_METHOD("update"), &FolioSnow::update);
	ClassDB::bind_method(D_METHOD("set_size", "s"), &FolioSnow::set_size);
	ClassDB::bind_method(D_METHOD("get_size"), &FolioSnow::get_size);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "size"), "set_size", "get_size");

	ClassDB::bind_method(D_METHOD("set_subdivisions", "n"), &FolioSnow::set_subdivisions);
	ClassDB::bind_method(D_METHOD("get_subdivisions"), &FolioSnow::get_subdivisions);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "subdivisions"), "set_subdivisions", "get_subdivisions");
}
