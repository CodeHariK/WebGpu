// STEP 2 — blobs: a still pattern laid on in metres.
// Noise needs no UV either: read it at the world position. The noise is the waterfall's, baked
// into a 3D texture (waterfall_noise_a, a global uniform: one texture read instead of working the
// noise out); a 2D pattern is a slice through it (z fixed). Its blue is value noise of 3 sizes
// (fBM), 0..1, about 0.5 on average: above 0.5 lightens, below darkens.
shader_type spatial;
render_mode world_vertex_coords;

global uniform sampler3D waterfall_noise_a : filter_linear, repeat_enable;  // NEW: the baked noises
const float NOISE_PERIOD = 6.0;  // NEW: the texture repeats every 6 noise units

uniform vec3 deep_color : source_color = vec3(0.03, 0.2, 0.36);
uniform vec3 shallow_color : source_color = vec3(0.2, 0.62, 0.7);
uniform float radius = 6.0;  // metres: how far the whirl reaches
uniform float eye = 0.7;  // metres: the middle
uniform float blob_size = 1.0;  // NEW: metres a blob
uniform float blob_strength : hint_range(0.0, 0.5) = 0.15;  // NEW: how much they lighten / darken

varying vec2 offset;  // metres from the middle (x, z)

// Value noise of 3 sizes at p (noise units), 0..1: one read of the baked texture.
float noise3(vec3 p) {  // NEW
	return texture(waterfall_noise_a, p / NOISE_PERIOD).b;  // NEW
}  // NEW

// Soft blobs at q (metres from the middle), 0..1.
float blobs(vec2 q) {  // NEW
	return noise3(vec3(q / blob_size, 0.5));  // NEW: a slice through the 3D noise
}  // NEW

void vertex() {
	offset = VERTEX.xz - MODEL_MATRIX[3].xz;
}

void fragment() {
	float r = length(offset);
	vec3 color = mix(deep_color, shallow_color, smoothstep(eye, radius, r));
	color *= 1.0 + (blobs(offset) - 0.5) * 2.0 * blob_strength;  // NEW
	ALBEDO = color;
	ROUGHNESS = 0.25;
}
