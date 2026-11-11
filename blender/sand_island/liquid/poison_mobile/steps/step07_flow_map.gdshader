// STEP 7 — the flow-map trick: ripples that run along the stream and never stretch.
// Instead of sliding forever, two copies of the pattern each slide for one cycle (flow_cycle
// seconds) and then jump back to the start, half a cycle apart. Each copy is faded out just as it
// jumps (its weight is 0 there, 1 halfway), and the other copy is fully shown then, so the jump
// never shows. A copy never slides more than one cycle's worth, so it can't stretch far at a bend.
// fract(x) is the part after the point: 0 -> 1 over and over. The second copy is offset so its
// blobs aren't the same ones.
shader_type spatial;
render_mode cull_disabled;

#include "res://shader_lib/liquid_noise.gdshaderinc"

uniform vec3 deep_color : source_color = vec3(0.2, 0.55, 0.08);  // the poison's own green
uniform vec3 shallow_color : source_color = vec3(0.75, 0.95, 0.3);  // pale, sickly
uniform float depth_range : hint_range(0.1, 4.0) = 1.2;  // metres from shore to full colour
uniform float flow_speed = 0.35;  // metres a second: thick
uniform float ripple_scale = 1.3;  // about this many ripples a metre
uniform float ripple_strength : hint_range(0.0, 0.5) = 0.12;  // how much they lighten / darken
uniform float flow_cycle = 1.6;  // NEW: seconds one copy slides before it restarts
uniform int show_channel : hint_enum("colour", "ripples only: grey") = 0;  // C in the viewer: the ripples alone, easier to watch

varying float depth;

// Two sizes of blobs together look like ripples: 0..1.
float ripples(vec2 p) {
	return noise(p) * 0.65 + noise(p * 2.3 + 7.1) * 0.35;
}

// Two copies, each sliding for one cycle and restarting, half a cycle apart, cross-faded.
float flowing_ripples(vec2 p, vec2 velocity, float t) {  // NEW
	float phase0 = fract(t / flow_cycle); // 0 -> 1 every cycle
	float phase1 = fract(t / flow_cycle + 0.5); // the same, half a cycle later
	float weight0 = 1.0 - abs(1.0 - 2.0 * phase0); // 0 at copy 0's restart, 1 halfway
	float r0 = ripples(p - velocity * phase0 * flow_cycle);
	float r1 = ripples(p - velocity * phase1 * flow_cycle + vec2(3.7, 1.3)); // offset: not the same blobs
	return mix(r1, r0, weight0);
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
	float r = flowing_ripples(UV * ripple_scale, velocity * ripple_scale, TIME);  // NEW: the trick
	colour *= 1.0 + (r - 0.5) * 2.0 * ripple_strength;
	ALBEDO = show_channel == 1 ? vec3(r) : colour;  // the ripples alone (C), full strength
	ROUGHNESS = 1.0;
	SPECULAR = 0.0;
}
