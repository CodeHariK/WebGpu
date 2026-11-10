// STEP 9 — froth: pale foam along the shore, churning where a fall lands, and at each fall's foot.
// Three kinds, each 0..1, added and capped at 1:
//   shore    shallower than shore_width (step 2's depth), its edge wobbling with slow noise; not
//            on a stream (UV2.y is -1 there: a thin sheet, shallow everywhere, it would be all foam)
//   landing  COLOR.a is 1 where a fall lands: patches of fast-moving noise there
//   foot     UV2.y is metres above the landing on a fall: the last landing_height metres, its top
//            edge following the threads (step 8)
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
uniform vec3 foam_color : source_color = vec3(0.88, 1.0, 0.6);  // NEW: pale froth
uniform float shore_width = 0.15;  // NEW: metres of depth that still get shore foam
uniform float landing_height = 0.35;  // NEW: metres above a fall's landing that foam

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
	float on_flat = 1.0 - smoothstep(0.0, 0.85, UV2.x);
	colour *= 1.0 + (r - 0.5) * 2.0 * ripple_strength * on_flat;

	float fallen = max(UV.y, 0.0) + 0.25;
	vec2 q = vec2(UV.x * streaks_per_metre, sqrt(fallen) * 2.0 - TIME * flow_speed * fall_speed);
	float threads = noise(q) * 0.6 + noise(q * vec2(2.1, 1.7) + 4.3) * 0.4;
	float bright = smoothstep(0.55, 0.85, threads);
	colour = mix(colour, streak_color, bright * 0.8 * (1.0 - on_flat));

	float on_stream = clamp(-UV2.y, 0.0, 1.0);  // NEW: 1 on a stream
	float wobble = noise(UV * 4.0 + TIME * 0.6 * flow_speed);  // NEW
	float shore = 1.0 - smoothstep(0.0, shore_width * (0.6 + wobble * 0.8), depth);  // NEW
	shore *= (1.0 - on_stream) * on_flat;  // NEW
	float churn = noise(UV * 2.6 + vec2(1.0, -0.7) * TIME * flow_speed * 1.7);  // NEW
	float landing = smoothstep(0.5, 1.0, COLOR.a) * smoothstep(0.45, 0.75, churn) * on_flat;  // NEW
	float fall_foot = (1.0 - smoothstep(0.0, landing_height, UV2.y - threads * 0.15)) * (1.0 - on_flat);  // NEW
	float foam = clamp(shore + landing + fall_foot, 0.0, 1.0);  // NEW
	colour = mix(colour, foam_color, foam * 0.9);  // NEW
	ALBEDO = colour;
	ROUGHNESS = 1.0;
	SPECULAR = 0.0;
}
