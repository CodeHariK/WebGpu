// STEP 3 — paint by direction: warm tops, purple undersides.
// Before any lighting, a painter already colours a rock by which way each face points. Tops face the
// sky and the sun, so they are lighter and warmer (sun-bleached). Undersides only see light bounced
// up from the ground and sky, so they are cool and purple. The world normal's y says which way a
// facet points, the same test the snow used. We multiply tints so the per-facet variation from
// step 2 survives.
shader_type spatial;

#include "res://shader_lib/hash.gdshaderinc"

uniform vec3 rock_colour : source_color = vec3(0.74, 0.68, 0.62);
uniform float facet_grid = 24.0;      // 24 for flat-shaded rocks, ~5 for smooth meshes
uniform float facet_value = 0.16;
uniform float facet_hue = 0.1;
uniform vec3 top_tint : source_color = vec3(1.0, 0.97, 0.88);    // NEW: × colour on upward faces
uniform vec3 under_tint : source_color = vec3(0.78, 0.74, 0.9);  // NEW: × colour on downward faces
uniform vec3 shadow_tint : source_color = vec3(0.6, 0.58, 0.8);
uniform float shadow_light : hint_range(0.0, 1.0) = 0.35;

varying vec3 world_pos;
varying vec3 world_normal;

void vertex() {
	world_pos = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;
	world_normal = normalize((MODEL_MATRIX * vec4(NORMAL, 0.0)).xyz);
}

void fragment() {
	float up = world_normal.y;  // NEW: 1 up, 0 sideways, −1 down

	vec3 facet_id = floor(world_normal * facet_grid + 0.5) + floor(NODE_POSITION_WORLD * 3.0) * 7.0;
	float r1 = hash(facet_id);
	float r2 = hash(facet_id + 11.0);
	vec3 colour = rock_colour * (1.0 + (r1 - 0.5) * 2.0 * facet_value);
	colour *= 1.0 + vec3(1.0, 0.0, -1.0) * facet_hue * (r2 - 0.5) * 2.0;

	colour *= mix(vec3(1.0), top_tint * 1.15, smoothstep(0.3, 0.8, up));   // NEW: tops lighter, warmer
	colour *= mix(vec3(1.0), under_tint, smoothstep(-0.1, -0.6, up));      // NEW: undersides purple

	ALBEDO = colour;
	ROUGHNESS = 1.0;
	SPECULAR = 0.0;
}

void light() {
	float lit = smoothstep(0.15, 0.3, dot(NORMAL, LIGHT) * ATTENUATION);
	DIFFUSE_LIGHT += LIGHT_COLOR / PI * mix(shadow_tint * shadow_light, vec3(1.0), lit);
}
