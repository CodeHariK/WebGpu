#include "rain_lines.h"

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
#include "../weather.h"
#include "../view/view.h"

using namespace godot;

FolioRainLines::FolioRainLines() {}
FolioRainLines::~FolioRainLines() {}

static double remap_clamp(double v, double in_min, double in_max, double out_min, double out_max) {
	double t = (in_max - in_min);
	double r = (t == 0.0) ? out_min : (out_min + (out_max - out_min) * (v - in_min) / t);
	return Math::clamp(r, Math::min(out_min, out_max), Math::max(out_min, out_max));
}

// folio RainLines geometry: `count` line-quads. Base XZ in [0,1] per line, a
// per-line random, per-vertex offset (side, top/bottom). Packed as:
//   VERTEX = (x, 0, z)   UV = offset   UV2.x = random.
Ref<ArrayMesh> FolioRainLines::_build_mesh() const {
	PackedVector3Array positions;
	PackedVector2Array offsets;
	PackedVector2Array randoms;
	PackedInt32Array indices;
	positions.resize(count * 4);
	offsets.resize(count * 4);
	randoms.resize(count * 4);
	indices.resize(count * 6);

	// folio per-vertex offsets: v0(1,1) v1(1,0) v2(0,0) v3(0,1).
	static const Vector2 OFF[4] = { Vector2(1, 1), Vector2(1, 0), Vector2(0, 0), Vector2(0, 1) };

	for (int line = 0; line < count; line++) {
		const float x = (float)UtilityFunctions::randf();
		const float z = (float)UtilityFunctions::randf();
		const float rnd = (float)UtilityFunctions::randf();
		for (int v = 0; v < 4; v++) {
			const int idx = line * 4 + v;
			positions[idx] = Vector3(x, 0.0f, z);
			offsets[idx] = OFF[v];
			randoms[idx] = Vector2(rnd, 0.0f);
		}
		const int base = line * 4;
		const int i6 = line * 6;
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
	arrays[Mesh::ARRAY_TEX_UV] = offsets;
	arrays[Mesh::ARRAY_TEX_UV2] = randoms;
	arrays[Mesh::ARRAY_INDEX] = indices;

	Ref<ArrayMesh> m;
	m.instantiate();
	m->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, arrays);
	return m;
}

void FolioRainLines::_ready() {
	if (ready_done) {
		return;
	}
	ready_done = true;

	mesh = _build_mesh();

	material.instantiate();
	Ref<Shader> shader = ResourceLoader::get_singleton()->load("res://material/shaders/folio/rain_lines.gdshader");
	if (shader.is_valid()) {
		material->set_shader(shader);
	}
	material->set_shader_parameter("thickness", (float)thickness);
	material->set_shader_parameter("elevation", (float)elevation);
	material->set_shader_parameter("visible_ratio", 0.0f);

	mesh_instance = memnew(MeshInstance3D);
	mesh_instance->set_name("RainLinesMesh");
	mesh_instance->set_mesh(mesh);
	mesh_instance->set_material_override(material);
	mesh_instance->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
	mesh_instance->set_position(Vector3(0.0, -0.3, 0.0)); // folio mesh.position.y = -0.3
	// Never frustum-cull: the vertex stage relocates everything (folio frustumCulled = false).
	mesh_instance->set_custom_aabb(AABB(Vector3(-1000, -1000, -1000), Vector3(2000, 2000, 2000)));
	add_child(mesh_instance);

	FolioTicker *ticker = FolioTicker::get_singleton();
	if (ticker) {
		ticker->connect_tick(callable_mp(this, &FolioRainLines::update), 10);
	}
}

void FolioRainLines::update() {
	FolioGame *game = FolioGame::get_singleton();
	FolioWeather *weather = game ? game->get_weather() : nullptr;
	FolioView *view = game ? game->get_view() : nullptr;
	FolioTicker *ticker = FolioTicker::get_singleton();

	const double rain = weather ? weather->get_rain() : 0.0;
	const double snow = weather ? weather->get_snow() : 0.0;
	const double wind = weather ? weather->get_wind() : 0.5;

	// folio weather bindings.
	const double visible_ratio = rain * rain; // pow(rain, 2)
	const double snow_ratio = 1.0 - Math::pow(1.0 - Math::max(snow, 0.0), 4.0);
	const double line_length = Math::lerp(remap_clamp(rain, 0.0, 1.0, 1.0, 3.0), 0.03, snow_ratio);
	speed = Math::lerp(remap_clamp(rain, 0.0, 1.0, 0.2, 0.4), 0.05, snow_ratio);
	const double incline = remap_clamp(wind, 0.0, 1.0, 0.1, 0.4);

	const bool visible = visible_ratio > 0.00001;
	if (mesh_instance) {
		mesh_instance->set_visible(visible);
	}
	if (!visible) {
		return;
	}

	// Field size + centre follow the view (folio size = radius*2).
	double size = 80.0;
	Vector2 center;
	if (view) {
		size = view->get_optimal_radius() * 2.0;
		const Vector3 c = view->get_optimal_area_position();
		center = Vector2(c.x, c.z);
	}

	const double delta_scaled = ticker ? ticker->get_delta_scaled() : (1.0 / 30.0);
	local_time += delta_scaled * speed;

	material->set_shader_parameter("visible_ratio", (float)visible_ratio);
	material->set_shader_parameter("line_length", (float)line_length);
	material->set_shader_parameter("incline", (float)incline);
	material->set_shader_parameter("size", (float)size);
	material->set_shader_parameter("center", center);
	material->set_shader_parameter("local_time", (float)local_time);
}

void FolioRainLines::_bind_methods() {
	ClassDB::bind_method(D_METHOD("update"), &FolioRainLines::update);
	ClassDB::bind_method(D_METHOD("set_count", "n"), &FolioRainLines::set_count);
	ClassDB::bind_method(D_METHOD("get_count"), &FolioRainLines::get_count);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "count"), "set_count", "get_count");
}
