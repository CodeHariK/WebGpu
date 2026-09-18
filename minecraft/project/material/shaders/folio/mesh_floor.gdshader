shader_type spatial;

// =============================================================================
// Folio "Floor" material  (folio World/Floor.js)
//
// A displaced ground plane that reads the shared terrain data map and paints the
// island: the terrain gradient/grass base, with a stone "slab" texture blended in
// by the road/dirt channel. Uses the shared folio pipeline (folio_material inc)
// for light/shadow/fog, with light-bounce and water OFF (folio Floor params) and
// a flat up normal.
//
//   base   = terrain.colorNode(data)                       (gradient + grass)
//   slab   = data.r * perlinNoise                          (road/dirt mask)
//   final  = mix(base, mix(slabLow, slabHigh, slabsTex), slab)
//   vertex : y += data.b * -1.5 * edgeFade                 (displacement)
// =============================================================================

render_mode cull_back, ambient_light_disabled;

#include "res://material/shaders/folio/folio_material.gdshaderinc"

// Perlin noise for the slab mask (published by FolioNoises).
global uniform sampler2D folio_noise_perlin : repeat_enable, filter_linear;

// Floor-only detail inputs (folio Floor uniforms).
uniform sampler2D floor_slabs : repeat_enable, filter_linear;
uniform vec4 slab_high_color : source_color = vec4(1.0, 0.812, 0.545, 1.0); // #ffcf8b
uniform vec4 slab_low_color : source_color = vec4(0.659, 0.467, 0.384, 1.0); // #a87762
uniform float slab_texture_frequency = 0.175;
uniform float slab_noise_frequency = 0.03;
uniform float displacement_amount = -1.5;

// The island base colour (terrain gradient/grass + slab detail). Pure (no discard),
// so both fragment() and light() can call it.
vec3 floor_base_color(vec2 world_xz) {
	vec4 data = folio_terrain_sample(world_xz);
	vec3 base = folio_terrain_color(data);

	float slab_terrain = data.r;
	float slab_noise = texture(folio_noise_perlin, world_xz * slab_noise_frequency).r;
	float slabs_tex = texture(floor_slabs, world_xz * slab_texture_frequency).r;
	vec3 slab_color = mix(slab_low_color.rgb, slab_high_color.rgb, slabs_tex);

	float slab = slab_terrain * slab_noise;
	return mix(base, slab_color, slab);
}

void vertex() {
	// World XZ before displacement drives the terrain-data lookup.
	vec3 wp = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;
	vec2 tuv = wp.xz / folio_terrain_subdivision / 1.5 + 0.5;
	float b = textureLod(folio_terrain_data, tuv, 0.0).b;

	// folio: fade displacement to 0 toward the plane edges (uvDim).
	float edge_fade = min(min(UV.x, UV.y) * 20.0, 1.0);
	VERTEX.y += b * displacement_amount * edge_fade;

	// Recompute world pos from the displaced vertex; folio uses a flat up normal.
	v_world_pos = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;
	v_world_normal = vec3(0.0, 1.0, 0.0);
}

void fragment() {
	ALBEDO = folio_shade(floor_base_color(v_world_pos.xz), SCREEN_UV, -VERTEX.z);
}

void light() {
	DIFFUSE_LIGHT = folio_light_shadow(ALBEDO, floor_base_color(v_world_pos.xz), ATTENUATION);
}
