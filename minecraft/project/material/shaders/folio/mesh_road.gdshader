shader_type spatial;

// =============================================================================
// Folio road material (folio World/Scenery.js setRoad): a dark asphalt colour
// (#383039) through the folio pipeline, plus a sparse hash*perlin GLITTER added
// as emission so the flat dark road catches little sparkles like the web.
// =============================================================================

render_mode cull_back, ambient_light_disabled;

#include "res://material/shaders/folio/folio_material.gdshaderinc"

global uniform sampler2D folio_noise_hash : repeat_enable, filter_linear;
global uniform sampler2D folio_noise_perlin : repeat_enable, filter_linear;

uniform vec4 road_color : source_color = vec4(0.220, 0.188, 0.224, 1.0); // #383039
uniform float glitter_scarcity = 35.0;
uniform float glitter_intensity = 0.7;
uniform float glitter_hash_freq = 0.20;
uniform float glitter_perlin_freq = 0.05;
uniform float ambient_floor = 0.14; // dim self-glow so the dark road reads as asphalt, not a void

void vertex() {
	v_world_pos = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;
	v_world_normal = normalize(mat3(MODEL_MATRIX) * NORMAL);
}

// folio: hash^scarcity * intensity * perlin  -> sparse bright specks.
float road_glitter(vec2 wxz) {
	float hash = texture(folio_noise_hash, wxz * glitter_hash_freq).r;
	hash = abs(mod(hash * 2.0, 2.0) - 1.0);
	float g = pow(hash, glitter_scarcity) * glitter_intensity;
	g *= texture(folio_noise_perlin, wxz * glitter_perlin_freq).r;
	return g;
}

void fragment() {
	ALBEDO = folio_shade(road_color.rgb, SCREEN_UV, -VERTEX.z);
	EMISSION = vec3(road_glitter(v_world_pos.xz)) + road_color.rgb * ambient_floor;
}

void light() {
	DIFFUSE_LIGHT = folio_light_shadow(ALBEDO, road_color.rgb, ATTENUATION);
}
