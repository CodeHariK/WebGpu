// STEP 2 — snow where the ground faces UP.
// The world normal's y is 1 on flat ground and 0 on a vertical cliff. Snow piles up on flat tops
// and slides off steep sides, so the amount of snow is smoothstep(slope_min, slope_full, normal.y):
// 0 on cliffs, 1 on flat ground. This one line is the core of every snow shader. The snow is pure
// white for now and still uses the rock's toon light. Notice it looks flat and blown out, and goes
// all the way into the sea (fixed in steps 3, 5 and 6).
shader_type spatial;

#include "res://shader_lib/texture_noise.gdshaderinc"

uniform float water_level = 0.0;
uniform float slope_min = 0.55;        // NEW: normal.y where snow starts to stick (≈ 57° slope)
uniform float slope_full = 0.78;       // NEW: fully covered when at least this flat
uniform float snow_everywhere : hint_range(0.0, 1.0) = 0.0;  // NEW: 1 = ignore the slope (snow-cap sheets)
uniform vec3 snow_colour : source_color = vec3(1.0);         // NEW: pure white (see step 6)
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
	float up = world_normal.y;  // NEW: 1 = facing up, 0 = vertical
	vec2 p = world_pos.xz;

	float tone = tex_noise2(p * 0.4 + 1.0 * NOISE_LAYER);
	float strata = smoothstep(0.55, 0.8, sin(world_pos.y * 7.0 + tex_noise2(p * 0.5 + 3.0 * NOISE_LAYER) * 3.0) * 0.5 + 0.5);
	vec3 rock_col = mix(rock, rock_light, tone * 0.6 + strata * 0.4);
	rock_col = mix(rock_col, seabed, smoothstep(0.0, -0.8, h));

	float cover = max(smoothstep(slope_min, slope_full, up), snow_everywhere);  // NEW: how much snow
	vec3 colour = mix(rock_col, snow_colour, cover);                            // NEW

	ALBEDO = colour;
	ROUGHNESS = 1.0;
	SPECULAR = 0.0;
}

void light() {
	float ndl = dot(NORMAL, LIGHT);
	float rock_lit = smoothstep(band_lit - band_softness, band_lit + band_softness, ndl * ATTENUATION);
	DIFFUSE_LIGHT += LIGHT_COLOR / PI * mix(rock_shadow * rock_shadow_light, vec3(1.0), rock_lit);
}
