// STEP 11 — the bubbles ride the stream: they drift with the flow, like the ripples.
// The same velocity as the ripples (step 6), the same trick (step 7): two copies of the bubbles,
// each sliding for one bubble_cycle and starting over, half a cycle apart. A bubble is born after
// its copy restarts and has popped before the next restart, so the jump never shows and nothing
// needs cross-fading: max() puts the two copies together.
shader_type spatial;
render_mode cull_disabled;

#include "res://shader_lib/liquid_noise.gdshaderinc"

uniform vec3 deep_color : source_color = vec3(0.2, 0.55, 0.08);  // the poison's own green
uniform vec3 shallow_color : source_color = vec3(0.75, 0.95, 0.3);  // pale, sickly
uniform float depth_range : hint_range(0.1, 4.0) = 1.2;  // metres from shore to full colour
uniform float flow_speed = 0.35;  // metres a second: thick
uniform float ripple_scale = 1.3;  // about this many ripples a metre
uniform float ripple_strength : hint_range(0.0, 0.5) = 0.12;  // how much they lighten / darken
uniform float flow_cycle = 1.6;  // seconds one copy slides before it restarts
uniform vec3 streak_color : source_color = vec3(0.85, 1.0, 0.45);  // the fall's threads
uniform float streaks_per_metre = 9.0;  // threads a metre across the fall
uniform float fall_speed = 2.0;  // x flow_speed, one metre down the fall
uniform vec3 foam_color : source_color = vec3(0.88, 1.0, 0.6);  // pale froth
uniform float shore_width = 0.15;  // metres of depth that still get shore foam
uniform float landing_height = 0.35;  // metres above a fall's landing that foam
uniform float bubbles_per_metre = 2.5;  // squares a metre; each may hold one bubble
uniform float bubble_amount : hint_range(0.0, 1.0) = 0.35;  // the share of squares that do
uniform float bubble_cycle = 3.0;  // seconds of one bubble cycle

varying float depth;

// Two sizes of blobs together look like ripples: 0..1.
float ripples(vec2 p) {
	return noise(p) * 0.65 + noise(p * 2.3 + 7.1) * 0.35;
}

// Two copies, each sliding for one cycle and restarting, half a cycle apart, cross-faded.
float flowing_ripples(vec2 p, vec2 velocity, float t) {
	float phase0 = fract(t / flow_cycle);
	float phase1 = fract(t / flow_cycle + 0.5);
	float weight0 = 1.0 - abs(1.0 - 2.0 * phase0);
	float r0 = ripples(p - velocity * phase0 * flow_cycle);
	float r1 = ripples(p - velocity * phase1 * flow_cycle + vec2(3.7, 1.3));
	return mix(r1, r0, weight0);
}

// One bubble per square of the plane (some squares have none). p_phase (0..1) is how far through
// its cycle the bubble's square is; each bubble is born at a random moment in it, grows (fast at
// first, like a sphere coming up) for most of its life, then pops: a thin ring spreading out and
// fading. All of it is over before the cycle restarts (phase 1 -> 0), so the restart never shows.
// Returns x: the bubble (0 outside, brighter at its rim, like a sphere's edge), y: the pop ring.
vec2 bubble(vec2 p, float p_phase) {
	vec2 square = floor(p); // which square of the plane
	vec2 f = fract(p); // where in the square, 0..1
	float has = step(hash(square + 5.0), bubble_amount); // this square: a bubble or not
	float birth = 0.05 + hash(square + 9.0) * 0.6; // born between 0.05 and 0.65 of the cycle
	float age = (p_phase - birth) / 0.3; // 0 -> 1 over its life (0.3 of the cycle)
	float alive = step(0.0, age) * step(age, 1.0);
	vec2 centre = 0.35 + hash2(square + 2.0) * 0.3; // near the square's middle, so it stays inside
	float d = length(f - centre);
	// growing (age 0 .. 0.85): radius up to 0.3 of the square, sqrt: quick at first
	float radius = 0.3 * sqrt(clamp(age / 0.85, 0.0, 1.0));
	float disc = 1.0 - smoothstep(radius - 0.03, radius, d);
	float rim = smoothstep(radius * 0.5, radius, d);
	float body = disc * (0.4 + 0.6 * rim) * step(age, 0.85);
	// popped (age 0.85 .. 1): a ring from the bubble's edge out a little, fading
	float popped = clamp((age - 0.85) / 0.15, 0.0, 1.0);
	float ring = (1.0 - smoothstep(0.0, 0.025, abs(d - (0.3 + popped * 0.05)))) * (1.0 - popped) * step(0.85, age);
	return vec2(body, ring) * has * alive;
}

// The bubbles carried along the flow: the flow-map trick again (step 7), two copies half a cycle
// apart. Each bubble only lives inside its copy's cycle (born after the restart, popped before the
// next), so nothing needs cross-fading: the two copies are just put together with max.
vec2 flowing_bubbles(vec2 p, vec2 velocity, float t) {  // NEW
	float phase0 = fract(t / bubble_cycle);
	float phase1 = fract(t / bubble_cycle + 0.5);
	vec2 b0 = bubble(p - velocity * phase0 * bubble_cycle, phase0);
	vec2 b1 = bubble(p - velocity * phase1 * bubble_cycle + vec2(4.1, 7.7), phase1);
	return max(b0, b1);
}

void vertex() {
	depth = CUSTOM0.x;
}

void fragment() {
	float deepness = smoothstep(0.0, depth_range, depth);
	vec3 colour = mix(shallow_color, deep_color, deepness);
	vec2 direction = COLOR.rg * 2.0 - 1.0;
	float flows = clamp(COLOR.b, 0.0, 1.0);
	vec2 velocity = mix(vec2(0.08, 0.05), direction * flow_speed, flows);
	float r = flowing_ripples(UV * ripple_scale, velocity * ripple_scale, TIME);
	float on_flat = 1.0 - smoothstep(0.0, 0.85, UV2.x);
	colour *= 1.0 + (r - 0.5) * 2.0 * ripple_strength * on_flat;

	float fallen = max(UV.y, 0.0) + 0.25;
	vec2 q = vec2(UV.x * streaks_per_metre, sqrt(fallen) * 2.0 - TIME * flow_speed * fall_speed);
	float threads = noise(q) * 0.6 + noise(q * vec2(2.1, 1.7) + 4.3) * 0.4;
	float bright = smoothstep(0.55, 0.85, threads);
	colour = mix(colour, streak_color, bright * 0.8 * (1.0 - on_flat));

	float on_stream = clamp(-UV2.y, 0.0, 1.0);  // 1 on a stream
	float wobble = noise(UV * 4.0 + TIME * 0.6 * flow_speed);
	float shore = 1.0 - smoothstep(0.0, shore_width * (0.6 + wobble * 0.8), depth);
	shore *= (1.0 - on_stream) * on_flat;
	float churn = noise(UV * 2.6 + vec2(1.0, -0.7) * TIME * flow_speed * 1.7);
	float landing = smoothstep(0.5, 1.0, COLOR.a) * smoothstep(0.45, 0.75, churn) * on_flat;
	float fall_foot = (1.0 - smoothstep(0.0, landing_height, UV2.y - threads * 0.15)) * (1.0 - on_flat);
	float foam = clamp(shore + landing + fall_foot, 0.0, 1.0);
	colour = mix(colour, foam_color, foam * 0.9);

	vec2 b = flowing_bubbles(UV * bubbles_per_metre, velocity * bubbles_per_metre, TIME);  // NEW: drifting with the flow
	b *= on_flat * (1.0 - foam);  // on the flat poison, not on the foam
	colour = mix(colour, foam_color, b.x * 0.6);  // the bubble, in the froth's colour
	colour = mix(colour, foam_color, b.y);  // the pop
	ALBEDO = colour;
	ROUGHNESS = 1.0;
	SPECULAR = 0.0;
}
