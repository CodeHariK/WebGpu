// STEP 10 — our own lighting: soft toon bands. This is the finished sand.gdshader.
// Writing a light() function replaces Godot's lighting for this material. It runs once per light per
// pixel: dot(NORMAL, LIGHT) is 1 facing the sun, 0 side-on, negative facing away (× ATTENUATION =
// shadows). smoothstep turns that into a soft lit/shadow step, and the shadow side gets a cool
// lilac tint (shadow_tint) instead of plain dark — the cartoon look. Specular is switched off.
shader_type spatial;
render_mode specular_disabled;  // NEW: no shiny highlight

group_uniforms heights;
uniform float water_level = 0.0;
uniform float grass_height = 1.45;     // above water
uniform float wet_height = 0.07;       // wet band reaches this high (plus the wave)
uniform float wave_reach = 0.05;       // how far the wet band moves with the waves
uniform float wave_speed = 0.9;

group_uniforms colours;
uniform vec3 sand_dry : source_color = vec3(0.98, 0.86, 0.62);
uniform vec3 sand_light : source_color = vec3(1.0, 0.92, 0.72);
uniform vec3 sand_wet : source_color = vec3(0.92, 0.76, 0.54);
uniform vec3 seabed : source_color = vec3(0.62, 0.78, 0.7);
uniform vec3 grass : source_color = vec3(0.42, 0.74, 0.3);
uniform vec3 grass_dark : source_color = vec3(0.32, 0.62, 0.24);
uniform vec3 bank : source_color = vec3(0.92, 0.55, 0.3);
uniform vec3 bank_stripe : source_color = vec3(0.98, 0.7, 0.42);

group_uniforms detail;
uniform float ripple_spacing = 0.55;   // metres between ripple lines
uniform float ripple_strength = 0.35;  // how much ripples tilt the light
uniform float ripple_shade = 0.05;     // how much ripples darken/lighten the colour
uniform float grain_density = 0.04;    // fraction of grain cells that get a speck

group_uniforms lighting;
uniform vec3 shadow_tint : source_color = vec3(0.7, 0.68, 0.9);
uniform float shadow_light : hint_range(0.0, 1.0) = 0.35;
uniform float band_lit = 0.25;
uniform float band_softness = 0.12;

varying vec3 world_pos;
varying vec3 world_normal;

float hash(vec3 p) {
	p = fract(p * 0.3183099 + 0.1);
	p *= 17.0;
	return fract(p.x * p.y * p.z * (p.x + p.y + p.z));
}

float noise(vec3 x) {
	vec3 i = floor(x);
	vec3 f = fract(x);
	f = f * f * (3.0 - 2.0 * f);
	return mix(mix(mix(hash(i), hash(i + vec3(1, 0, 0)), f.x), mix(hash(i + vec3(0, 1, 0)), hash(i + vec3(1, 1, 0)), f.x), f.y),
			   mix(mix(hash(i + vec3(0, 0, 1)), hash(i + vec3(1, 0, 1)), f.x), mix(hash(i + vec3(0, 1, 1)), hash(i + vec3(1, 1, 1)), f.x), f.y), f.z);
}

// Wind ripples: wavy parallel ridges with a sharp crest and a soft back (like the photo).
float ripple(vec2 p) {
	float warp = noise(vec3(p * 0.35, 0.0)) * 3.0 + noise(vec3(p * 1.1, 4.0)) * 0.6;
	float phase = fract((p.x * 0.35 + p.y * 0.94) / ripple_spacing + warp);
	return pow(phase, 3.0);  // slow rise, sudden drop = asymmetric ridge
}

void vertex() {
	world_pos = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;
	world_normal = normalize((MODEL_MATRIX * vec4(NORMAL, 0.0)).xyz);
}

void fragment() {
	float h = world_pos.y - water_level;
	float up = world_normal.y;
	vec2 p = world_pos.xz;

	// --- sand
	float tone = noise(vec3(p * 0.25, 1.0));
	vec3 sand = mix(sand_dry, sand_light, smoothstep(0.35, 0.75, tone));
	float r = ripple(p);
	sand *= 1.0 - ripple_shade + ripple_shade * 2.0 * r;
	float grain = step(1.0 - grain_density, hash(floor(world_pos * 22.0)));
	sand = mix(sand, sand * (hash(floor(world_pos * 22.0) + 3.0) > 0.5 ? 1.08 : 0.88), grain);

	float wave = sin(TIME * wave_speed + p.x * 0.15) * 0.5 + 0.5;
	float wet_top = wet_height + wave * wave_reach + (noise(vec3(p * 0.8, 2.0)) - 0.5) * 0.04;
	float wet = 1.0 - smoothstep(wet_top - 0.04, wet_top, h);
	sand = mix(sand, sand_wet * (0.95 + 0.1 * r), wet);
	sand = mix(sand, seabed, smoothstep(0.0, -0.8, h));

	// --- grass and bank
	float patches = smoothstep(0.45, 0.6, noise(vec3(p * 0.5, 7.0)));
	vec3 lawn = mix(grass, grass_dark, patches * 0.7);
	float stripes = smoothstep(0.55, 0.75, sin(world_pos.y * 9.0 + noise(vec3(p * 0.6, 3.0)) * 3.0) * 0.5 + 0.5);
	vec3 earth = mix(bank, bank_stripe, stripes * 0.6);

	float is_grass = smoothstep(grass_height - 0.08, grass_height + 0.02, h) * smoothstep(0.6, 0.8, up);
	float is_bank = (1.0 - smoothstep(0.65, 0.85, up)) * smoothstep(wet_height + 0.1, wet_height + 0.25, h);
	vec3 colour = mix(sand, earth, is_bank);
	colour = mix(colour, lawn, is_grass);

	// ripples tilt the normal on flat sand only
	float flat_sand = (1.0 - is_grass) * (1.0 - is_bank) * smoothstep(0.85, 0.97, up);
	float e = 0.03;
	float dx = ripple(p + vec2(e, 0.0)) - r;
	float dz = ripple(p + vec2(0.0, e)) - r;
	vec3 n = normalize(world_normal - vec3(dx, 0.0, dz) / e * 0.02 * ripple_strength * flat_sand);
	NORMAL = normalize((VIEW_MATRIX * vec4(n, 0.0)).xyz);

	ALBEDO = colour;
	ROUGHNESS = 1.0;
}

void light() {  // NEW: everything in here is new
	float ndl = dot(NORMAL, LIGHT) * ATTENUATION;
	float lit = smoothstep(band_lit - band_softness, band_lit + band_softness, ndl);
	DIFFUSE_LIGHT += LIGHT_COLOR / PI * mix(shadow_tint * shadow_light, vec3(1.0), lit);
}
