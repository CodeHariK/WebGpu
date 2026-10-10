// STEP 8 — moss on top, darker at the foot.
// Two more painter's habits. MOSS settles where rain and light land: on upward facets, with a ragged
// noise edge (the snow coverage idea in green). It only covers facets that clearly face up, so the
// low-poly shape still shows through. GROUNDING: the base of a rock, where it meets the grass, is
// darker and a little greener, a cheap fake contact shadow (ambient occlusion) that plants the rock
// in the ground instead of letting it float.
// Cost of the moss: the world normal (already there) + 1 texture read for the ragged edge + a few
// multiplies, which is cheap. Because it uses the WORLD normal, a kit rock rotated any way gets moss on
// whichever faces end up on top. The Blender rock stays bare; moss_amount = 0 turns it off here.
shader_type spatial;

#include "res://shader_lib/hash.gdshaderinc"
#include "res://shader_lib/texture_noise.gdshaderinc"

uniform vec3 rock_colour : source_color = vec3(0.74, 0.68, 0.62);
uniform float facet_grid = 24.0;      // 24 for flat-shaded rocks, ~5 for smooth meshes
uniform float facet_value = 0.16;
uniform float facet_hue = 0.1;
uniform vec3 top_tint : source_color = vec3(1.0, 0.97, 0.88);
uniform vec3 under_tint : source_color = vec3(0.78, 0.74, 0.9);
uniform float wash_scale = 0.9;        // 1/metres: patch size ≈ 1 m
uniform float wash_strength = 0.24;    // ± brightness of the washes
uniform vec2 stroke_scale = vec2(8.0, 70.0);  // UV → noise: long along u, thin along v
uniform float stroke_strength = 0.14;
uniform sampler2D rock_rg : hint_default_black, filter_linear_mipmap;  // rock_rg.png (data): R = edges
uniform vec3 edge_colour : source_color = vec3(1.0, 0.95, 0.84);  // warm highlight
uniform float edge_strength : hint_range(0.0, 1.0) = 0.55;
uniform vec3 moss_colour : source_color = vec3(0.47, 0.64, 0.3);  // NEW
uniform vec3 moss_light : source_color = vec3(0.62, 0.76, 0.36);   // NEW
uniform float moss_amount : hint_range(0.0, 1.0) = 1.0;            // NEW: 0 bare rock … 1 mossy tops
uniform float ground_y = 0.0;                                      // NEW: world height of the ground
uniform float foot_height = 0.6;                                   // NEW: metres of darkening above it
uniform vec3 foot_tint : source_color = vec3(0.62, 0.66, 0.58);    // NEW: × colour right at the ground
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

	float wash = tex_noise(world_pos * wash_scale) * 0.7 + tex_noise(world_pos * wash_scale * 2.7 + 9.0) * 0.3;
	colour *= 1.0 + (wash - 0.5) * 2.0 * wash_strength;

	float stroke = tex_noise2(UV * stroke_scale + 21.0 * NOISE_LAYER);  // streaks inside each facet
	colour *= 1.0 + (stroke - 0.5) * 2.0 * stroke_strength;

	float edge = texture(rock_rg, UV).r;                                                        // 1 on the edges
	colour = mix(colour, max(colour * 1.25, edge_colour * 0.85), edge * edge_strength);

	// NEW: moss on upward facets, ragged edge, two greens
	float ragged = (tex_noise2(world_pos.xz * 1.6 + 30.0 * NOISE_LAYER) - 0.5) * 0.5;
	float moss = smoothstep(0.5, 0.56, (smoothstep(0.55, 0.9, up) + ragged) * moss_amount);  // 0 = bare rock
	vec3 moss_col = mix(moss_colour, moss_light, wash) * (1.0 + (r1 - 0.5) * 0.2);
	colour = mix(colour, moss_col, moss);

	// NEW: darker, greener foot (fake contact shadow)
	colour *= mix(foot_tint, vec3(1.0), smoothstep(0.0, foot_height, world_pos.y - ground_y));

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
