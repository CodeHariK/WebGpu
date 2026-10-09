// STEP 3 — zones: grass on top, earth on the steep bank, sand everywhere else.
// Two facts per pixel decide the zone: its height (step 2) and how steep the ground is. The normal
// is the direction the surface faces; in world space its y is 1 on flat ground and 0 on a wall.
// smoothstep turns each fact into a soft 0..1 mask, and mix() paints each zone over the last.
shader_type spatial;

uniform vec3 sand_dry : source_color = vec3(0.98, 0.86, 0.62);
uniform vec3 grass : source_color = vec3(0.42, 0.74, 0.3);  // NEW
uniform vec3 bank : source_color = vec3(0.92, 0.55, 0.3);   // NEW
uniform float water_level = 0.0;
uniform float grass_height = 1.45;  // NEW: grass starts this high above the water

varying vec3 world_pos;
varying vec3 world_normal;  // NEW

void vertex() {
	world_pos = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;
	world_normal = normalize((MODEL_MATRIX * vec4(NORMAL, 0.0)).xyz);  // NEW: normal → world space
}

void fragment() {
	float h = world_pos.y - water_level;
	float up = world_normal.y;  // NEW: 1 = flat, 0 = vertical
	float is_grass = smoothstep(grass_height - 0.08, grass_height + 0.02, h) * smoothstep(0.6, 0.8, up);  // NEW: high AND flat
	float is_bank = (1.0 - smoothstep(0.65, 0.85, up)) * smoothstep(0.17, 0.32, h);                      // NEW: steep AND above the beach
	vec3 colour = mix(sand_dry, bank, is_bank);  // NEW
	colour = mix(colour, grass, is_grass);       // NEW
	ALBEDO = colour;
	ROUGHNESS = 1.0;
}
