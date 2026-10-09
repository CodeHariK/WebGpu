// STEP 3 — colour by depth.
// mix(a, b, t) blends a→b as t goes 0→1. smoothstep(lo, hi, x) is 0 below lo, 1 above hi and a
// soft S-curve between — the main tool for "bands with soft edges".
// "uniform" = a setting you can change in the Inspector without touching code.
shader_type spatial;
render_mode unshaded;

uniform vec3 shore_colour : source_color = vec3(0.6, 0.93, 0.9);    // NEW
uniform vec3 shallow_colour : source_color = vec3(0.25, 0.75, 0.88); // NEW
uniform vec3 deep_colour : source_color = vec3(0.12, 0.42, 0.85);    // NEW
uniform float shallow_end = 0.18;  // NEW: depth where shore colour has turned into shallow
uniform float deep_start = 0.45;   // NEW: depth where it turns deep

varying float depth;

void vertex() {
	depth = COLOR.r;
}

void fragment() {
	vec3 colour = mix(shore_colour, shallow_colour, smoothstep(0.02, shallow_end, depth));          // NEW
	colour = mix(colour, deep_colour, smoothstep(deep_start - 0.08, deep_start + 0.08, depth));     // NEW
	ALBEDO = colour;
}
