// STEP 3 — turn it: the whole pattern spins round the middle, like a plate.
// To spin a pattern, read it where the liquid was at the start: turn the point back by the angle
// it has turned so far (turn × TIME). rotate() turns a 2D point round the middle by an angle
// (radians). Everything turns at the same rate, so nothing tears or stretches, but it looks like
// a spinning plate, not liquid: liquid near the middle goes round faster (next step).
shader_type spatial;
render_mode world_vertex_coords;

global uniform sampler3D waterfall_noise_a : filter_linear, repeat_enable;  // the baked noises
const float NOISE_PERIOD = 6.0;  // the texture repeats every 6 noise units

uniform vec3 deep_color : source_color = vec3(0.03, 0.2, 0.36);
uniform vec3 shallow_color : source_color = vec3(0.2, 0.62, 0.7);
uniform float radius = 6.0;  // metres: how far the whirl reaches
uniform float eye = 0.7;  // metres: the middle
uniform float blob_size = 1.0;  // metres a blob
uniform float blob_strength : hint_range(0.0, 0.5) = 0.15;  // how much they lighten / darken
uniform float turn = 0.3;  // NEW: radians a second, everywhere

varying vec2 offset;  // metres from the middle (x, z)

// Value noise of 3 sizes at p (noise units), 0..1: one read of the baked texture.
float noise3(vec3 p) {
	return texture(waterfall_noise_a, p / NOISE_PERIOD).b;
}

// Soft blobs at q (metres from the middle), 0..1.
float blobs(vec2 q) {
	return noise3(vec3(q / blob_size, 0.5));
}

// p turned round the middle by a radians.
vec2 rotate(vec2 p, float a) {  // NEW
	float c = cos(a);  // NEW
	float s = sin(a);  // NEW
	return vec2(c * p.x - s * p.y, s * p.x + c * p.y);  // NEW
}  // NEW

void vertex() {
	offset = VERTEX.xz - MODEL_MATRIX[3].xz;
}

void fragment() {
	float r = length(offset);
	vec3 color = mix(deep_color, shallow_color, smoothstep(eye, radius, r));
	vec2 q = rotate(offset, -turn * TIME);  // NEW: where this bit of liquid was at the start
	color *= 1.0 + (blobs(q) - 0.5) * 2.0 * blob_strength;  // NEW: the blobs read there
	ALBEDO = color;
	ROUGHNESS = 0.25;
}
