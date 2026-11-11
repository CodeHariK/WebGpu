// STEP 6 — the lip: a white band where the water bends over the edge of a fall.
// Over the lip the normal turns from up to sideways, so normal.y runs 1 -> 0 there. Two toon cuts
// keep a slice of it: above fall_start - lip_width and below fall_start, i.e. just inside the fall
// where it starts to drop. It needs no extra data and costs no noise; it also outlines a fall's
// sides where they curl round.
shader_type spatial;
render_mode cull_disabled, specular_disabled;

#include "res://shader_lib/liquid_noise.gdshaderinc"

uniform vec3 top_color : source_color = vec3(0.13, 0.5, 0.95);  // the flat water
uniform vec3 fall_color : source_color = vec3(0.2, 0.62, 1.0);  // a fall, lighter
uniform float fall_start : hint_range(0.0, 1.0) = 0.6;  // normal.y below this: a fall
uniform float shade : hint_range(0.0, 1.0) = 0.6;  // light on the side away from the sun
uniform float sharpness = 4.0;  // how quickly the three sides hand over
uniform vec3 band_color : source_color = vec3(0.55, 0.85, 1.0);  // the middle toon band
uniform vec3 foam_color : source_color = vec3(0.96, 0.99, 1.0);  // white
uniform float streaks_per_metre = 4.0;  // across a fall
uniform float fall_speed = 2.5;  // metres a second the streaks fall
uniform float lip_width : hint_range(0.0, 0.5) = 0.25;  // NEW: how much of the bend is white

varying vec3 world_pos;
varying vec3 world_normal;

void vertex() {
	world_pos = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;
	world_normal = normalize((MODEL_MATRIX * vec4(NORMAL, 0.0)).xyz);
}

// A hard toon edge: 0 below edge, 1 above (a little blend, so it isn't jagged).
float band(float v, float edge) {
	return smoothstep(edge - 0.02, edge + 0.02, v);
}

// Streaks on one side view: p.x across (metres), p.y up. Squeezed across, stretched up, sliding
// down with time: 0..1.
float streaks(vec2 p) {
	vec2 q = vec2(p.x * streaks_per_metre, (p.y + TIME * fall_speed) * 0.3);
	return noise(q) * 0.65 + noise(q * vec2(2.3, 1.7) + 5.1) * 0.35;
}

void fragment() {
	vec3 n = normalize(world_normal);
	float fall = 1.0 - smoothstep(fall_start - 0.05, fall_start + 0.05, abs(n.y));
	vec3 colour = mix(top_color, fall_color, fall);

	vec3 w = pow(abs(n), vec3(sharpness));
	w /= w.x + w.y + w.z;

	if (fall > 0.0) {
		// the x and z side views only (the top one is the flat water's), weighted
		float s = (streaks(world_pos.zy) * w.x + streaks(world_pos.xy) * w.z) / max(w.x + w.z, 0.001);
		colour = mix(colour, band_color, band(s, 0.42) * fall);  // light band
		colour = mix(colour, foam_color, band(s, 0.58) * fall);  // white threads
	}

	float lip = band(abs(n.y), fall_start - lip_width) * (1.0 - band(abs(n.y), fall_start));  // NEW: a slice of the bend
	colour = mix(colour, foam_color, lip);  // NEW

	ALBEDO = colour;
}

void light() {
	float lit = smoothstep(-0.05, 0.05, dot(NORMAL, LIGHT)) * ATTENUATION;
	DIFFUSE_LIGHT += mix(shade, 1.0, lit) * LIGHT_COLOR / PI;
}
