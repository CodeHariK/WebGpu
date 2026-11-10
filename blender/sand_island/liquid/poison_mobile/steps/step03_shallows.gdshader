// STEP 3 — shallow and deep: a pale sickly green at the edges, the poison's own green where deep.
// smoothstep(a, b, x) is 0 below a, 1 above b and a smooth ramp in between: the "deepness".
// mix(x, y, t) blends two values: x at t = 0, y at t = 1. So the edges get shallow_color, deep
// water deep_color, and the depth in between a blend. Lit again (the debug view is over).
shader_type spatial;
render_mode cull_disabled;

uniform vec3 deep_color : source_color = vec3(0.2, 0.55, 0.08);  // the poison's own green
uniform vec3 shallow_color : source_color = vec3(0.75, 0.95, 0.3);  // NEW: pale, sickly
uniform float depth_range : hint_range(0.1, 4.0) = 1.2;  // metres from shore to full colour

varying float depth;

void vertex() {
	depth = CUSTOM0.x;
}

void fragment() {
	float deepness = smoothstep(0.0, depth_range, depth);  // NEW: 0 at the edge .. 1 deep
	vec3 colour = mix(shallow_color, deep_color, deepness);  // NEW
	ALBEDO = colour;
	ROUGHNESS = 1.0;
	SPECULAR = 0.0;
}
