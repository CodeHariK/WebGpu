// STEP 5 — the wet band moves with the waves.
// Exactly the formula from water step 6: sin(TIME * wave_speed + x * 0.15) swings 0..1, and the
// top of the wet band moves up by wave * wave_reach. Because the water's foam uses the same numbers,
// the wet sand and the foam wash in and out together — they look like one effect.
shader_type spatial;

uniform vec3 sand_dry : source_color = vec3(0.98, 0.86, 0.62);
uniform vec3 grass : source_color = vec3(0.42, 0.74, 0.3);
uniform vec3 bank : source_color = vec3(0.92, 0.55, 0.3);
uniform float water_level = 0.0;
uniform float grass_height = 1.45;
uniform float wet_height = 0.07;
uniform vec3 sand_wet : source_color = vec3(0.92, 0.76, 0.54);
uniform vec3 seabed : source_color = vec3(0.62, 0.78, 0.7);
uniform float wave_reach = 0.05;  // NEW
uniform float wave_speed = 0.9;   // NEW: keep equal to the water's wave_speed

varying vec3 world_pos;
varying vec3 world_normal;

void vertex() {
	world_pos = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;
	world_normal = normalize((MODEL_MATRIX * vec4(NORMAL, 0.0)).xyz);
}

void fragment() {
	float h = world_pos.y - water_level;
	float up = world_normal.y;
	vec2 p = world_pos.xz;
	vec3 sand = sand_dry;
	float wave = sin(TIME * wave_speed + p.x * 0.15) * 0.5 + 0.5;  // NEW: same as the water
	float wet_top = wet_height + wave * wave_reach;                  // NEW
	float wet = 1.0 - smoothstep(wet_top - 0.04, wet_top, h);      // NEW: uses the moving top
	sand = mix(sand, sand_wet, wet);
	sand = mix(sand, seabed, smoothstep(0.0, -0.8, h));

	float is_grass = smoothstep(grass_height - 0.08, grass_height + 0.02, h) * smoothstep(0.6, 0.8, up);
	float is_bank = (1.0 - smoothstep(0.65, 0.85, up)) * smoothstep(wet_height + 0.1, wet_height + 0.25, h);
	vec3 colour = mix(sand, bank, is_bank);
	colour = mix(colour, grass, is_grass);
	ALBEDO = colour;
	ROUGHNESS = 1.0;
}
