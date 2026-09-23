shader_type spatial;

// =============================================================================
// Folio "Snow" — ground accumulation  (folio World/Snow.js)
//
// A camera-following snow sheet that LIES ON the terrain (same height source as
// the floor: folio_terrain_data.b * -1.5) plus perlin snow lumps scaled by
// `coverage`. Uses the shared folio material pipeline (folio_shade /
// folio_light_shadow) so the snow lights, shadows and fogs exactly like the
// world. `coverage` (0..1, from FolioSnow's weather accumulation) fades the sheet
// in and scales the lumps; the sheet fades out over water and toward its roaming
// edges. A hash/perlin glitter adds sparkle.
//
// Deferred vs folio: the RT elevation field and wheel tracks (FolioTracks).
// =============================================================================

render_mode cull_disabled, depth_prepass_alpha;

#include "res://material/shaders/folio/folio_material.gdshaderinc"

// Noise (published by FolioNoises; not part of the material include).
global uniform sampler2D folio_noise_perlin : repeat_enable, filter_linear;
global uniform sampler2D folio_noise_hash : repeat_enable, filter_linear;

uniform float coverage : hint_range(0.0, 1.0) = 0.0; // fade-in + lump-height scale
uniform float half_size = 50.0; // roaming-plane half extent (edge fade)
uniform float edge_fade_start = 0.75;

uniform float lump_height = 0.35;
uniform float lump_frequency = 0.06;

uniform vec3 snow_color : source_color = vec3(0.92, 0.95, 1.0);

// Fade the snow out over water (folio pushes it down + hides it there).
uniform float water_edge_low = 0.16;
uniform float water_edge_high = 0.24;

uniform float glitter_variation = 0.0;
uniform float glitter_frequency = 0.5;
uniform float glitter_scarcity = 8.0;
uniform float glitter_intensity = 1.5;

varying float v_plane_edge; // 1 in the middle, 0 at the roaming plane's rim
varying float v_coverage_frag;

float snow_lump(vec2 wxz) {
	float n1 = texture(folio_noise_perlin, wxz * lump_frequency).r;
	float n2 = texture(folio_noise_perlin, wxz * lump_frequency * 0.4).r;
	return n1 * n2 * lump_height * coverage;
}

// Surface height at world xz: terrain (matches the floor) + snow lumps.
float snow_height(vec2 wxz) {
	vec4 data = folio_terrain_sample(wxz);
	return data.b * -1.5 + snow_lump(wxz);
}

vec3 snow_base_color(vec2 wxz) {
	float g = texture(folio_noise_hash, wxz * glitter_frequency + vec2(glitter_variation)).r;
	g = abs(g * 2.0 - 1.0);
	g = pow(g, glitter_scarcity);
	float pmask = smoothstep(0.0, 0.5, texture(folio_noise_perlin, wxz * 0.05).r);
	g *= pmask * glitter_intensity * coverage; // never sparkle more than the snow itself
	return snow_color + vec3(g);
}

void vertex() {
	vec2 wxz = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xz;

	float h = snow_height(wxz);
	VERTEX.y += h;

	// Normal from terrain+lump neighbours.
	float d = 0.75;
	float hx = snow_height(wxz + vec2(d, 0.0));
	float hz = snow_height(wxz + vec2(0.0, d));
	vec3 n = normalize(vec3(h - hx, d, h - hz));

	// Fade the square roaming boundary out.
	vec2 lc = abs(VERTEX.xz) / half_size;
	v_plane_edge = 1.0 - smoothstep(edge_fade_start, 1.0, max(lc.x, lc.y));
	v_coverage_frag = coverage;

	v_world_pos = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;
	v_world_normal = n;
}

void fragment() {
	ALBEDO = folio_shade(snow_base_color(v_world_pos.xz), SCREEN_UV, -VERTEX.z);

	// Hide the snow over water and fade it in with coverage + plane edge.
	vec4 data = folio_terrain_sample(v_world_pos.xz);
	float water_fade = 1.0 - smoothstep(water_edge_low, water_edge_high, data.b);
	ALPHA = clamp(v_coverage_frag, 0.0, 1.0) * v_plane_edge * water_fade;
}

void light() {
	DIFFUSE_LIGHT = folio_light_shadow(ALBEDO, snow_base_color(v_world_pos.xz), ATTENUATION);
}
