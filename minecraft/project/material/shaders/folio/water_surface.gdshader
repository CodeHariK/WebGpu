shader_type spatial;

// =============================================================================
// Folio "WaterSurface"  (folio World/WaterSurface.js)
//
// A water plane at the water elevation. folio's quality-0 path makes the surface
// OPAQUE and chooses its colour per fragment:
//   - details mask >= 0.5 -> the white water surface (foam / shore / ripples),
//                            lit + fogged through the shared folio pipeline
//   - details mask <  0.5 -> a BLURRED sample of the scene already rendered
//                            behind the plane (screen texture) => translucent,
//                            tinted "deep water" refraction.
// The mask is the union of animated ripple bands + a shoreline band, read from
// the terrain data map's B (elevation) channel.
//
// Reading the screen texture makes Godot render this in the transparent pass
// (after the opaque floor/background), so the behind-surface sample is correct.
//
// Ported: ripples (default on) + shore + refraction blur. Deferred vs folio:
// ice + splashes + weather gating (need Weather), drop shadows on the surface,
// ice physics, camera-follow recentring. Ripples use the shared shader clock in
// place of folio's (unported) Wind.localTime.
// =============================================================================

render_mode unshaded, cull_disabled;

#include "res://material/shaders/folio/folio_material.gdshaderinc"

global uniform float folio_elapsed_scaled; // stands in for folio Wind.localTime
global uniform sampler2D folio_noise_perlin : repeat_enable, filter_linear;
uniform sampler2D screen_tex : hint_screen_texture, filter_linear;

// Ripple / shore tunables (folio WaterSurface uniforms).
uniform float ripples_ratio = 1.0; // folio: from temperature; 1 = full ripples
uniform float ripples_slope_frequency = 10.0;
uniform float ripples_noise_frequency = 0.1;
uniform float ripples_noise_offset = 0.345;
uniform float shore_edge = 0.17;

// Refraction blur (folio quality-0 hashBlur).
uniform float refraction_blur_amount = 0.012;
uniform int refraction_blur_repeats : hint_range(1, 40) = 25;

const float WATER_TAU = 6.28318530718;

float water_hash(vec2 p) {
	vec3 p3 = fract(vec3(p.xyx) * 0.1031);
	p3 += dot(p3, p3.yzx + 33.33);
	return fract((p3.x + p3.y) * p3.z);
}

// Golden-angle disk blur of the scene behind the surface (deep-water refraction).
vec3 refraction_blur(vec2 uv) {
	float rot = water_hash(uv) * WATER_TAU;
	vec3 acc = vec3(0.0);
	for (int i = 0; i < refraction_blur_repeats; i++) {
		float t = (float(i) + 0.5) / float(refraction_blur_repeats);
		float ang = rot + float(i) * 2.399963;
		float rad = refraction_blur_amount * sqrt(t);
		acc += texture(screen_tex, uv + vec2(cos(ang), sin(ang)) * rad).rgb;
	}
	return acc / float(refraction_blur_repeats);
}

// folio ripplesNode: animated bands flowing by elevation + time, broken by noise.
float ripples_node(float b, vec2 world_xz) {
	float ripple_index = floor((b + folio_elapsed_scaled * 0.5) * ripples_slope_frequency);
	float ripples_noise = texture(folio_noise_perlin,
			(world_xz + ripple_index / ripples_noise_offset) * ripples_noise_frequency).r;

	float ripples = mod((b + folio_elapsed_scaled * 0.5) * ripples_slope_frequency, 1.0)
			- (1.0 - ((b * 1.3) - 0.3)) // remap(b,0,1,-0.3,1) = b*1.3 - 0.3
			+ ripples_noise;

	float edge = mix(-1.0, -0.4, ripples_ratio);
	return step(edge, ripples);
}

float shore_node(float b) {
	return step(shore_edge, b);
}

float details_mask(vec2 world_xz) {
	float b = folio_terrain_sample(world_xz).b;
	float value = 0.0;
	value = max(value, ripples_node(b, world_xz));
	value = max(value, shore_node(b));
	return value;
}

void vertex() {
	VERTEX.y += folio_water_surface_elevation; // pin to the water plane
	v_world_pos = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;
	v_world_normal = vec3(0.0, 1.0, 0.0);
}

void fragment() {
	float mask = details_mask(v_world_pos.xz);
	if (mask < 0.5) {
		// Deep water: blurred refraction of the scene behind.
		ALBEDO = refraction_blur(SCREEN_UV);
	} else {
		// Surface / foam: white, lit + fogged through the shared pipeline.
		ALBEDO = folio_shade(vec3(1.0), SCREEN_UV, -VERTEX.z);
	}
}
