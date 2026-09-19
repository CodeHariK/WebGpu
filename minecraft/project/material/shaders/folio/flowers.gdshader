shader_type spatial;

// =============================================================================
// Folio "Flowers"  (folio World/Flowers.js)
//
// Tiny flower tufts: a few small solid-colour quads per cluster, instanced on the
// grass. No leaf mask — just a coloured speck. Upper verts sway with the wind
// field (folio: offset scaled by clamp(localY,0,1)). Lit through the shared folio
// pipeline; double-sided.
// =============================================================================

render_mode cull_disabled, ambient_light_disabled;

#include "res://material/shaders/folio/folio_material.gdshaderinc"

global uniform vec2 folio_wind_direction;
global uniform float folio_wind_position_frequency;
global uniform float folio_wind_strength;
global uniform float folio_wind_time;
global uniform sampler2D folio_noise_perlin : repeat_enable, filter_linear;

uniform vec4 flower_color : source_color = vec4(0.95, 0.93, 0.98, 1.0);

vec2 fl_wind_offset(vec2 pos) {
	vec2 rp = pos * folio_wind_position_frequency;
	vec2 uv1 = rp * 0.2 + folio_wind_direction * folio_wind_time;
	float n1 = textureLod(folio_noise_perlin, uv1, 0.0).r - 0.5;
	vec2 uv2 = rp * 0.1 + folio_wind_direction * folio_wind_time * 0.2;
	float n2 = textureLod(folio_noise_perlin, uv2, 0.0).r - 0.5;
	return folio_wind_direction * (n1 + n2) * folio_wind_strength;
}

void vertex() {
	float mult = clamp(VERTEX.y, 0.0, 1.0); // only upper verts sway
	vec2 w = fl_wind_offset(VERTEX.xz);
	vec3 v = VERTEX + vec3(w.x, 0.0, w.y) * mult;
	v_world_pos = (MODEL_MATRIX * vec4(v, 1.0)).xyz;
	v_world_normal = normalize(mat3(MODEL_MATRIX) * NORMAL);
	VERTEX = v;
}

void fragment() {
	ALBEDO = folio_shade(flower_color.rgb, SCREEN_UV, -VERTEX.z);
}

void light() {
	DIFFUSE_LIGHT = folio_light_shadow(ALBEDO, flower_color.rgb, ATTENUATION);
}
