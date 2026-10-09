// STEP 6 — snow colour: not pure white, with bluer patches.
// In a custom light() Godot gives you LIGHT_COLOR already multiplied by π, so sun + sky add up to
// more than 1. Pure white (1.0) then clips: every lit pixel becomes the same flat white and all the
// soft shading from step 5 disappears. About 88% white leaves room for it. Big, soft noise patches
// of slightly bluer snow give the surface some variety.
shader_type spatial;

#include "res://shader_lib/texture_noise.gdshaderinc"

uniform float water_level = 0.0;
uniform float snow_line = 0.18;
uniform float slope_min = 0.55;
uniform float slope_full = 0.78;
uniform float edge_noise = 0.3;
uniform float snow_everywhere : hint_range(0.0, 1.0) = 0.0;
uniform vec3 snow_colour : source_color = vec3(0.86, 0.88, 0.92);  // NEW: not pure white
uniform vec3 snow_cool : source_color = vec3(0.74, 0.8, 0.93);     // NEW: patches of bluer snow
uniform vec3 snow_edge : source_color = vec3(0.62, 0.72, 0.94);
uniform vec3 rock : source_color = vec3(0.46, 0.49, 0.58);
uniform vec3 rock_light : source_color = vec3(0.62, 0.64, 0.72);
uniform vec3 rock_wet : source_color = vec3(0.28, 0.31, 0.4);
uniform vec3 seabed : source_color = vec3(0.4, 0.55, 0.62);
uniform vec3 shadow_colour : source_color = vec3(0.52, 0.64, 0.92);
uniform float shadow_light : hint_range(0.0, 1.0) = 0.6;
uniform float snow_wrap : hint_range(0.0, 1.0) = 0.35;
uniform float snow_soft : hint_range(0.05, 1.0) = 0.7;
uniform vec3 rock_shadow : source_color = vec3(0.7, 0.68, 0.9);
uniform float rock_shadow_light : hint_range(0.0, 1.0) = 0.35;
uniform float band_lit = 0.25;
uniform float band_softness = 0.12;

varying vec3 world_pos;
varying vec3 world_normal;
varying float snow;

void vertex() {
	world_pos = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;
	world_normal = normalize((MODEL_MATRIX * vec4(NORMAL, 0.0)).xyz);
}

void fragment() {
	float h = world_pos.y - water_level;
	float up = world_normal.y;
	vec2 p = world_pos.xz;

	float tone = tex_noise2(p * 0.4 + 1.0 * NOISE_LAYER);
	float strata = smoothstep(0.55, 0.8, sin(world_pos.y * 7.0 + tex_noise2(p * 0.5 + 3.0 * NOISE_LAYER) * 3.0) * 0.5 + 0.5);
	vec3 rock_col = mix(rock, rock_light, tone * 0.6 + strata * 0.4);
	rock_col = mix(rock_wet, rock_col, smoothstep(0.0, 0.14, h));
	rock_col = mix(rock_col, seabed, smoothstep(0.0, -0.8, h));

	float ragged = (tex_noise2(p * 1.3 + 5.0 * NOISE_LAYER) - 0.5) * edge_noise;
	float by_slope = smoothstep(slope_min, slope_full, up + ragged);
	float by_height = smoothstep(snow_line, snow_line + 0.12, h + ragged * 0.5);
	float cover = max(by_slope * by_height, snow_everywhere);
	float s = smoothstep(0.45, 0.55, cover);
	float edge = smoothstep(0.25, 0.45, cover) * (1.0 - s);

	float patches = tex_noise2(p * 0.3 + 6.0 * NOISE_LAYER);                              // NEW: big blobs (~3 m)
	vec3 snow_col = mix(snow_colour, snow_cool, smoothstep(0.45, 0.85, patches) * 0.7);  // NEW

	vec3 colour = mix(rock_col, snow_edge, edge);
	colour = mix(colour, snow_col, s);  // NEW: snow_col instead of snow_colour

	snow = s;
	ALBEDO = colour;
	ROUGHNESS = 1.0;
	SPECULAR = 0.0;
}

void light() {
	float ndl = dot(NORMAL, LIGHT);
	float rock_lit = smoothstep(band_lit - band_softness, band_lit + band_softness, ndl * ATTENUATION);
	vec3 rock_light_col = mix(rock_shadow * rock_shadow_light, vec3(1.0), rock_lit);

	float wrapped = (ndl + snow_wrap) / (1.0 + snow_wrap);
	float snow_lit = smoothstep(0.0, snow_soft, wrapped) * ATTENUATION;
	vec3 snow_light_col = mix(shadow_colour * shadow_light, vec3(1.0), snow_lit);

	DIFFUSE_LIGHT += LIGHT_COLOR / PI * mix(rock_light_col, snow_light_col, snow);
}
