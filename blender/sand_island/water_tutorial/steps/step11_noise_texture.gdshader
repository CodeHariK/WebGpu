// STEP 11 — the same water, cheaper: noise from a texture.
// noise() did 8 hash() calls. A 128×128 texture of random pixels holds the same randomness and the
// GPU's filter blends it for free: static noise → 1 read, noise that changes with TIME → 2 reads
// (blend two layers). Sparkles read one texel per cell instead of hashing.
// Set noise_tex to textures/noise_128.png — one grey channel, see shader_lib/ (import: Lossless, no mipmaps, filter + repeat on).
shader_type spatial;
render_mode unshaded, blend_mix, cull_back, depth_draw_opaque;

uniform vec3 shore_colour : source_color = vec3(0.6, 0.93, 0.9);
uniform vec3 shallow_colour : source_color = vec3(0.25, 0.75, 0.88);
uniform vec3 deep_colour : source_color = vec3(0.12, 0.42, 0.85);
uniform float shallow_end = 0.18;
uniform float deep_start = 0.45;
uniform vec3 foam_colour : source_color = vec3(1.0);
uniform float foam_width = 0.012;
uniform float wash = 0.015;
uniform float wave_speed = 0.9;
uniform float line_speed = 0.12;
uniform float line_count = 3.0;
uniform float sparkle_density = 0.004;
uniform float ripple_height = 0.03;

varying float depth;
varying vec3 world_pos;

uniform sampler2D noise_tex : filter_linear, repeat_enable;  // NEW: textures/noise_128.png (random grey pixels)
const float NOISE_SIZE = 128.0;                                // NEW
const vec2 SLICE = vec2(37.0, 17.0);                           // NEW: offset between independent noise "layers"

// NEW — hash: one random number per grid cell, read straight from one texel (texelFetch = no
// filtering). Replaces ~10 multiplies; mainly here so ALL randomness comes from the texture.
float hash(vec3 cell) {
	ivec2 texel = ivec2(mod(cell.xz + cell.y * vec2(31.0, 17.0), NOISE_SIZE));
	return texelFetch(noise_tex, texel, 0).r;
}

// NEW — value noise from ONE texture read instead of 8 hash() calls. Bilinear filtering already
// blends the 4 random pixels around x; moving the sample point with smoothstep inside the cell
// gives the same soft S-curve as before. (textureLod 0: no mipmaps, so it never goes blurry.)
float noise2(vec2 x) {
	vec2 i = floor(x);
	vec2 f = fract(x);
	f = f * f * (3.0 - 2.0 * f);
	return textureLod(noise_tex, (i + f + 0.5) / NOISE_SIZE, 0.0).r;
}

// NEW — 3D noise for patterns that change over TIME: z picks a layer; between two layers we blend,
// so this one costs 2 reads (still 4x cheaper than 8 hashes).
float noise(vec3 x) {
	float z = floor(x.z);
	float fz = fract(x.z);
	fz = fz * fz * (3.0 - 2.0 * fz);
	vec2 a = x.xy + z * SLICE;
	return mix(noise2(a), noise2(a + SLICE), fz);
}

void vertex() {
	depth = COLOR.r;
	world_pos = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;
	float swell = sin(TIME * 1.3 + world_pos.x * 0.6) * cos(TIME * 1.1 + world_pos.z * 0.5);
	VERTEX.y += swell * ripple_height * smoothstep(0.05, 0.3, depth);                        
}

void fragment() {
	vec2 p = world_pos.xz;
	float wobble = (noise(vec3(p * 0.6, TIME * 0.2)) - 0.5) * 0.08;  // NEW: 2 texture reads
	float d = depth + wobble;
	vec3 colour = mix(shore_colour, shallow_colour, smoothstep(0.02, shallow_end, d));
	colour = mix(colour, deep_colour, smoothstep(deep_start - 0.08, deep_start + 0.08, d));
	float wave = sin(TIME * wave_speed + p.x * 0.15) * 0.5 + 0.5;
	float edge = foam_width + wave * wash;
	float foam = 1.0 - smoothstep(edge - 0.004, edge, depth + wobble * 0.15);
	float lines = fract(d * line_count / deep_start - TIME * line_speed);
	float line = smoothstep(0.0, 0.03, lines) * (1.0 - smoothstep(0.05, 0.09, lines));
	line *= step(0.45, noise(vec3(p * 0.9, TIME * 0.3))) * (1.0 - smoothstep(0.1, deep_start, d));  // NEW: 2 texture reads
	foam = max(foam, line * 0.8);

	vec3 cell = floor(vec3((p + TIME * vec2(0.3, 0.2)) * 9.0, floor(TIME * 2.0)));
	float sparkle = step(1.0 - sparkle_density, hash(cell)) * smoothstep(0.15, 0.4, d);

	ALBEDO = mix(colour, foam_colour, max(foam, sparkle));
	ALPHA = max(mix(0.55, 0.95, smoothstep(0.0, 0.35, depth)), foam);
}
