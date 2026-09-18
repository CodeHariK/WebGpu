shader_type spatial;

// =============================================================================
// Folio "WaterSurface"  (folio World/WaterSurface.js)
//
// A transparent plane at the water elevation. Its ALPHA is a "details mask": the
// union of animated ripple bands and a shoreline band, both read from the shared
// terrain data map's B (elevation) channel. Where the mask is ~0 the surface is
// see-through (deep water shows the floor beneath); where it's ~1 it paints the
// white water surface (foam / shore / ripple crests), lit through the shared
// folio pipeline (drop shadows + fog).
//
// Ported: ripples (default on) + shore. Deferred vs folio: ice + splashes +
// weather gating (need Weather), the quality-0 screen-blur refraction
// (viewportSharedTexture hashBlur), and ice physics. Ripples use the shared
// shader clock in place of folio's (unported) Wind.localTime.
// =============================================================================

render_mode cull_disabled, ambient_light_disabled, depth_draw_never;

#include "res://material/shaders/folio/folio_material.gdshaderinc"

global uniform float folio_elapsed_scaled; // stands in for folio Wind.localTime
global uniform sampler2D folio_noise_perlin : repeat_enable, filter_linear;

// Tunables (folio WaterSurface uniforms).
uniform float ripples_ratio = 1.0; // folio: from temperature; 1 = full ripples
uniform float ripples_slope_frequency = 10.0;
uniform float ripples_noise_frequency = 0.1;
uniform float ripples_noise_offset = 0.345;
uniform float shore_edge = 0.17;

// folio ripplesNode: animated bands flowing by elevation + time, broken up by noise.
float ripples_node(float b, vec2 world_xz) {
	float ripple_index = floor((b + folio_elapsed_scaled * 0.5) * ripples_slope_frequency);
	float ripples_noise = texture(folio_noise_perlin,
			(world_xz + ripple_index / ripples_noise_offset) * ripples_noise_frequency).r;

	float ripples = mod((b + folio_elapsed_scaled * 0.5) * ripples_slope_frequency, 1.0)
			- (1.0 - ((b * 1.3) - 0.3)) // remap(b,0,1,-0.3,1) = b*1.3 - 0.3
			+ ripples_noise;

	// step(remap(ripplesRatio,0,1,-1,-0.4), ripples)
	float edge = mix(-1.0, -0.4, ripples_ratio);
	return step(edge, ripples);
}

// folio shoreNode: step(shoreEdge, b)
float shore_node(float b) {
	return step(shore_edge, b);
}

// The details mask = union of the enabled detail layers (ripples + shore).
float details_mask(vec2 world_xz) {
	float b = folio_terrain_sample(world_xz).b;
	float value = 0.0;
	value = max(value, ripples_node(b, world_xz));
	value = max(value, shore_node(b));
	return value;
}

void vertex() {
	// Pin the plane to the water elevation (folio sets mesh.position.y to it).
	VERTEX.y += folio_water_surface_elevation;
	v_world_pos = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;
	v_world_normal = vec3(0.0, 1.0, 0.0);
}

void fragment() {
	// folio water material: colorNode = white, no bounce, no water flatten, no core
	// shadow, no reveal; keep fog. alpha uniform stays 1 (folio_shade won't discard).
	ALBEDO = folio_shade(vec3(1.0), SCREEN_UV, -VERTEX.z);
	ALPHA = details_mask(v_world_pos.xz);
}

void light() {
	DIFFUSE_LIGHT = folio_light_shadow(ALBEDO, vec3(1.0), ATTENUATION);
}
