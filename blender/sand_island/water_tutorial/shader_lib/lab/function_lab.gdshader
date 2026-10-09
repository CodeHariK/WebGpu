// Function lab: shows one shader_lib function, one stage at a time, as a grey image (black = 0,
// white = 1) with a graph underneath of the values along the red line. function_lab.gd picks
// `fn` and `stage` and explains each stage on screen.
shader_type canvas_item;

#include "res://shader_lib/value_noise.gdshaderinc"
#include "res://shader_lib/texture_noise.gdshaderinc"
#include "res://shader_lib/ripple.gdshaderinc"

uniform int fn = 0;          // 0 hash · 1 value noise · 2 texture noise · 3 ripple
uniform int stage = 4;       // 1..4
uniform float span = 16.0;   // how many units fit across the image
uniform vec2 offset = vec2(0.0);
uniform float aspect = 1.6;  // width / height of the image area
uniform float spacing = 0.55;

const float GRAPH = 0.26;    // bottom part of the screen used for the graph

float hash_stage(vec2 p) {
	vec2 c = floor(p);
	vec3 c3 = vec3(c.x, 0.0, c.y);
	if (stage == 1) return fract((c.x + c.y * 16.0) / 64.0);             // the cell number: orderly
	vec3 q = fract(c3 * 0.3183099 + 0.1);
	if (stage == 2) return q.x;                                            // squash: messy decimals
	q *= 17.0;
	if (stage == 3) return log(1.0 + q.x * q.y * q.z * (q.x + q.y + q.z)) / log(250000.0);  // the big product (log scale)
	return hash(c3);                                                       // keep the decimals: random
}

float value_noise_stage(vec2 p) {
	vec2 i = floor(p);
	vec2 f = fract(p);
	float a = hash(vec3(i.x, i.y, 0.0));
	float b = hash(vec3(i.x + 1.0, i.y, 0.0));
	float c = hash(vec3(i.x, i.y + 1.0, 0.0));
	float d = hash(vec3(i.x + 1.0, i.y + 1.0, 0.0));
	if (stage == 1) return a;                                                     // corner values only
	if (stage == 2) return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);              // straight-line blend
	if (stage == 3) return noise(vec3(p, 0.0));                                   // smoothstep blend
	return noise(vec3(p, 0.0)) * 0.7 + noise(vec3(p * 3.0, 5.0)) * 0.3;           // two sizes added
}

float texture_noise_stage(vec2 p) {
	if (stage == 1) return tex_hash(vec3(floor(p.x), 0.0, floor(p.y)));          // the raw pixels
	if (stage == 2) return textureLod(noise_tex, (p + 0.5) / NOISE_SIZE, 0.0).r; // GPU blend only
	if (stage == 3) return tex_noise2(p);                                         // + smoothstep trick
	return noise(vec3(p, 0.0));                                                   // maths version, to compare
}

float ripple_stage(vec2 p) {
	float d = p.x * 0.35 + p.y * 0.94;
	if (stage == 1) return fract(d / 8.0);                                        // distance along the wind
	if (stage == 2) return fract(d / spacing);                                    // stripes (sawtooth)
	float warp = tex_noise2(p * 0.35) * 3.0 + tex_noise2(p * 1.1 + 4.0 * NOISE_LAYER) * 0.6;
	if (stage == 3) return fract(d / spacing + warp);                             // bent by noise
	return ripple(p, spacing);                                                    // pow 3: ripple shape
}

float value_at(vec2 p) {
	if (fn == 0) return hash_stage(p);
	if (fn == 1) return value_noise_stage(p);
	if (fn == 2) return texture_noise_stage(p);
	return ripple_stage(p);
}

void fragment() {
	float image_h = 1.0 - GRAPH;
	float row = 0.5;  // the graphed row, in image coordinates
	if (UV.y < image_h) {
		vec2 uv = vec2(UV.x, UV.y / image_h);
		vec2 p = offset + vec2(uv.x * span, (1.0 - uv.y) * span / aspect);
		COLOR = vec4(vec3(value_at(p)), 1.0);
		if (abs(uv.y - row) < 0.002) COLOR = vec4(0.95, 0.2, 0.2, 1.0);
	} else {
		float gy = 1.0 - (UV.y - image_h) / GRAPH;          // 0 at the bottom of the graph, 1 at the top
		vec2 p = offset + vec2(UV.x * span, (1.0 - row) * span / aspect);
		float v = value_at(p);
		float px = 1.5 / (SCREEN_PIXEL_SIZE.y > 0.0 ? 1.0 / SCREEN_PIXEL_SIZE.y : 900.0) / GRAPH;
		float line = 1.0 - smoothstep(px, px * 2.0, abs(gy * 1.1 - 0.05 - v));
		vec3 bg = mix(vec3(0.09, 0.1, 0.13), vec3(0.14, 0.15, 0.19), step(0.5, fract(p.x)));  // stripes = 1 unit
		COLOR = vec4(mix(bg, vec3(0.95, 0.35, 0.3), line), 1.0);
	}
}
