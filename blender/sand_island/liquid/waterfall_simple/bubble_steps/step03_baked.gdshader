// STEP 3 — baked noise, as the sheet's step 10: the dots read voronoi F1 from a 3D texture
// (noise_a red, noise/, tools/bake_waterfall_noise.py) instead of checking 27 cells a pixel. The
// wobble stays worked out: it runs per vertex, not per pixel, so it costs little.
shader_type spatial;
render_mode unshaded, cull_disabled;

// NEW: the noises, baked by tools/bake_waterfall_noise.py into two 3D textures (noise/) that
// tile every NOISE_PERIOD noise units; set once for the project (Project Settings > Shader
// Globals), so no material needs them
global uniform sampler3D waterfall_noise_a : filter_linear, repeat_enable;  // NEW: F1, borders, value fBMs
const float NOISE_PERIOD = 6.0;  // NEW: noise units the textures cover before they repeat

uniform vec3 bubble_color = vec3(1.3);  // linear, a little past white
uniform float wobble_strength = 0.5;  // step 2: metres per noise unit
uniform float wobble_size = 0.41;  // step 2: metres a crumple (Blender's Clouds size)
uniform float wobble_fall = 2.2;  // step 2: metres a second the crumples sink

// a random point in each cell of a 3D grid (Dave Hoskins' hash, no sin).
vec3 hash33(vec3 p) {
	p = fract(p * vec3(0.1031, 0.1030, 0.0973));
	p += dot(p, p.yxz + 33.33);
	return fract((p.xxy + p.yxx) * p.zyx);
}

// Voronoi F1, baked: noise_a's red (it was the nearest of 27 cells' points, worked out per pixel).
float cells(vec3 p) {
	return texture(waterfall_noise_a, p / NOISE_PERIOD).r;  // NEW
}

varying vec3 object_p;  // the square's own position (Blender's Object coordinates)

// gradient (Perlin) noise in 3D, about -1..1 (the pool's step 2).
float gnoise3(vec3 p) {
	vec3 i = floor(p);
	vec3 f = p - i;
	vec3 u = f * f * f * (f * (f * 6.0 - 15.0) + 10.0);
	float n000 = dot(hash33(i) * 2.0 - 1.0, f);
	float n100 = dot(hash33(i + vec3(1.0, 0.0, 0.0)) * 2.0 - 1.0, f - vec3(1.0, 0.0, 0.0));
	float n010 = dot(hash33(i + vec3(0.0, 1.0, 0.0)) * 2.0 - 1.0, f - vec3(0.0, 1.0, 0.0));
	float n110 = dot(hash33(i + vec3(1.0, 1.0, 0.0)) * 2.0 - 1.0, f - vec3(1.0, 1.0, 0.0));
	float n001 = dot(hash33(i + vec3(0.0, 0.0, 1.0)) * 2.0 - 1.0, f - vec3(0.0, 0.0, 1.0));
	float n101 = dot(hash33(i + vec3(1.0, 0.0, 1.0)) * 2.0 - 1.0, f - vec3(1.0, 0.0, 1.0));
	float n011 = dot(hash33(i + vec3(0.0, 1.0, 1.0)) * 2.0 - 1.0, f - vec3(0.0, 1.0, 1.0));
	float n111 = dot(hash33(i + vec3(1.0, 1.0, 1.0)) * 2.0 - 1.0, f - vec3(1.0, 1.0, 1.0));
	return mix(mix(mix(n000, n100, u.x), mix(n010, n110, u.x), u.y), mix(mix(n001, n101, u.x), mix(n011, n111, u.x), u.y), u.z);
}

void vertex() {
	object_p = VERTEX;
	// crumple the square: pushed along its normal by a noise in the world that sinks
	vec3 world = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz + vec3(0.0, wobble_fall * TIME, 0.0);
	float n = gnoise3(world / wobble_size) + 0.5 * gnoise3(world * 2.0 / wobble_size);
	VERTEX += NORMAL * n * wobble_strength;
}

void fragment() {
	float dot_in = step(cells((object_p + vec3(0.0, 0.0, 0.92 * TIME)) * 1.93), 0.2);  // 1 on a bubble
	vec3 q = (object_p * 0.5 + 0.5) + vec3(-0.5, -0.48, -0.07);  // Generated, moved
	float r = max(1.0 - length(q), 0.0);  // the sphere gradient
	float in_patch = step(0.141, r * r);
	ALBEDO = bubble_color;
	ALPHA = dot_in * in_patch;
	ALPHA_SCISSOR_THRESHOLD = 0.5;
}
