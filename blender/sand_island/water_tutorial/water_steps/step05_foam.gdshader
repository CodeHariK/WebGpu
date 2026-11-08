// STEP 5 — shore foam.
// "1 - smoothstep(...)" flips the S-curve: 1 (foam) where the water is shallower than foam_width,
// fading to 0 just past it. Then we mix white in where foam is 1.
shader_type spatial;
render_mode unshaded, blend_mix;

uniform vec3 shore_colour : source_color = vec3(0.6, 0.93, 0.9);
uniform vec3 shallow_colour : source_color = vec3(0.25, 0.75, 0.88);
uniform vec3 deep_colour : source_color = vec3(0.12, 0.42, 0.85);
uniform float shallow_end = 0.18;
uniform float deep_start = 0.45;
uniform vec3 foam_colour : source_color = vec3(1.0);  // NEW
uniform float foam_width = 0.03;                       // NEW: foam band, in depth units

varying float depth;

void vertex() {
	depth = COLOR.r;
}

void fragment() {
	vec3 colour = mix(shore_colour, shallow_colour, smoothstep(0.02, shallow_end, depth));
	colour = mix(colour, deep_colour, smoothstep(deep_start - 0.08, deep_start + 0.08, depth));
	float foam = 1.0 - smoothstep(foam_width - 0.004, foam_width, depth);  // NEW: 1 = foam
	ALBEDO = mix(colour, foam_colour, foam);                               // NEW
	ALPHA = max(mix(0.55, 0.95, smoothstep(0.0, 0.35, depth)), foam);      // NEW: foam is solid
}
