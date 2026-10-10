// STEP 12 — ripples and grain BAKED in Blender: one texture read instead of six noise reads.
// Step 11 still built the ripples in the shader: ripple() = 2 noise reads, and it ran 3 times
// (here and a little to the right and forward) to get the slope for the lighting, so 6 reads + maths,
// plus 2 hash lookups for the grain. sand_texture/sand_texture.blend makes the same sand as a
// TILEABLE material (every noise sampled on a 4D loop of the UVs, so the tile repeats with no seam),
// and its bake packs everything into sand_ripples.png (512², data):
//   R = ripple height · G, B = ripple slope in x and z (computed from the height, wrapping around
//   the edges) · A = grain (1 none, 0.5 dark speck, 0.25 light speck: opaque almost everywhere, so
//   the PNG previews normally and no tool drops the RGB under transparent pixels)
// The shader reads it once at world xz / 4 m. Tone, wet band, grass and bank stay computed: they
// depend on height and slope of THIS island, which no tile can know.
shader_type spatial;
render_mode specular_disabled;

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

group_uniforms lighting;
uniform vec3 shadow_tint : source_color = vec3(0.7, 0.68, 0.9);
uniform float shadow_light : hint_range(0.0, 1.0) = 0.35;
uniform float band_lit = 0.25;
uniform float band_softness = 0.12;

varying vec3 world_pos;
varying vec3 world_normal;

uniform sampler2D noise_tex : filter_linear, repeat_enable;  // textures/noise_128.png (random grey pixels)
const float NOISE_SIZE = 128.0;
const vec2 SLICE = vec2(37.0, 17.0);
uniform sampler2D sand_tex : filter_linear_mipmap, repeat_enable;  // NEW: sand_texture/sand_ripples.png (data)
uniform float sand_tile = 4.0;     // NEW: metres per repeat of the baked tile
uniform float slope_range = 8.0;   // NEW: G, B store slope / (2 × this) + 0.5 (the bake's S)                           // offset between independent noise "layers"

// hash: one random number per grid cell, read straight from one texel (texelFetch = no
// filtering). Replaces ~10 multiplies; mainly here so ALL randomness comes from the texture.
float hash(vec3 cell) {
	ivec2 texel = ivec2(mod(cell.xz + cell.y * vec2(31.0, 17.0), NOISE_SIZE));
	return texelFetch(noise_tex, texel, 0).r;
}

// value noise from ONE texture read instead of 8 hash() calls. Bilinear filtering already
// blends the 4 random pixels around x; moving the sample point with smoothstep inside the cell
// gives the same soft S-curve as before. (textureLod 0: no mipmaps, so it never goes blurry.)
float noise2(vec2 x) {
	vec2 i = floor(x);
	vec2 f = fract(x);
	f = f * f * (3.0 - 2.0 * f);
	return textureLod(noise_tex, (i + f + 0.5) / NOISE_SIZE, 0.0).r;
}

// 3D noise for patterns that change over TIME: z picks a layer; between two layers we blend,
// so this one costs 2 reads (still 4x cheaper than 8 hashes).
float noise(vec3 x) {
	float z = floor(x.z);
	float fz = fract(x.z);
	fz = fz * fz * (3.0 - 2.0 * fz);
	vec2 a = x.xy + z * SLICE;
	return mix(noise2(a), noise2(a + SLICE), fz);
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
	float tone = noise2(p * 0.25 + 1.0 * SLICE);  // 1 texture read
	vec3 sand = mix(sand_dry, sand_light, smoothstep(0.35, 0.75, tone));
	vec4 baked = texture(sand_tex, p / sand_tile);       // NEW: ONE read: ripple height, slope, grain
	float r = baked.r;
	sand *= 1.0 - ripple_shade + ripple_shade * 2.0 * r;
	float grain = step(baked.a, 0.75);                    // NEW: A: 1 none, 0.5 dark, 0.25 light speck
	sand = mix(sand, sand * (baked.a < 0.375 ? 1.08 : 0.88), grain);

	float wave = sin(TIME * wave_speed + p.x * 0.15) * 0.5 + 0.5;
	float wet_top = wet_height + wave * wave_reach + (noise2(p * 0.8 + 2.0 * SLICE) - 0.5) * 0.04;  // 1 texture read
	float wet = 1.0 - smoothstep(wet_top - 0.04, wet_top, h);
	sand = mix(sand, sand_wet * (0.95 + 0.1 * r), wet);
	sand = mix(sand, seabed, smoothstep(0.0, -0.8, h));

	// --- grass and bank
	float patches = smoothstep(0.45, 0.6, noise2(p * 0.5 + 7.0 * SLICE));  // 1 texture read
	vec3 lawn = mix(grass, grass_dark, patches * 0.7);
	float stripes = smoothstep(0.55, 0.75, sin(world_pos.y * 9.0 + noise2(p * 0.6 + 3.0 * SLICE) * 3.0) * 0.5 + 0.5);  // 1 texture read
	vec3 earth = mix(bank, bank_stripe, stripes * 0.6);

	float is_grass = smoothstep(grass_height - 0.08, grass_height + 0.02, h) * smoothstep(0.6, 0.8, up);
	float is_bank = (1.0 - smoothstep(0.65, 0.85, up)) * smoothstep(wet_height + 0.1, wet_height + 0.25, h);
	vec3 colour = mix(sand, earth, is_bank);
	colour = mix(colour, lawn, is_grass);

	// ripples tilt the normal on flat sand only
	float flat_sand = (1.0 - is_grass) * (1.0 - is_bank) * smoothstep(0.85, 0.97, up);
	vec2 slope = (baked.gb * 2.0 - 1.0) * slope_range;   // NEW: the baked slope, no extra evaluations
	vec3 n = normalize(world_normal - vec3(slope.x, 0.0, slope.y) * 0.02 * ripple_strength * flat_sand);
	NORMAL = normalize((VIEW_MATRIX * vec4(n, 0.0)).xyz);

	ALBEDO = colour;
	ROUGHNESS = 1.0;
}

void light() { 
	float ndl = dot(NORMAL, LIGHT) * ATTENUATION;
	float lit = smoothstep(band_lit - band_softness, band_lit + band_softness, ndl);
	DIFFUSE_LIGHT += LIGHT_COLOR / PI * mix(shadow_tint * shadow_light, vec3(1.0), lit);
}
