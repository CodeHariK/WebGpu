// STEP 6 — make it run: slide the ripples along the flow (the simple way, and its problem).
// The velocity is the flow direction (step 4) times flow_speed in a stream, a slow drift in a pond
// (mix by COLOR.b). Sliding the pattern by velocity * TIME moves it downstream. Watch a bend for a
// while: each pixel slides its own way, so where the direction changes the ripples are pulled
// apart further and further, into long streaks. Step 7 fixes that.
shader_type spatial;
render_mode cull_disabled;

#include "res://shader_lib/liquid_noise.gdshaderinc"

uniform vec3 deep_color : source_color = vec3(0.2, 0.55, 0.08);  // the poison's own green
uniform vec3 shallow_color : source_color = vec3(0.75, 0.95, 0.3);  // pale, sickly
uniform float depth_range : hint_range(0.1, 4.0) = 1.2;  // metres from shore to full colour
uniform float flow_speed = 0.35;  // NEW: metres a second: thick
uniform float ripple_scale = 1.3;  // about this many ripples a metre
uniform float ripple_strength : hint_range(0.0, 0.5) = 0.12;  // how much they lighten / darken

varying float depth;

// Two sizes of blobs together look like ripples: 0..1.
float ripples(vec2 p) {
	return noise(p) * 0.65 + noise(p * 2.3 + 7.1) * 0.35;
}

void vertex() {
	depth = CUSTOM0.x;
}

void fragment() {
	float deepness = smoothstep(0.0, depth_range, depth);
	vec3 colour = mix(shallow_color, deep_color, deepness);
	vec2 direction = COLOR.rg * 2.0 - 1.0;  // NEW: the flow (step 4)
	float flows = clamp(COLOR.b, 0.0, 1.0);  // NEW
	vec2 velocity = mix(vec2(0.08, 0.05), direction * flow_speed, flows);  // NEW: a pond just drifts
	float r = ripples((UV - velocity * TIME) * ripple_scale);  // NEW: slid downstream
	colour *= 1.0 + (r - 0.5) * 2.0 * ripple_strength;
	ALBEDO = colour;
	ROUGHNESS = 1.0;
	SPECULAR = 0.0;
}
