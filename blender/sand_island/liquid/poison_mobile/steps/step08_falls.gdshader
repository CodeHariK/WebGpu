// STEP 8 — the falls: thin light threads sliding down, faster the further they have fallen.
// UV2.x tells the flat poison from a fall: 0 on the flat, 0..1 over the lip, 1 and more down the
// fall; on_flat is 1 on the flat and 0 down a fall. On a fall UV is its own frame: x across the
// fall, y metres down from the lip. Noise squeezed across (many threads a metre) and stretched
// down makes long threads; only the brightest are kept (smoothstep), so they are crisp. They slide
// by sqrt(metres fallen), not metres, so a thread speeds up as it falls, like a real fall. The
// ripples (step 7) are kept to the flat.
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
uniform vec3 streak_color : source_color = vec3(0.85, 1.0, 0.45);  // NEW
uniform float streaks_per_metre = 9.0;  // NEW: threads a metre across the fall
uniform float fall_speed = 2.0;  // NEW: x flow_speed, one metre down the fall

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
	float on_flat = 1.0 - smoothstep(0.0, 0.85, UV2.x);  // NEW: 1 on the flat, 0 down a fall
	colour *= 1.0 + (r - 0.5) * 2.0 * ripple_strength * on_flat;  // NEW: ripples on the flat only

	float fallen = max(UV.y, 0.0) + 0.25;  // NEW: + a little: not standing still at the lip
	vec2 q = vec2(UV.x * streaks_per_metre, sqrt(fallen) * 2.0 - TIME * flow_speed * fall_speed);  // NEW
	float threads = noise(q) * 0.6 + noise(q * vec2(2.1, 1.7) + 4.3) * 0.4;  // NEW: two sizes, 0..1
	float bright = smoothstep(0.55, 0.85, threads);  // NEW: only the brightest threads
	colour = mix(colour, streak_color, bright * 0.8 * (1.0 - on_flat));  // NEW: only down the falls
	ALBEDO = colour;
	ROUGHNESS = 1.0;
	SPECULAR = 0.0;
}
