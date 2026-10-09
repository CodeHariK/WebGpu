// STEP 3 — no snow at the water's edge, and wet rock there.
// Waves keep the shore clear, so snow also needs a height test: by_height = smoothstep(snow_line,
// snow_line + 0.12, h). MULTIPLY the two masks, so there is snow only where both say yes: facing
// up AND above the line (a × b is the shader way to say "a and b"). Just above the water the rock
// gets a dark wet band, like wet sand.
shader_type spatial;

#include "res://shader_lib/texture_noise.gdshaderinc"

uniform float water_level = 0.0;
uniform float snow_line = 0.18;        // NEW: no snow below this height above the water
uniform float slope_min = 0.55;
uniform float slope_full = 0.78;
uniform float snow_everywhere : hint_range(0.0, 1.0) = 0.0;
uniform vec3 snow_colour : source_color = vec3(1.0);
uniform vec3 rock : source_color = vec3(0.46, 0.49, 0.58);
uniform vec3 rock_light : source_color = vec3(0.62, 0.64, 0.72);
uniform vec3 rock_wet : source_color = vec3(0.28, 0.31, 0.4);  // NEW
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
	float up = world_normal.y;
	vec2 p = world_pos.xz;

	float tone = tex_noise2(p * 0.4 + 1.0 * NOISE_LAYER);
	float strata = smoothstep(0.55, 0.8, sin(world_pos.y * 7.0 + tex_noise2(p * 0.5 + 3.0 * NOISE_LAYER) * 3.0) * 0.5 + 0.5);
	vec3 rock_col = mix(rock, rock_light, tone * 0.6 + strata * 0.4);
	rock_col = mix(rock_wet, rock_col, smoothstep(0.0, 0.14, h));  // NEW: wet band at the waterline
	rock_col = mix(rock_col, seabed, smoothstep(0.0, -0.8, h));

	float by_slope = smoothstep(slope_min, slope_full, up);
	float by_height = smoothstep(snow_line, snow_line + 0.12, h);  // NEW
	float cover = max(by_slope * by_height, snow_everywhere);       // NEW: both must say yes
	vec3 colour = mix(rock_col, snow_colour, cover);

	ALBEDO = colour;
	ROUGHNESS = 1.0;
	SPECULAR = 0.0;
}

void light() {
	float ndl = dot(NORMAL, LIGHT);
	float rock_lit = smoothstep(band_lit - band_softness, band_lit + band_softness, ndl * ATTENUATION);
	DIFFUSE_LIGHT += LIGHT_COLOR / PI * mix(rock_shadow * rock_shadow_light, vec3(1.0), rock_lit);
}
