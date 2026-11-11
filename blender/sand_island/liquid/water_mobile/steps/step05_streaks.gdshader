// STEP 5 — streaks: white threads falling down the falls, in toon bands.
// The two side views of step 4 (from x and from z) carry the falls. On each, noise squeezed across
// (streaks_per_metre) and stretched up (0.3) makes long vertical threads; adding TIME to the height
// before the noise slides them down. band() is a toon cut: 0 below an edge, 1 above it, with only a
// sliver of blend. Two cuts give three flat colours: the fall's blue, a light band, white on the
// brightest threads. if (fall > 0.0): the flat water skips the 4 noises (most of the screen).
shader_type spatial;
render_mode cull_disabled, specular_disabled;  // lit again

#include "res://shader_lib/liquid_noise.gdshaderinc"

uniform vec3 top_color : source_color = vec3(0.13, 0.5, 0.95);  // the flat water
uniform vec3 fall_color : source_color = vec3(0.2, 0.62, 1.0);  // a fall, lighter
uniform float fall_start : hint_range(0.0, 1.0) = 0.6;  // normal.y below this: a fall
uniform float shade : hint_range(0.0, 1.0) = 0.6;  // light on the side away from the sun
uniform float sharpness = 4.0;  // how quickly the three sides hand over
uniform vec3 band_color : source_color = vec3(0.55, 0.85, 1.0);  // NEW: the middle toon band
uniform vec3 foam_color : source_color = vec3(0.96, 0.99, 1.0);  // NEW: white
uniform float streaks_per_metre = 4.0;  // NEW: across a fall
uniform float fall_speed = 2.5;  // NEW: metres a second the streaks fall

varying vec3 world_pos;
varying vec3 world_normal;

void vertex() {
	world_pos = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;
	world_normal = normalize((MODEL_MATRIX * vec4(NORMAL, 0.0)).xyz);
}

// A hard toon edge: 0 below edge, 1 above (a little blend, so it isn't jagged).
float band(float v, float edge) {  // NEW
	return smoothstep(edge - 0.02, edge + 0.02, v);
}

// Streaks on one side view: p.x across (metres), p.y up. Squeezed across, stretched up, sliding
// down with time: 0..1.
float streaks(vec2 p) {  // NEW
	vec2 q = vec2(p.x * streaks_per_metre, (p.y + TIME * fall_speed) * 0.3);
	return noise(q) * 0.65 + noise(q * vec2(2.3, 1.7) + 5.1) * 0.35;
}

void fragment() {
	vec3 n = normalize(world_normal);
	float fall = 1.0 - smoothstep(fall_start - 0.05, fall_start + 0.05, abs(n.y));
	vec3 colour = mix(top_color, fall_color, fall);

	vec3 w = pow(abs(n), vec3(sharpness));
	w /= w.x + w.y + w.z;

	if (fall > 0.0) {  // NEW
		// NEW: the x and z side views only (the top one is the flat water's), weighted
		float s = (streaks(world_pos.zy) * w.x + streaks(world_pos.xy) * w.z) / max(w.x + w.z, 0.001);
		colour = mix(colour, band_color, band(s, 0.42) * fall);  // NEW: light band
		colour = mix(colour, foam_color, band(s, 0.58) * fall);  // NEW: white threads
	}

	ALBEDO = colour;
}

void light() {
	float lit = smoothstep(-0.05, 0.05, dot(NORMAL, LIGHT)) * ATTENUATION;
	DIFFUSE_LIGHT += mix(shade, 1.0, lit) * LIGHT_COLOR / PI;
}
