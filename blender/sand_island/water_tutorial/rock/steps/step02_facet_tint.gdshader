// STEP 2 — every facet gets its own slight colour.
// Painters never paint two faces of a rock the same colour. We need one random number per FACET,
// and flat shading gives it to us for free: the normal is the same everywhere on a facet and
// different on the next one. So hash(the normal) is a facet ID, with no extra data in the mesh. We add
// the rock's own position (NODE_POSITION_WORLD) so two rocks with a face pointing the same way still
// differ. Each facet then shifts a little lighter or darker, and warmer or cooler.
shader_type spatial;

#include "res://shader_lib/hash.gdshaderinc"

uniform vec3 rock_colour : source_color = vec3(0.74, 0.68, 0.62);
uniform float facet_grid = 24.0;      // NEW: normal rounding: 24 = every flat facet its own colour;
                                      //      ~5 on SMOOTH meshes (big painted planes, not shattered)
uniform float facet_value = 0.16;       // NEW: ± brightness per facet
uniform float facet_hue = 0.1;        // NEW: ± warm/cool per facet
uniform vec3 shadow_tint : source_color = vec3(0.6, 0.58, 0.8);
uniform float shadow_light : hint_range(0.0, 1.0) = 0.35;

varying vec3 world_pos;
varying vec3 world_normal;

void vertex() {
	world_pos = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;
	world_normal = normalize((MODEL_MATRIX * vec4(NORMAL, 0.0)).xyz);
}

void fragment() {
	// NEW: one random pair per facet: round the normal to a grid so the whole facet hits the same cell
	vec3 facet_id = floor(world_normal * facet_grid + 0.5) + floor(NODE_POSITION_WORLD * 3.0) * 7.0;
	float r1 = hash(facet_id);
	float r2 = hash(facet_id + 11.0);
	vec3 colour = rock_colour * (1.0 + (r1 - 0.5) * 2.0 * facet_value);               // NEW: lighter / darker
	colour *= 1.0 + vec3(1.0, 0.0, -1.0) * facet_hue * (r2 - 0.5) * 2.0;               // NEW: warmer (+red −blue) / cooler

	ALBEDO = colour;
	ROUGHNESS = 1.0;
	SPECULAR = 0.0;
}

void light() {
	float lit = smoothstep(0.15, 0.3, dot(NORMAL, LIGHT) * ATTENUATION);
	DIFFUSE_LIGHT += LIGHT_COLOR / PI * mix(shadow_tint * shadow_light, vec3(1.0), lit);
}
