shader_type spatial;

// =============================================================================
// Folio "WaterSurface"  (folio World/WaterSurface.js)
//
// A water plane at the water elevation. folio's quality-0 path makes the surface
// OPAQUE and chooses its colour per fragment:
//   - details mask >= 0.5 -> the white water surface (foam / shore / ripples /
//                            ice / rain splashes), lit + fogged through the
//                            shared folio pipeline
//   - details mask <  0.5 -> a BLURRED sample of the scene already rendered
//                            behind the plane (screen texture) => translucent,
//                            tinted "deep water" refraction.
// The mask is the union of animated ripple bands + shoreline + ice sheet + rain
// splashes, read from the terrain data map's B (elevation) channel and gated by
// the live Weather globals (temperature -> ripples/ice, rain -> splashes).
//
// Reading the screen texture makes Godot render this in the transparent pass
// (after the opaque floor/background), so the behind-surface sample is correct.
//
// Ported: ripples + shore + refraction blur + ice + rain splashes + weather
// gating. Deferred vs folio: drop shadows on the surface, ice physics/friction,
// camera-follow recentring. Ripples/splashes use the shared shader clock in
// place of folio's (unported) Wind.localTime.
// =============================================================================

render_mode unshaded, cull_disabled;

#include "res://material/shaders/folio/folio_material.gdshaderinc"

global uniform float folio_elapsed_scaled; // stands in for folio Wind.localTime
global uniform sampler2D folio_noise_perlin : repeat_enable, filter_linear;
global uniform sampler2D folio_noise_voronoi : repeat_enable, filter_linear;
uniform sampler2D screen_tex : hint_screen_texture, filter_linear;

// Live weather (FolioWeather globals). Ratios are derived from these per folio.
global uniform float folio_weather_temperature; // deg C
global uniform float folio_weather_rain; // 0..1

// Ripple / shore tunables (folio WaterSurface uniforms).
uniform float ripples_slope_frequency = 10.0;
uniform float ripples_noise_frequency = 0.1;
uniform float ripples_noise_offset = 0.345;
uniform float shore_edge = 0.17;

// Ice / splashes tunables (folio WaterSurface uniforms).
uniform float ice_noise_frequency = 0.3;
uniform float splashes_noise_frequency = 0.33;
uniform float splashes_time_frequency = 6.0;
uniform float splashes_thickness = 0.3;
uniform float splashes_edge_low = 0.14;
uniform float splashes_edge_high = 1.0;

// Refraction blur (folio quality-0 hashBlur).
uniform float refraction_blur_amount = 0.012;
uniform int refraction_blur_repeats : hint_range(1, 40) = 25;

const float WATER_TAU = 6.28318530718;

float water_hash(vec2 p) {
	vec3 p3 = fract(vec3(p.xyx) * 0.1031);
	p3 += dot(p3, p3.yzx + 33.33);
	return fract((p3.x + p3.y) * p3.z);
}

// folio hash(float): 1D value hash used to randomise per-splash time/visibility.
float water_hash1(float p) {
	p = fract(p * 0.1031);
	p *= p + 33.33;
	p *= p + p;
	return fract(p);
}

float water_remap_clamp(float v, float in_min, float in_max, float out_min, float out_max) {
	float t = (in_max - in_min);
	float r = (t == 0.0) ? out_min : (out_min + (out_max - out_min) * (v - in_min) / t);
	return clamp(r, min(out_min, out_max), max(out_min, out_max));
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
// ripples_ratio from temperature: full when warm, fades to 0 approaching freezing.
float ripples_node(float b, vec2 world_xz, float ripples_ratio) {
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

// folio iceNode: a voronoi-broken sheet that grows from shore (high b) to open
// water (low b) as ice_ratio rises with cold. Step direction chosen so colder
// weather visibly adds ice (folio's TSL method form is ambiguous in GLSL).
float ice_node(float b, vec2 world_xz, float ice_ratio) {
	float ice_voronoi = texture(folio_noise_voronoi, world_xz * ice_noise_frequency).r;
	float ice_edge = 1.0 - ice_ratio; // colder -> lower edge -> more coverage
	float b_jittered = b - (ice_voronoi - 0.5) * 0.15; // break the boundary
	return step(ice_edge, b_jittered) * step(0.0001, ice_ratio);
}

// folio splashesNode: scattered expanding rain rings, gated by splashes_ratio.
float splashes_node(vec2 world_xz, float splashes_ratio) {
	vec3 sv = texture(folio_noise_voronoi, world_xz * splashes_noise_frequency).rgb;
	float sp = texture(folio_noise_perlin, world_xz * splashes_noise_frequency * 0.25).r;

	float splash = sv.r;
	float splash_time_random = water_hash1(sv.b * 123456.0) + sp;
	float splash_time = folio_elapsed_scaled * splashes_time_frequency + splash_time_random;
	splash = mod(splash - splash_time, 1.0);

	float edge_multiplier = water_remap_clamp(sv.g, splashes_edge_low, splashes_edge_high, 0.0, 1.0);
	float thickness = splashes_thickness * edge_multiplier;
	// Thin dot while the cell's phase is early (splash < thickness), so each cell
	// blinks a small splash mark rather than flooding white.
	splash = 1.0 - step(thickness, splash);

	// Per-cell visibility: a cell is active when its random value is under the
	// splashes_ratio, so heavier rain lights up more cells.
	float visibility_random = water_hash1(sv.b * 654321.0);
	float visible = mod(visibility_random + sp, 1.0);
	visible = step(visible, splashes_ratio);
	return splash * visible;
}

float details_mask(vec2 world_xz) {
	float b = folio_terrain_sample(world_xz).b;

	// Weather-driven ratios (folio bindings).
	float ripples_ratio = water_remap_clamp(folio_weather_temperature, 0.0, -3.0, 1.0, 0.0);
	float ice_ratio = water_remap_clamp(folio_weather_temperature, 0.0, -5.0, 0.0, 1.0);
	float splashes_ratio = folio_weather_rain * folio_weather_rain; // pow(rain, 2)

	float value = 0.0;
	value = max(value, ripples_node(b, world_xz, ripples_ratio));
	value = max(value, ice_node(b, world_xz, ice_ratio));
	value = max(value, splashes_node(world_xz, splashes_ratio));
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
		// Surface / foam / ice / splashes: white, lit + fogged through the pipeline.
		ALBEDO = folio_shade(vec3(1.0), SCREEN_UV, -VERTEX.z);
	}
}
