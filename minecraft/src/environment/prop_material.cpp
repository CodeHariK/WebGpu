/**
 * @file prop_material.cpp
 * @brief Shared prop material (shader source lives here).
 */
#include "prop_material.h"
#include <godot_cpp/classes/shader.hpp>

namespace godot {

Ref<ShaderMaterial> make_prop_material(bool p_two_sided) {
	Ref<Shader> shader;
	shader.instantiate();
	String head =
			p_two_sided ? "render_mode cull_disabled, depth_draw_opaque;" : "render_mode cull_back, depth_draw_opaque;";
	shader->set_code(String("shader_type spatial;\n") + head + R"(

uniform float wrap : hint_range(0.0, 1.0) = 0.35;              // Diffuse wrap past the terminator
uniform float specular_strength : hint_range(0.0, 2.0) = 0.3;
uniform float specular_power : hint_range(1.0, 256.0) = 28.0;
uniform float rim_strength : hint_range(0.0, 2.0) = 0.5;
uniform float rim_power : hint_range(0.5, 8.0) = 3.0;
uniform vec3 rim_color : source_color = vec3(1.0, 0.95, 1.0);
uniform float transmission : hint_range(0.0, 2.0) = 0.35;      // Light through the object towards the eye
uniform float transmission_power : hint_range(1.0, 32.0) = 6.0;
uniform float glow : hint_range(0.0, 4.0) = 0.0;               // Emission strength at UV.y = 1 (tips)
uniform vec3 glow_color : source_color = vec3(0.8, 0.6, 1.0);
uniform float saturation : hint_range(0.0, 2.0) = 1.0;
global uniform int ts_debug_view;

varying vec3 v_albedo;
varying vec3 v_raw_color;

vec3 srgb_to_linear(vec3 c) {
	return mix(c / 12.92, pow((c + vec3(0.055)) / 1.055, vec3(2.4)), step(vec3(0.04045), c));
}

void vertex() {
	vec3 c = srgb_to_linear(COLOR.rgb);
	float l = dot(c, vec3(0.2126, 0.7152, 0.0722));
	v_albedo = clamp(mix(vec3(l), c, saturation), 0.0, 1.0);
	v_raw_color = COLOR.rgb;
}

void fragment() {
	if (ts_debug_view == 1) {
		ALBEDO = vec3(0.0); EMISSION = vec3(UV.x, fract(UV.y), 0.0);
	} else if (ts_debug_view == 2) {
		ALBEDO = vec3(0.0); EMISSION = vec3(UV2.x, UV2.y, 0.0);
	} else if (ts_debug_view == 3) {
		ALBEDO = vec3(0.0); EMISSION = v_raw_color;
	} else if (ts_debug_view != 0) {
		ALBEDO = vec3(0.0);
	} else {
		ALBEDO = v_albedo;
		ROUGHNESS = 0.55;
		SPECULAR = 0.0; // Highlights are done in light()
		float fresnel = pow(1.0 - clamp(dot(NORMAL, VIEW), 0.0, 1.0), rim_power);
		EMISSION = rim_color * v_albedo * fresnel * rim_strength + glow_color * glow * pow(clamp(UV.y, 0.0, 1.0), 2.0);
	}
}

void light() {
	if (ts_debug_view == 0) {
		vec3 light_col = LIGHT_COLOR / PI; // LIGHT_COLOR carries energy * PI
		float ndl = dot(NORMAL, LIGHT);
		// Wrapped diffuse: light reaches around the terminator like a translucent solid.
		float diffuse = clamp((ndl + wrap) / (1.0 + wrap), 0.0, 1.0);
		DIFFUSE_LIGHT += ALBEDO * light_col * diffuse * ATTENUATION;
		// Tight highlight.
		vec3 h = normalize(LIGHT + VIEW);
		float spec = pow(clamp(dot(NORMAL, h), 0.0, 1.0), specular_power) * specular_strength;
		SPECULAR_LIGHT += light_col * spec * ATTENUATION * step(0.0, ndl);
		// Transmission: the light behind the object glows through it towards the eye.
		float back = pow(clamp(dot(VIEW, -LIGHT), 0.0, 1.0), transmission_power);
		DIFFUSE_LIGHT += ALBEDO * light_col * back * transmission;
	}
}
)");
	Ref<ShaderMaterial> m;
	m.instantiate();
	m->set_shader(shader);
	return m;
}

Ref<ShaderMaterial> make_stone_material() {
	Ref<ShaderMaterial> m = make_prop_material(false);
	m->set_shader_parameter("wrap", 0.15f);
	m->set_shader_parameter("specular_strength", 0.08f);
	m->set_shader_parameter("rim_strength", 0.18f);
	m->set_shader_parameter("transmission", 0.0f);
	m->set_shader_parameter("glow", 0.0f);
	return m;
}

} // namespace godot
