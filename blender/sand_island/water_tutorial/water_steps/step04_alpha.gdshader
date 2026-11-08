// STEP 4 — see-through shallows.
// ALPHA = how opaque the pixel is (0 invisible, 1 solid). blend_mix turns on transparency.
// Shallow water lets the sand show through; deep water is almost solid.
shader_type spatial;
render_mode unshaded, blend_mix;  // NEW: blend_mix = transparency on

uniform vec3 shore_colour : source_color = vec3(0.6, 0.93, 0.9);
uniform vec3 shallow_colour : source_color = vec3(0.25, 0.75, 0.88);
uniform vec3 deep_colour : source_color = vec3(0.12, 0.42, 0.85);
uniform float shallow_end = 0.18;
uniform float deep_start = 0.45;

varying float depth;

void vertex() {
	depth = COLOR.r;
}

void fragment() {
	vec3 colour = mix(shore_colour, shallow_colour, smoothstep(0.02, shallow_end, depth));
	colour = mix(colour, deep_colour, smoothstep(deep_start - 0.08, deep_start + 0.08, depth));
	ALBEDO = colour;
	ALPHA = mix(0.55, 0.95, smoothstep(0.0, 0.35, depth));  // NEW: 55% at the shore → 95% deep
}
