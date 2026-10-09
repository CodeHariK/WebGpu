// STEP 1 — rock everywhere, toon light.
// Snow is a LAYER on top of something, so we start with what is underneath: grey-blue rock with soft
// colour patches (noise) and wavy strata stripes (sin of the height, bent by noise). Under the water
// it fades to seabed. It is lit with the same two-band toon light as the sand (sand step 10).
// Every later step only adds snow on top of this.
shader_type spatial;

#include "res://shader_lib/texture_noise.gdshaderinc"

uniform float water_level = 0.0;
uniform vec3 rock : source_color = vec3(0.46, 0.49, 0.58);
uniform vec3 rock_light : source_color = vec3(0.62, 0.64, 0.72);
uniform vec3 seabed : source_color = vec3(0.4, 0.55, 0.62);
uniform vec3 rock_shadow : source_color = vec3(0.7, 0.68, 0.9);
uniform float rock_shadow_light : hint_range(0.0, 1.0) = 0.35;
uniform float band_lit = 0.25;
uniform float band_softness = 0.12;

varying vec3 world_pos;
varying vec3 world_normal;

void vertex() {
	world_pos = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;
	world_normal = normalize((MODEL_MATRIX * vec4(NORMAL, 0.0)).xyz);
}

void fragment() {
	float h = world_pos.y - water_level;
	vec2 p = world_pos.xz;

	float tone = tex_noise2(p * 0.4 + 1.0 * NOISE_LAYER);  // soft colour patches

	float strata = smoothstep(0.55, 0.8, sin(world_pos.y * 7.0 + tex_noise2(p * 0.5 + 3.0 * NOISE_LAYER) * 3.0) * 0.5 + 0.5);  // wavy stripes
	vec3 rock_col = mix(rock, rock_light, tone * 0.6 + strata * 0.4);
	rock_col = mix(rock_col, seabed, smoothstep(0.0, -0.8, h));

	ALBEDO = rock_col;
	ROUGHNESS = 1.0;
	SPECULAR = 0.0;
}

void light() {
	float ndl = dot(NORMAL, LIGHT);
	float rock_lit = smoothstep(band_lit - band_softness, band_lit + band_softness, ndl * ATTENUATION);
	DIFFUSE_LIGHT += LIGHT_COLOR / PI * mix(rock_shadow * rock_shadow_light, vec3(1.0), rock_lit);
}
