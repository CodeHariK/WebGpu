// STEP 7 — lines on the top: thin wavy light lines, like light through the ripples. (C: two views)
// The top view of step 4 (x, z) carries the flat water. A noise has blobs; draw a line wherever it
// crosses 0.5 (half way between dark and light), so the lines go round the blobs. abs(n - 0.5) is
// how far the noise is from the line, but in noise units: where the noise is steep the line would
// be thin, where it is flat a wide smear. fwidth(n) is how much the noise changes from one pixel to
// the next, so dividing by it turns that distance into pixels: every line line_pixels wide on
// screen. Two sizes (the second 1.9x smaller, offset) together look richer. Still for now.
//   colour   the lines on the water
//   noise    the noise under the bigger lines, as grey: the lines sit where it is mid grey
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
uniform float lip_width : hint_range(0.0, 0.5) = 0.25;  // how much of the bend is white
uniform float line_scale = 1.2;  // NEW: blobs a metre (the lines go round them)
uniform float line_pixels = 2.0;  // NEW: how wide the lines are, in pixels
uniform int show_channel : hint_enum("colour", "noise") = 0;  // NEW: C in the viewer

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

// A line where the noise crosses 0.5, line_pixels wide on screen.
float contour(float n) {  // NEW
	float pixels = abs(n - 0.5) / max(fwidth(n), 0.0001);  // NEW: pixels from the line
	return 1.0 - smoothstep(line_pixels - 1.0, line_pixels, pixels);  // NEW
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

	float lip = band(abs(n.y), fall_start - lip_width) * (1.0 - band(abs(n.y), fall_start));  // a slice of the bend
	colour = mix(colour, foam_color, lip);

	float top = noise(world_pos.xz * line_scale);  // NEW: the top view
	if (fall < 1.0) {  // NEW: not where it is all fall
		float lines = max(contour(top), contour(noise((world_pos.xz * 1.9 + 11.0) * line_scale)));  // NEW: two sizes
		colour = mix(colour, band_color, lines * (1.0 - fall));  // NEW
	}

	ALBEDO = show_channel == 1 ? vec3(top) : colour;  // NEW
}

void light() {
	float lit = smoothstep(-0.05, 0.05, dot(NORMAL, LIGHT)) * ATTENUATION;
	DIFFUSE_LIGHT += mix(shade, 1.0, lit) * LIGHT_COLOR / PI;
}
