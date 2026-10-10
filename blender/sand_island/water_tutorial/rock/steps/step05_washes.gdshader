// STEP 5 — watercolour washes: big soft blotches across the rock.
// A painted rock is never one flat wash per face. The pigment pools, so some areas come out lighter
// and some darker, in big soft patches that run ACROSS facet edges. That ties the facets together
// into one rock, while step 2 pulls them apart. We use shader_lib's tex_noise in 3D world space
// (2 texture reads), so the patches wrap around the rock in every direction with no seams and no UVs.
// One big octave and one smaller one, as always.
shader_type spatial;

#include "res://shader_lib/hash.gdshaderinc"
#include "res://shader_lib/texture_noise.gdshaderinc"

uniform vec3 rock_colour : source_color = vec3(0.74, 0.68, 0.62);
uniform float facet_grid = 24.0;      // 24 for flat-shaded rocks, ~5 for smooth meshes
uniform float facet_value = 0.16;
uniform float facet_hue = 0.1;
uniform vec3 top_tint : source_color = vec3(1.0, 0.97, 0.88);
uniform vec3 under_tint : source_color = vec3(0.78, 0.74, 0.9);
uniform float wash_scale = 0.9;        // NEW: 1/metres: patch size ≈ 1 m
uniform float wash_strength = 0.24;    // NEW: ± brightness of the washes
uniform vec3 lit_colour : source_color = vec3(1.0, 0.95, 0.85);   // warm sun
uniform vec3 mid_colour : source_color = vec3(0.8, 0.8, 0.88);    // cool half-tone
uniform vec3 shadow_tint : source_color = vec3(0.6, 0.58, 0.8);
uniform float shadow_light : hint_range(0.0, 1.0) = 0.35;
uniform float band_mid = 0.05;          // N·L where shadow turns to half-tone
uniform float band_lit = 0.45;          // N·L where half-tone turns to lit
uniform float band_softness = 0.05;

varying vec3 world_pos;
varying vec3 world_normal;

void vertex() {
	world_pos = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;
	world_normal = normalize((MODEL_MATRIX * vec4(NORMAL, 0.0)).xyz);
}

void fragment() {
	float up = world_normal.y;

	vec3 facet_id = floor(world_normal * facet_grid + 0.5) + floor(NODE_POSITION_WORLD * 3.0) * 7.0;
	float r1 = hash(facet_id);
	float r2 = hash(facet_id + 11.0);
	vec3 colour = rock_colour * (1.0 + (r1 - 0.5) * 2.0 * facet_value);
	colour *= 1.0 + vec3(1.0, 0.0, -1.0) * facet_hue * (r2 - 0.5) * 2.0;

	colour *= mix(vec3(1.0), top_tint * 1.15, smoothstep(0.3, 0.8, up));
	colour *= mix(vec3(1.0), under_tint, smoothstep(-0.1, -0.6, up));

	float wash = tex_noise(world_pos * wash_scale) * 0.7 + tex_noise(world_pos * wash_scale * 2.7 + 9.0) * 0.3;  // NEW
	colour *= 1.0 + (wash - 0.5) * 2.0 * wash_strength;                                                     // NEW

	ALBEDO = colour;
	ROUGHNESS = 1.0;
	SPECULAR = 0.0;
}

void light() {
	float ndl = dot(NORMAL, LIGHT) * ATTENUATION;
	float mid = smoothstep(band_mid - band_softness, band_mid + band_softness, ndl);
	float lit = smoothstep(band_lit - band_softness, band_lit + band_softness, ndl);
	vec3 light_col = mix(shadow_tint * shadow_light, mid_colour * 0.7, mid);          // shadow → half-tone
	light_col = mix(light_col, lit_colour, lit);                                       // → lit
	DIFFUSE_LIGHT += LIGHT_COLOR / PI * light_col;
}
