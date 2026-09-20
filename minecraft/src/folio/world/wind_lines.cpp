#include "wind_lines.h"

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
#include "../wind.h"
#include "../view/view.h"

using namespace godot;

FolioWindLines::FolioWindLines() {}
FolioWindLines::~FolioWindLines() {}

static double remap_clamp(double v, double in_min, double in_max, double out_min, double out_max) {
	double t = (in_max - in_min);
	double r = (t == 0.0) ? out_min : (out_min + (out_max - out_min) * (v - in_min) / t);
	return Math::clamp(r, Math::min(out_min, out_max), Math::max(out_min, out_max));
}

// Uniform Catmull-Rom through p0..p3 at parameter u (folio uses CatmullRomCurve3).
static Vector3 catmull_rom(const Vector3 &p0, const Vector3 &p1, const Vector3 &p2, const Vector3 &p3, double u) {
	const double u2 = u * u;
	const double u3 = u2 * u;
	return (p1 * 2.0 + (p2 - p0) * u + (p0 * 2.0 - p1 * 5.0 + p2 * 4.0 - p3) * u2 + (p1 * 3.0 - p0 - p2 * 3.0 + p3) * u3) * 0.5;
}

// folio WindLineGeometry -> LineGeometry: a wavy centreline sampled into a ribbon
// strip. Two verts per point (edges chosen in-shader by UV.y), UV = (ratio, side).
Ref<ArrayMesh> FolioWindLines::_build_ribbon() const {
	const int handles_count = 4;
	const double length = 10.0;
	const double amplitude = 1.0;
	const int divisions = 30;
	const double half_extent = length * 0.5;
	const double handle_span = length / (double)(handles_count - 1);

	// Handles (folio: alternating y, evenly spaced z).
	Vector<Vector3> handles;
	for (int i = 0; i < handles_count; i++) {
		const double y = (double)(i % 2) - 0.5 * amplitude;
		const double z = -half_extent + i * handle_span;
		handles.push_back(Vector3(0, y, z));
	}

	// Sample the Catmull-Rom curve into `divisions + 1` points.
	const int count = divisions + 1;
	Vector<Vector3> points;
	for (int s = 0; s <= divisions; s++) {
		const double gt = (double)s / (double)divisions * (double)(handles_count - 1);
		int seg = (int)Math::floor(gt);
		if (seg > handles_count - 2) {
			seg = handles_count - 2;
		}
		const double u = gt - seg;
		const Vector3 p0 = handles[seg > 0 ? seg - 1 : 0];
		const Vector3 p1 = handles[seg];
		const Vector3 p2 = handles[seg + 1];
		const Vector3 p3 = handles[seg < handles_count - 2 ? seg + 2 : handles_count - 1];
		points.push_back(catmull_rom(p0, p1, p2, p3, u));
	}

	PackedVector3Array positions;
	PackedVector2Array uvs;
	PackedInt32Array indices;
	positions.resize(count * 2);
	uvs.resize(count * 2);
	for (int i = 0; i < count; i++) {
		const double ratio = (double)i / (double)(count - 1);
		positions[i * 2 + 0] = points[i];
		positions[i * 2 + 1] = points[i];
		uvs[i * 2 + 0] = Vector2(ratio, 0.5); // side +0.5
		uvs[i * 2 + 1] = Vector2(ratio, -0.5); // side -0.5
	}
	for (int i = 0; i < count - 1; i++) {
		const int i2 = i * 2;
		// folio winding (cull disabled, so orientation is irrelevant).
		indices.push_back(i2 + 2);
		indices.push_back(i2);
		indices.push_back(i2 + 1);
		indices.push_back(i2 + 1);
		indices.push_back(i2 + 3);
		indices.push_back(i2 + 2);
	}

	Array arrays;
	arrays.resize(Mesh::ARRAY_MAX);
	arrays[Mesh::ARRAY_VERTEX] = positions;
	arrays[Mesh::ARRAY_TEX_UV] = uvs;
	arrays[Mesh::ARRAY_INDEX] = indices;

	Ref<ArrayMesh> mesh;
	mesh.instantiate();
	mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, arrays);
	return mesh;
}

void FolioWindLines::_ready() {
	if (ready_done) {
		return;
	}
	ready_done = true;

	ribbon = _build_ribbon();

	Ref<Shader> shader = ResourceLoader::get_singleton()->load("res://material/shaders/folio/wind_line.gdshader");

	for (int i = 0; i < pool_size; i++) {
		Line line;
		line.mesh = memnew(MeshInstance3D);
		line.mesh->set_name(String("WindLine_") + String::num_int64(i));
		line.mesh->set_mesh(ribbon);
		line.mesh->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
		line.mesh->set_visible(false);
		// Big AABB so the shader-displaced ribbon is never frustum-culled.
		line.mesh->set_custom_aabb(AABB(Vector3(-1000, -1000, -1000), Vector3(2000, 2000, 2000)));

		line.material.instantiate();
		if (shader.is_valid()) {
			line.material->set_shader(shader);
		}
		line.material->set_shader_parameter("thickness", (float)thickness);
		line.material->set_shader_parameter("progress", 0.0f);
		line.mesh->set_material_override(line.material);

		add_child(line.mesh);
		pool.push_back(line);
	}

	FolioTicker *ticker = FolioTicker::get_singleton();
	if (ticker) {
		ticker->connect_tick(callable_mp(this, &FolioWindLines::update), 10);
		next_spawn = ticker->get_elapsed(); // spawn the first one promptly
	}
}

void FolioWindLines::_display(double p_elapsed) {
	// Find a free slot.
	int idx = -1;
	for (int i = 0; i < pool.size(); i++) {
		if (!pool[i].active) {
			idx = i;
			break;
		}
	}
	if (idx < 0) {
		return;
	}

	FolioGame *game = FolioGame::get_singleton();
	FolioWeather *weather = game ? game->get_weather() : nullptr;
	FolioWind *wind = game ? game->get_wind() : nullptr;
	FolioView *view = game ? game->get_view() : nullptr;

	// folio: duration = remapClamp(weather.wind, 0,1, 8,2); stronger wind = faster.
	const double wind_value = weather ? weather->get_wind() : 0.5;
	const double duration = remap_clamp(wind_value, 0.0, 1.0, 8.0, 2.0);

	const double angle = wind ? wind->get_angle() : (Math::PI * 0.6);
	Vector3 center;
	double radius = 40.0;
	if (view) {
		center = view->get_optimal_area_position();
		radius = view->get_optimal_radius();
	}

	Vector3 pos;
	pos.x = center.x + (UtilityFunctions::randf() - 0.5) * radius;
	pos.z = center.z + (UtilityFunctions::randf() - 0.5) * radius;
	pos.y = spawn_height;

	Line &line = pool.write[idx];
	line.active = true;
	line.t_start = p_elapsed;
	line.duration = duration;
	line.start_pos = pos;
	line.end_pos = pos + Vector3(Math::sin(angle) * translation, 0.0, Math::cos(angle) * translation);

	line.mesh->set_position(pos);
	line.mesh->set_rotation(Vector3(0.0, angle, 0.0));
	line.mesh->set_visible(true);
	line.material->set_shader_parameter("progress", 0.0f);
}

void FolioWindLines::update() {
	FolioTicker *ticker = FolioTicker::get_singleton();
	const double elapsed = ticker ? ticker->get_elapsed() : 0.0;

	// Advance active streaks (folio gsap position + progress tweens).
	for (int i = 0; i < pool.size(); i++) {
		Line &line = pool.write[i];
		if (!line.active) {
			continue;
		}
		double u = (line.duration > 0.0) ? (elapsed - line.t_start) / line.duration : 1.0;
		if (u >= 1.0) {
			line.active = false;
			line.mesh->set_visible(false);
			continue;
		}
		line.material->set_shader_parameter("progress", (float)u);
		line.mesh->set_position(line.start_pos.lerp(line.end_pos, u));
	}

	// Spawn on a random interval (folio setTimeout 0.3–2s).
	if (elapsed >= next_spawn) {
		_display(elapsed);
		const double gap = interval_min + UtilityFunctions::randf() * (interval_max - interval_min);
		next_spawn = elapsed + gap;
	}
}

void FolioWindLines::set_thickness(double p_v) {
	thickness = p_v;
	for (int i = 0; i < pool.size(); i++) {
		if (pool[i].material.is_valid()) {
			pool[i].material->set_shader_parameter("thickness", (float)thickness);
		}
	}
}

void FolioWindLines::_bind_methods() {
	ClassDB::bind_method(D_METHOD("update"), &FolioWindLines::update);
	ClassDB::bind_method(D_METHOD("set_pool_size", "n"), &FolioWindLines::set_pool_size);
	ClassDB::bind_method(D_METHOD("get_pool_size"), &FolioWindLines::get_pool_size);
	ClassDB::bind_method(D_METHOD("set_thickness", "v"), &FolioWindLines::set_thickness);
	ClassDB::bind_method(D_METHOD("get_thickness"), &FolioWindLines::get_thickness);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "pool_size"), "set_pool_size", "get_pool_size");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "thickness"), "set_thickness", "get_thickness");
}
