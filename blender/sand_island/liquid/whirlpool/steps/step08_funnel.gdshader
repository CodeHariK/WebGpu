// STEP 8 — the funnel: the vertex shader sinks the middle, and the eye goes dark.
// The sheet is pushed down by depth × e^(−r² / funnel²): a smooth bell, deepest in the middle and
// nearly flat two funnels out. A sunk surface faces another way, so its normal comes from the
// bell's slope (how fast the height changes along x and z: the derivative of the bell is the
// sink × 2 × offset / funnel²); the light then shines off the funnel's sides. offset is taken
// before the sinking, so the pattern doesn't slide. The sheet needs the vertices for it (0.12 m
// squares here); on a single quad only the colour would work.
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
uniform float spin = 2.5;  // radians a second in the middle
uniform float cycle = 2.0;  // seconds one copy turns before it restarts
uniform float pull = 0.12;  // how fast it sinks in: about 12 % closer a second
uniform float streak_ring = 1.6;  // the circle the noise is read on: bigger, more and shorter streaks
uniform float streak_width = 0.3;  // metres across a streak
uniform float streak_strength : hint_range(0.0, 0.5) = 0.2;  // how much they lighten / darken
uniform vec3 eye_color : source_color = vec3(0.0, 0.05, 0.12);  // NEW: the dark hole
uniform float depth = 1.5;  // NEW: metres the funnel sinks in the middle
uniform float funnel = 2.0;  // NEW: metres: how wide the funnel is

varying vec2 offset;  // metres from the middle (x, z)

// Value noise of 3 sizes at p (noise units), 0..1: one read of the baked texture.
float noise3(vec3 p) {
	return texture(waterfall_noise_a, p / NOISE_PERIOD).b;
}

// Soft blobs at q (metres from the middle), 0..1; each copy its own slice.
float blobs(vec2 q, float copy) {
	return noise3(vec3(q / blob_size, 0.5 + copy * 2.3));
}

// Streaks round the middle at q (metres from it), 0..1; each copy its own part of the noise.
float streaks(vec2 q, float copy) {
	float r = length(q);
	vec2 around = q / max(r, 0.001);  // (cos, sin) of the angle
	return noise3(vec3(around * streak_ring + copy * 2.3, r / streak_width));
}

// Both patterns at q: x the blobs, y the streaks.
vec2 pattern(vec2 q, float copy) {
	return vec2(blobs(q, copy), streaks(q, copy));
}

// p turned round the middle by a radians.
vec2 rotate(vec2 p, float a) {
	float c = cos(a);
	float s = sin(a);
	return vec2(c * p.x - s * p.y, s * p.x + c * p.y);
}

// How fast the liquid r metres from the middle goes round, radians a second.
float spin_speed(float r) {
	return spin / (1.0 + r / eye);
}

// Where the liquid now at d was s seconds ago: turned back, and further out.
vec2 came_from(vec2 d, float s) {
	return rotate(d, -spin_speed(length(d)) * s) * exp(pull * s);
}

// The patterns at d at time t: two copies, half a cycle apart, faded, the contrast kept.
vec2 whirl(vec2 d, float t) {
	float phase0 = fract(t / cycle);  // 0 -> 1 every cycle
	float phase1 = fract(t / cycle + 0.5);  // the same, half a cycle later
	float weight0 = 1.0 - abs(1.0 - 2.0 * phase0);  // 0 at copy 0's restart, 1 halfway
	vec2 a = pattern(came_from(d, phase0 * cycle), 0.0);
	vec2 b = pattern(came_from(d, phase1 * cycle), 1.0);
	vec2 mixed = mix(b, a, weight0);
	return 0.5 + (mixed - 0.5) / sqrt(weight0 * weight0 + (1.0 - weight0) * (1.0 - weight0));
}

void vertex() {
	offset = VERTEX.xz - MODEL_MATRIX[3].xz;
	float sink = depth * exp(-dot(offset, offset) / (funnel * funnel));  // NEW: metres down
	VERTEX.y -= sink;  // NEW
	vec2 slope = offset * (2.0 / (funnel * funnel)) * sink;  // NEW: how fast it rises along x, z
	NORMAL = normalize(vec3(-slope.x, 1.0, -slope.y));  // NEW: tipped away from the rise
}

void fragment() {
	float r = length(offset);
	vec3 color = mix(deep_color, shallow_color, smoothstep(eye, radius, r));
	vec2 w = whirl(offset, TIME);
	color *= 1.0 + (w.x - 0.5) * 2.0 * blob_strength;
	color *= 1.0 + (w.y - 0.5) * 2.0 * streak_strength;
	color = mix(color, eye_color, exp(-r * r / (eye * eye)));  // NEW: the dark eye
	ALBEDO = color;
	ROUGHNESS = 0.25;
}
