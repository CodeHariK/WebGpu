// STEP 4 — faster inside: each ring turns at its own rate, and the pattern winds up.
// spin_speed(r): spin radians a second in the middle, half that one eye out, slower and slower
// further out. Now the blobs turn like liquid, but every second the inner rings get further
// ahead of the outer ones, so the blobs are smeared round into thinner and thinner spiral lines.
// Watch it for 20 seconds. (A flow map slid forever stretches the same way.)
shader_type spatial;
render_mode world_vertex_coords;

global uniform sampler3D waterfall_noise_a : filter_linear, repeat_enable;  // the baked noises
const float NOISE_PERIOD = 6.0;  // the texture repeats every 6 noise units

uniform vec3 deep_color : source_color = vec3(0.03, 0.2, 0.36);
uniform vec3 shallow_color : source_color = vec3(0.2, 0.62, 0.7);
uniform float radius = 6.0;  // metres: how far the whirl reaches
uniform float eye = 0.7;  // metres: the middle, where it spins fastest
uniform float blob_size = 1.0;  // metres a blob
uniform float blob_strength : hint_range(0.0, 0.5) = 0.15;  // how much they lighten / darken
uniform float spin = 2.5;  // NEW: radians a second in the middle (instead of one turn for all)

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
vec2 rotate(vec2 p, float a) {
	float c = cos(a);
	float s = sin(a);
	return vec2(c * p.x - s * p.y, s * p.x + c * p.y);
}

// How fast the liquid r metres from the middle goes round, radians a second.
float spin_speed(float r) {  // NEW
	return spin / (1.0 + r / eye);  // NEW
}  // NEW

void vertex() {
	offset = VERTEX.xz - MODEL_MATRIX[3].xz;
}

void fragment() {
	float r = length(offset);
	vec3 color = mix(deep_color, shallow_color, smoothstep(eye, radius, r));
	vec2 q = rotate(offset, -spin_speed(r) * TIME);  // NEW: each ring turned by its own angle
	color *= 1.0 + (blobs(q) - 0.5) * 2.0 * blob_strength;
	ALBEDO = color;
	ROUGHNESS = 0.25;
}
