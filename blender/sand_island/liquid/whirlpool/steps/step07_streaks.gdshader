// STEP 7 — streaks: long lines going round, read from noise on a circle.
// To stretch noise round the middle, read it in rings: (cos, sin of the angle) × streak_ring
// goes once round a circle in the noise as you go once round the whirlpool, so it meets itself
// (no seam where the angle jumps from π to −π); the distance from the middle / streak_width picks
// the ring. A small circle round and many rings out: long thin streaks. This is why the noise is
// 3D: the circle takes two of its directions, the rings the third. The streaks turn and sink with
// the blobs (the same two copies): whirl() now returns both, x the blobs, y the streaks.
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
uniform float streak_ring = 1.6;  // NEW: the circle the noise is read on: bigger, more and shorter streaks
uniform float streak_width = 0.3;  // NEW: metres across a streak
uniform float streak_strength : hint_range(0.0, 0.5) = 0.2;  // NEW: how much they lighten / darken

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
float streaks(vec2 q, float copy) {  // NEW
	float r = length(q);  // NEW
	vec2 around = q / max(r, 0.001);  // NEW: (cos, sin) of the angle
	return noise3(vec3(around * streak_ring + copy * 2.3, r / streak_width));  // NEW
}  // NEW

// Both patterns at q: x the blobs, y the streaks.
vec2 pattern(vec2 q, float copy) {  // NEW
	return vec2(blobs(q, copy), streaks(q, copy));  // NEW
}  // NEW

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
vec2 whirl(vec2 d, float t) {  // NEW: both patterns
	float phase0 = fract(t / cycle);  // 0 -> 1 every cycle
	float phase1 = fract(t / cycle + 0.5);  // the same, half a cycle later
	float weight0 = 1.0 - abs(1.0 - 2.0 * phase0);  // 0 at copy 0's restart, 1 halfway
	vec2 a = pattern(came_from(d, phase0 * cycle), 0.0);  // NEW
	vec2 b = pattern(came_from(d, phase1 * cycle), 1.0);  // NEW
	vec2 mixed = mix(b, a, weight0);  // NEW
	return 0.5 + (mixed - 0.5) / sqrt(weight0 * weight0 + (1.0 - weight0) * (1.0 - weight0));
}

void vertex() {
	offset = VERTEX.xz - MODEL_MATRIX[3].xz;
}

void fragment() {
	float r = length(offset);
	vec3 color = mix(deep_color, shallow_color, smoothstep(eye, radius, r));
	vec2 w = whirl(offset, TIME);  // NEW: x blobs, y streaks
	color *= 1.0 + (w.x - 0.5) * 2.0 * blob_strength;
	color *= 1.0 + (w.y - 0.5) * 2.0 * streak_strength;  // NEW
	ALBEDO = color;
	ROUGHNESS = 0.25;
}
