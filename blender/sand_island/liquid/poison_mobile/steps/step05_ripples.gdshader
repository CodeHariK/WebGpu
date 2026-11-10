// STEP 5 — ripples: a soft pattern of light and dark blobs on the surface (still for now).
// noise(p) (shader_lib/liquid_noise.gdshaderinc) gives every whole point of the plane a random
// number and blends smoothly between them: soft blobs, 0..1. Two sizes added together look like
// ripples. UV is metres along the water (x, z), so ripple_scale is ripples per metre. The colour
// is made a little lighter or darker by them: (r - 0.5) runs -0.5..0.5, times 2 times the strength.
shader_type spatial;
render_mode cull_disabled;

#include "res://shader_lib/liquid_noise.gdshaderinc"  // NEW: hash, noise

uniform vec3 deep_color : source_color = vec3(0.2, 0.55, 0.08);  // the poison's own green
uniform vec3 shallow_color : source_color = vec3(0.75, 0.95, 0.3);  // pale, sickly
uniform float depth_range : hint_range(0.1, 4.0) = 1.2;  // metres from shore to full colour
uniform float ripple_scale = 1.3;  // NEW: about this many ripples a metre
uniform float ripple_strength : hint_range(0.0, 0.5) = 0.12;  // NEW: how much they lighten / darken

varying float depth;

// Two sizes of blobs together look like ripples: 0..1.
float ripples(vec2 p) {  // NEW
	return noise(p) * 0.65 + noise(p * 2.3 + 7.1) * 0.35;
}

void vertex() {
	depth = CUSTOM0.x;
}

void fragment() {
	float deepness = smoothstep(0.0, depth_range, depth);
	vec3 colour = mix(shallow_color, deep_color, deepness);
	float r = ripples(UV * ripple_scale);  // NEW
	colour *= 1.0 + (r - 0.5) * 2.0 * ripple_strength;  // NEW: lighter / darker
	ALBEDO = colour;
	ROUGHNESS = 1.0;
	SPECULAR = 0.0;
}
