// STEP 4 — wet sand and seabed.
// Just above the water the sand is darker (wet): a thin band where h < wet_height. Below the water
// the sand fades to a cool sea-floor tone — you see it through the see-through shallows.
// Same tools as before: a height mask from smoothstep, then mix.
shader_type spatial;

uniform vec3 sand_dry : source_color = vec3(0.98, 0.86, 0.62);
uniform vec3 grass : source_color = vec3(0.42, 0.74, 0.3);
uniform vec3 bank : source_color = vec3(0.92, 0.55, 0.3);
uniform float water_level = 0.0;
uniform float grass_height = 1.45;
uniform float wet_height = 0.07;                                 // NEW
uniform vec3 sand_wet : source_color = vec3(0.92, 0.76, 0.54);  // NEW
uniform vec3 seabed : source_color = vec3(0.62, 0.78, 0.7);     // NEW

varying vec3 world_pos;
varying vec3 world_normal;

void vertex() {
	world_pos = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;
	world_normal = normalize((MODEL_MATRIX * vec4(NORMAL, 0.0)).xyz);
}

void fragment() {
	float h = world_pos.y - water_level;
	float up = world_normal.y;
	vec3 sand = sand_dry;
	float wet = 1.0 - smoothstep(wet_height - 0.04, wet_height, h);  // NEW: 1 below wet_height
	sand = mix(sand, sand_wet, wet);                                 // NEW
	sand = mix(sand, seabed, smoothstep(0.0, -0.8, h));              // NEW: deeper → seabed colour

	float is_grass = smoothstep(grass_height - 0.08, grass_height + 0.02, h) * smoothstep(0.6, 0.8, up);
	float is_bank = (1.0 - smoothstep(0.65, 0.85, up)) * smoothstep(wet_height + 0.1, wet_height + 0.25, h);
	vec3 colour = mix(sand, bank, is_bank);
	colour = mix(colour, grass, is_grass);
	ALBEDO = colour;
	ROUGHNESS = 1.0;
}
