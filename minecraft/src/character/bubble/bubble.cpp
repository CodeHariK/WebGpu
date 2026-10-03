#include "bubble.h"

#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/geometry_instance3d.hpp>
#include <godot_cpp/classes/sphere_mesh.hpp>
#include <godot_cpp/classes/sphere_shape3d.hpp>
#include <godot_cpp/core/math.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include <cmath>

namespace godot {

// Soap-bubble look in one pass: see-through centre, bright fresnel rim, a rainbow
// "thin film" tint that drifts with angle and time, and one fixed specular highlight.
// Unshaded (no lights needed) and never writes depth, so it layers cleanly.
static const char *BUBBLE_SHADER = R"(
shader_type spatial;
render_mode unshaded, blend_mix, cull_back, depth_draw_never;

uniform float rim_power = 2.5;
uniform float base_alpha = 0.06;
uniform float film_strength = 0.6;
uniform float fade = 0.0; // 0 = solid, 1 = gone (popping)

void fragment() {
	float ndv = clamp(dot(NORMAL, VIEW), 0.0, 1.0);
	float rim = pow(1.0 - ndv, rim_power);

	// Thin-film rainbow: hue from rim + a slow drift, as a cosine palette.
	float h = fract(rim * 1.5 + TIME * 0.05 + VERTEX.y * 0.15);
	vec3 film = 0.5 + 0.5 * cos(6.28318 * (h + vec3(0.0, 0.33, 0.67)));
	vec3 col = mix(vec3(0.85, 0.95, 1.0), film, film_strength);

	// One glossy highlight from a fixed view-space direction (upper left).
	vec3 hl = normalize(vec3(-0.4, 0.6, 0.7));
	float spec = pow(max(dot(NORMAL, hl), 0.0), 60.0);

	ALBEDO = col + vec3(spec);
	ALPHA = clamp(base_alpha + rim * 0.75 + spec, 0.0, 1.0) * (1.0 - fade);
}
)";

static const float POP_TIME = 0.15f; // swell-and-fade duration
static const float DIP_STIFFNESS = 120.0f; // underdamped: it jiggles a little when stood on
static const float DIP_DAMPING = 10.0f;

void Bubble::_bind_methods() {
	ClassDB::bind_method(D_METHOD("pop"), &Bubble::pop);
	ClassDB::bind_method(D_METHOD("is_popping"), &Bubble::is_popping);
	ClassDB::bind_method(D_METHOD("get_age"), &Bubble::get_age);
	ClassDB::bind_method(D_METHOD("get_velocity"), &Bubble::get_velocity);
}

Bubble::Bubble() {}

Ref<Shader> Bubble::shared_shader;

Ref<Shader> Bubble::_shared_shader() {
	if (shared_shader.is_null()) {
		shared_shader.instantiate();
		shared_shader->set_code(BUBBLE_SHADER);
	}
	return shared_shader;
}

void Bubble::_build() {
	set_collision_layer(2); // off layer 1 so the follow cameras' wall rays ignore it
	set_collision_mask(0); // kinematic: it doesn't need to detect anything itself

	Ref<SphereShape3D> sphere;
	sphere.instantiate();
	sphere->set_radius(radius);
	shape = memnew(CollisionShape3D);
	shape->set_shape(sphere);
	add_child(shape);

	Ref<SphereMesh> sm;
	sm.instantiate();
	sm->set_radius(radius);
	sm->set_height(radius * 2.0f);
	sm->set_radial_segments(24); // low-poly is plenty for a bubble
	sm->set_rings(12);
	material.instantiate();
	material->set_shader(_shared_shader());
	mesh = memnew(MeshInstance3D);
	mesh->set_mesh(sm);
	mesh->set_material_override(material);
	mesh->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
	add_child(mesh);
}

void Bubble::_ready() {
	if (Engine::get_singleton()->is_editor_hint()) {
		return;
	}
	add_to_group("bubbles");
	phase = (float)UtilityFunctions::randf_range(0.0, Math::TAU);
	_build();
	if (!launched) { // placed in a scene by hand: just float where it is
		base = get_global_position();
		prev_position = base;
		launched = true;
	}
	_apply_wobble();
}

void Bubble::launch(
		const Vector3 &p_origin,
		const Vector3 &p_dir
) {
	Vector3 flat(p_dir.x, 0.0f, p_dir.z);
	if (flat.length() > 0.001f) {
		flat = flat.normalized();
	}
	base = p_origin;
	prev_position = p_origin;
	launch_velocity = flat * launch_speed;
	age = 0.0f;
	launched = true;
	set_global_position(p_origin);
}

void Bubble::pop() {
	if (pop_progress < 0.0f) {
		pop_progress = 0.0f;
		if (shape) {
			shape->set_deferred("disabled", true); // nothing can stand on a popping bubble
		}
	}
}

void Bubble::_tick_float(float p_dt) {
	// Launch: decelerate smoothly to a stop (quadratic ease-out of the wand's push).
	if (age < launch_time) {
		float k = 1.0f - age / launch_time;
		base += launch_velocity * (k * k) * p_dt;
	}
	base.y += rise_speed * p_dt;

	// Overlapping sines at different frequencies -> a lazy path that never repeats.
	// Faded in after the launch so the shot itself stays a clean straight line.
	float settle = CLAMP((age - launch_time * 0.5f) / 0.5f, 0.0f, 1.0f);
	Vector3 bob(
			sway_amp * std::sin(age * 1.3f + phase),
			bob_amp * std::sin(age * 2.1f + phase),
			sway_amp * std::sin(age * 1.7f + phase * 2.0f)
	);

	// Spring dip: sinks under a rider, jiggles back when they leave.
	float target = stood_on ? -dip_depth : 0.0f;
	stood_on = false;
	dip_velocity += (DIP_STIFFNESS * (target - dip) - DIP_DAMPING * dip_velocity) * p_dt;
	dip += dip_velocity * p_dt;

	Vector3 pos = base + bob * settle + Vector3(0.0f, dip, 0.0f);
	velocity = (pos - prev_position) / p_dt;
	prev_position = pos;
	set_global_position(pos);

	if (age >= lifetime) {
		pop();
	}
}

void Bubble::_tick_pop(float p_dt) {
	velocity = Vector3();
	pop_progress += p_dt / POP_TIME;
	if (material.is_valid()) {
		material->set_shader_parameter("fade", CLAMP(pop_progress, 0.0f, 1.0f));
	}
	if (pop_progress >= 1.0f) {
		queue_free();
	}
}

void Bubble::_apply_wobble() {
	if (!mesh) {
		return;
	}
	// Breathing squash-and-stretch (roughly volume preserving) + flatten while dipped.
	float s = wobble_amp * std::sin(age * 6.0f + phase);
	float squash = CLAMP(-dip / MAX(0.01f, dip_depth), -1.0f, 1.0f) * 0.15f;
	Vector3 scale(1.0f + s + squash * 0.5f, 1.0f - s - squash, 1.0f + s + squash * 0.5f);

	// Pop-in: grow from small with a little overshoot (ease-out-back) over 0.2 s.
	float grow_t = CLAMP(age / 0.2f, 0.0f, 1.0f);
	float c = 1.70158f;
	float g = 1.0f + (c + 1.0f) * std::pow(grow_t - 1.0f, 3.0f) + c * std::pow(grow_t - 1.0f, 2.0f);
	float grow = Math::lerp(0.3f, 1.0f, g);

	// Popping: swell outward as it fades.
	float swell = (pop_progress >= 0.0f) ? 1.0f + 0.4f * pop_progress : 1.0f;

	mesh->set_scale(scale * grow * swell);
}

void Bubble::_physics_process(double p_delta) {
	if (Engine::get_singleton()->is_editor_hint()) {
		return;
	}
	float dt = (float)p_delta;
	if (dt <= 0.0f) {
		return;
	}
	age += dt;
	if (pop_progress >= 0.0f) {
		_tick_pop(dt);
	} else {
		_tick_float(dt);
	}
	_apply_wobble();
}

} // namespace godot
