/**
 * @file sky_cycle.cpp
 * @brief SkyCycle: the procedural sky shader, the palette interpolation and the light/ambient driving.
 */
#include "sky_cycle.h"
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/environment.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/sky.hpp>
#include <godot_cpp/core/class_db.hpp>

namespace godot {

// ---------------------------------------------------------------------------------------------
// The sky shader (textureless toon sky). The driver sets every colour and direction each frame.
// ---------------------------------------------------------------------------------------------

static const char *SKY_SHADER = R"(
shader_type sky;
render_mode use_debanding;

uniform vec3 sun_dir = vec3(0.0, 1.0, 0.0);
uniform vec3 moon_dir = vec3(0.0, -1.0, 0.0);
uniform vec3 zenith : source_color = vec3(0.16, 0.44, 0.78);
uniform vec3 horizon : source_color = vec3(0.66, 0.85, 0.95);
uniform vec3 ground : source_color = vec3(0.05, 0.06, 0.08);
uniform vec3 glow_color : source_color = vec3(1.0, 0.6, 0.3);
uniform float glow_amount = 0.0;      // Sunset/sunrise horizon glow near the sun
uniform int bands = 4;
uniform vec3 sun_color : source_color = vec3(1.0, 0.95, 0.8);
uniform float sun_size = 0.035;
uniform float sun_bright = 1.0;
uniform vec3 moon_color : source_color = vec3(1.0, 0.86, 0.55);
uniform float moon_size = 0.06;
uniform float moon_vis = 0.0;         // 0 day .. 1 night
uniform float moon_ring = 1.0;
uniform vec3 star_color : source_color = vec3(0.9, 0.95, 1.0);
uniform float star_amount = 0.0;      // 0 day .. 1 night, scaled by density

float hash(vec2 p) {
	p = fract(p * vec2(443.897, 441.423));
	p += dot(p, p + 19.19);
	return fract((p.x + p.y) * p.x);
}

void sky() {
	vec3 dir = EYEDIR;
	float up = dir.y;

	// Banded vertical gradient: ground below, horizon -> zenith above, posterized for the toon look.
	float t = clamp(up, 0.0, 1.0);
	if (bands > 1) {
		float b = float(bands);
		t = (floor(t * b) + smoothstep(0.35, 0.65, fract(t * b))) / b;
	}
	vec3 col = mix(horizon, zenith, t);
	col = mix(col, ground, smoothstep(0.0, -0.15, up));

	// Horizon glow, strongest low and towards the sun's azimuth.
	float az = max(dot(normalize(vec3(dir.x, 0.0, dir.z)), normalize(vec3(sun_dir.x, 0.0, sun_dir.z))), 0.0);
	float band = exp(-abs(up) * 7.0) * pow(az, 2.0);
	col = mix(col, glow_color, clamp(band * glow_amount, 0.0, 1.0));

	// Stars: one small round point per grid cell, placed and twinkled by a hash, only at night.
	if (star_amount > 0.001 && up > 0.03) {
		vec2 uv = dir.xz / (abs(dir.y) + 0.35) * 15.0;
		vec2 cell = floor(uv);
		vec2 f = fract(uv);
		float h = hash(cell);
		float present = step(1.0 - star_amount * 0.35, h);
		vec2 sp = vec2(hash(cell + 5.0), hash(cell + 9.0));
		float pt = smoothstep(0.09, 0.0, length(f - sp)) * present;
		float tw = 0.55 + 0.45 * sin(TIME * 3.0 + h * 60.0);
		col += star_color * pt * tw * smoothstep(0.03, 0.28, up);
	}

	// Moon: an invented ringed orb. Disc + soft halo, optional flat ring, fades in at night.
	float md = dot(dir, moon_dir);
	float m_ang = acos(clamp(md, -1.0, 1.0));
	float disc = 1.0 - smoothstep(moon_size * 0.85, moon_size, m_ang);
	float halo = pow(max(md, 0.0), 500.0) * 0.35;
	float ring = 0.0;
	if (moon_ring > 0.5) {
		float r = moon_size * 1.7;
		ring = (smoothstep(r - moon_size * 0.18, r, m_ang) - smoothstep(r, r + moon_size * 0.18, m_ang)) * 0.7;
	}
	col = mix(col, moon_color, clamp((disc + ring) * moon_vis, 0.0, 1.0));
	col += moon_color * halo * moon_vis;

	// Sun: flat disc + halo, always the brightest thing while it is up.
	float sd = dot(dir, sun_dir);
	float s_ang = acos(clamp(sd, -1.0, 1.0));
	float sun_disc = 1.0 - smoothstep(sun_size * 0.85, sun_size, s_ang);
	float sun_halo = pow(max(sd, 0.0), 120.0) * 0.6;
	col = mix(col, sun_color, clamp(sun_disc * sun_bright, 0.0, 1.0));
	col += sun_color * sun_halo * sun_bright;

	COLOR = col;
}
)";

// ---------------------------------------------------------------------------------------------
// Bindings
// ---------------------------------------------------------------------------------------------

// clang-format off
#define SC_BIND(m_variant, m_name, m_hint, m_hint_str)                                                                 \
	ClassDB::bind_method(D_METHOD("set_" #m_name, "value"), &SkyCycle::set_##m_name);                                  \
	ClassDB::bind_method(D_METHOD("get_" #m_name), &SkyCycle::get_##m_name);                                           \
	ADD_PROPERTY(PropertyInfo(Variant::m_variant, #m_name, m_hint, m_hint_str), "set_" #m_name, "get_" #m_name)

void SkyCycle::_bind_methods() {
	ADD_GROUP("Time", "");
	ClassDB::bind_method(D_METHOD("set_time_of_day", "value"), &SkyCycle::set_time_of_day);
	ClassDB::bind_method(D_METHOD("get_time_of_day"), &SkyCycle::get_time_of_day);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "time_of_day", PROPERTY_HINT_RANGE, "0,1,0.001"), "set_time_of_day", "get_time_of_day");
	SC_BIND(FLOAT, day_length, PROPERTY_HINT_RANGE, "0,3600,1,suffix:s");
	SC_BIND(BOOL, paused, PROPERTY_HINT_NONE, "");
	ClassDB::bind_method(D_METHOD("set_sun_light_path", "path"), &SkyCycle::set_sun_light_path);
	ClassDB::bind_method(D_METHOD("get_sun_light_path"), &SkyCycle::get_sun_light_path);
	ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "sun_light", PROPERTY_HINT_NODE_PATH_VALID_TYPES, "DirectionalLight3D"), "set_sun_light_path", "get_sun_light_path");

	ADD_GROUP("Sky Palette", "");
	SC_BIND(COLOR, day_zenith, PROPERTY_HINT_NONE, "");
	SC_BIND(COLOR, day_horizon, PROPERTY_HINT_NONE, "");
	SC_BIND(COLOR, dusk_zenith, PROPERTY_HINT_NONE, "");
	SC_BIND(COLOR, dusk_horizon, PROPERTY_HINT_NONE, "");
	SC_BIND(COLOR, night_zenith, PROPERTY_HINT_NONE, "");
	SC_BIND(COLOR, night_horizon, PROPERTY_HINT_NONE, "");
	SC_BIND(INT, bands, PROPERTY_HINT_RANGE, "1,12,1");

	ADD_GROUP("Sun / Moon / Stars", "");
	SC_BIND(COLOR, sun_color, PROPERTY_HINT_NONE, "");
	SC_BIND(FLOAT, sun_size, PROPERTY_HINT_RANGE, "0.005,0.3,0.001");
	SC_BIND(COLOR, moon_color, PROPERTY_HINT_NONE, "");
	SC_BIND(FLOAT, moon_size, PROPERTY_HINT_RANGE, "0.005,0.3,0.001");
	SC_BIND(BOOL, moon_ring, PROPERTY_HINT_NONE, "");
	SC_BIND(COLOR, star_color, PROPERTY_HINT_NONE, "");
	SC_BIND(FLOAT, star_density, PROPERTY_HINT_RANGE, "0,1,0.01");

	ADD_GROUP("Lighting", "");
	SC_BIND(COLOR, moonlight_color, PROPERTY_HINT_NONE, "");
	SC_BIND(FLOAT, sun_energy, PROPERTY_HINT_RANGE, "0,4,0.05");
	SC_BIND(FLOAT, moon_energy, PROPERTY_HINT_RANGE, "0,2,0.01");
	SC_BIND(COLOR, night_ambient, PROPERTY_HINT_NONE, "");
	SC_BIND(FLOAT, day_ambient_energy, PROPERTY_HINT_RANGE, "0,2,0.01");
	SC_BIND(FLOAT, night_ambient_energy, PROPERTY_HINT_RANGE, "0,1,0.005");
	ADD_GROUP("Fog", "");
	SC_BIND(BOOL, fog, PROPERTY_HINT_NONE, "");
	SC_BIND(COLOR, night_fog_color, PROPERTY_HINT_NONE, "");
	SC_BIND(FLOAT, night_fog_density, PROPERTY_HINT_RANGE, "0,0.2,0.001");
}
#undef SC_BIND
// clang-format on

SkyCycle::SkyCycle() {}
SkyCycle::~SkyCycle() {}

void SkyCycle::set_time_of_day(float p_t) {
	time_of_day = p_t - Math::floor(p_t); // Wrap to 0..1
	_apply();
}

bool SkyCycle::is_night() const {
	// Sun altitude from the same arc as _apply(); below the horizon (with a little margin) = night.
	const float phase = (time_of_day - 0.25f) * (float)Math::TAU;
	return Math::sin(phase) < -0.03f;
}

SkyCycle *SkyCycle::find(Node *p_node) {
	if (!p_node || !p_node->is_inside_tree()) {
		return nullptr;
	}
	return Object::cast_to<SkyCycle>(p_node->get_tree()->get_first_node_in_group("sky_cycle"));
}

void SkyCycle::_notification(int p_what) {
	if (p_what == NOTIFICATION_READY) {
		add_to_group("sky_cycle");
		_ensure_sky();
		_initialized = true;
		set_process(true);
		_apply();
	}
}

// ---------------------------------------------------------------------------------------------
// Setup
// ---------------------------------------------------------------------------------------------

DirectionalLight3D *SkyCycle::_find_sun() {
	if (_sun && _sun->is_inside_tree()) {
		return _sun;
	}
	if (!sun_light_path.is_empty()) {
		_sun = Object::cast_to<DirectionalLight3D>(get_node_or_null(sun_light_path));
	}
	if (!_sun) { // First DirectionalLight3D sibling
		Node *parent = get_parent();
		if (parent) {
			TypedArray<Node> kids = parent->get_children();
			for (int i = 0; i < kids.size(); ++i) {
				if (DirectionalLight3D *d = Object::cast_to<DirectionalLight3D>(kids[i])) {
					_sun = d;
					break;
				}
			}
		}
	}
	return _sun;
}

// The compiled sky program, shared by every SkyCycle. File scope (not a function-local static) so
// clear_sky_shader_cache() can release the RID before Godot's renderer shuts down.
namespace {
Ref<Shader> s_sky_shader;
}

void clear_sky_shader_cache() { s_sky_shader = Ref<Shader>(); }

/// Give the environment a Sky backed by our shader (creating the Environment / Sky if missing).
void SkyCycle::_ensure_sky() {
	Ref<Environment> env = get_environment();
	if (env.is_null()) {
		env.instantiate();
		set_environment(env);
	}
	env->set_background(Environment::BG_SKY);
	env->set_ambient_source(Environment::AMBIENT_SOURCE_COLOR);
	env->set_glow_enabled(true);
	env->set_fog_enabled(fog);
	env->set_fog_sky_affect(0.0f); // Never fog the sky itself — keep the stars / moon crisp

	Ref<Sky> sky = env->get_sky();
	if (sky.is_null()) {
		sky.instantiate();
		env->set_sky(sky);
	}
	_sky_mat = sky->get_material();
	if (_sky_mat.is_null() || _sky_mat->get_shader().is_null()) {
		// The sky program is identical for every SkyCycle; only the per-instance uniforms (driven each
		// frame) differ, so they live on the ShaderMaterial while the compiled Shader is shared.
		if (s_sky_shader.is_null()) {
			s_sky_shader.instantiate();
			s_sky_shader->set_code(SKY_SHADER);
		}
		_sky_mat.instantiate();
		_sky_mat->set_shader(s_sky_shader);
		sky->set_material(_sky_mat);
	}
}

// ---------------------------------------------------------------------------------------------
// Per-frame
// ---------------------------------------------------------------------------------------------

void SkyCycle::_process(double p_delta) {
	if (Engine::get_singleton()->is_editor_hint()) {
		return;
	}
	if (!paused && day_length > 0.0f) {
		time_of_day = Math::fposmod(time_of_day + (float)p_delta / day_length, 1.0f);
		_apply();
	}
}

/// Sky colour at the current time: night -> (dawn = dusk palette) -> day -> dusk -> night.
static Color phase_lerp(
		float p_t,
		const Color &p_night,
		const Color &p_dusk,
		const Color &p_day
) {
	// Keyframes at 0 night, 0.25 dawn(=dusk), 0.5 day, 0.75 dusk, 1 night.
	if (p_t < 0.25f) {
		return p_night.lerp(p_dusk, Math::smoothstep(0.05f, 0.25f, p_t));
	} else if (p_t < 0.5f) {
		return p_dusk.lerp(p_day, Math::smoothstep(0.25f, 0.45f, p_t));
	} else if (p_t < 0.75f) {
		return p_day.lerp(p_dusk, Math::smoothstep(0.55f, 0.75f, p_t));
	}
	return p_dusk.lerp(p_night, Math::smoothstep(0.75f, 0.95f, p_t));
}

void SkyCycle::_apply() {
	if (!_initialized || _sky_mat.is_null()) {
		return;
	}
	const float phase = (time_of_day - 0.25f) * (float)Math::TAU;
	const Vector3 sun_dir = Vector3(Math::cos(phase), Math::sin(phase), 0.22f).normalized();
	const Vector3 moon_dir =
			Vector3(Math::cos(phase + (float)Math::PI), Math::sin(phase + (float)Math::PI), -0.18f).normalized();
	const float sun_alt = sun_dir.y;
	const float moon_alt = moon_dir.y;

	const float day = Math::smoothstep(-0.08f, 0.22f, sun_alt); // 0 night .. 1 day
	const float night = 1.0f - day;
	const float dusk = 1.0f - Math::abs(sun_alt) * 4.0f; // Peaks near the horizon

	// Sky uniforms.
	_sky_mat->set_shader_parameter("sun_dir", sun_dir);
	_sky_mat->set_shader_parameter("moon_dir", moon_dir);
	_sky_mat->set_shader_parameter("zenith", phase_lerp(time_of_day, night_zenith, dusk_zenith, day_zenith));
	_sky_mat->set_shader_parameter("horizon", phase_lerp(time_of_day, night_horizon, dusk_horizon, day_horizon));
	_sky_mat->set_shader_parameter("ground", night_zenith.darkened(0.3f));
	_sky_mat->set_shader_parameter("glow_color", dusk_horizon);
	_sky_mat->set_shader_parameter("glow_amount", CLAMP(dusk, 0.0f, 1.0f) * 0.9f);
	_sky_mat->set_shader_parameter("bands", bands);
	_sky_mat->set_shader_parameter("sun_color", sun_color);
	_sky_mat->set_shader_parameter("sun_size", sun_size);
	_sky_mat->set_shader_parameter("sun_bright", Math::smoothstep(-0.06f, 0.05f, sun_alt));
	_sky_mat->set_shader_parameter("moon_color", moon_color);
	_sky_mat->set_shader_parameter("moon_size", moon_size);
	_sky_mat->set_shader_parameter("moon_ring", moon_ring ? 1.0f : 0.0f);
	_sky_mat->set_shader_parameter("moon_vis", CLAMP(night * Math::smoothstep(-0.1f, 0.1f, moon_alt), 0.0f, 1.0f));
	_sky_mat->set_shader_parameter("star_color", star_color);
	_sky_mat->set_shader_parameter("star_amount", night * star_density);

	// Ambient: bright sky by day, crushed to near-black cold at night (horror dark).
	Ref<Environment> env = get_environment();
	if (env.is_valid()) {
		const Color amb = night_ambient.lerp(day_horizon, day);
		env->set_ambient_light_color(amb);
		env->set_ambient_light_energy(Math::lerp(night_ambient_energy, day_ambient_energy, day));
		// Fog: thick and dark at night so distance vanishes into black; clears by day.
		env->set_fog_enabled(fog);
		if (fog) {
			// Dark, neutral fog (no blue haze); only really present deep at night (night^1.5 ramp).
			const float fog_night = Math::pow(CLAMP(night, 0.0f, 1.0f), 1.5f);
			env->set_fog_light_color(night_fog_color);
			env->set_fog_density(night_fog_density * fog_night);
			env->set_fog_aerial_perspective(0.15f);
		}
	}

	// Sun light: point along the sun by day, along the moon (dim, blue) by night.
	DirectionalLight3D *sun = _find_sun();
	if (sun) {
		const bool use_moon = sun_alt < -0.03f;
		const Vector3 to_ground = use_moon ? -moon_dir : -sun_dir;
		const Vector3 upv = Math::abs(to_ground.y) > 0.98f ? Vector3(0, 0, 1) : Vector3(0, 1, 0);
		sun->set_global_transform(Transform3D(Basis::looking_at(to_ground, upv), sun->get_global_position()));
		if (use_moon) {
			sun->set_color(moonlight_color);
			sun->set_param(Light3D::PARAM_ENERGY, moon_energy * Math::smoothstep(-0.05f, 0.2f, moon_alt));
		} else {
			const Color warm = sun_color.lerp(dusk_horizon, CLAMP(dusk, 0.0f, 1.0f) * 0.8f);
			sun->set_color(warm);
			sun->set_param(Light3D::PARAM_ENERGY, sun_energy * Math::smoothstep(-0.03f, 0.25f, sun_alt));
		}
	}
}

} // namespace godot
