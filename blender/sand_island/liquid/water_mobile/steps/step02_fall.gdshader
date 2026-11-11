// STEP 2 — flat or falling: the flat water one blue, the falls a lighter one.
// normal.y is 1 facing straight up, 0 facing sideways. smoothstep turns it into "fall": 0 on the
// flat, 1 once normal.y drops below fall_start, with a narrow blend (0.1 wide) so the edge is crisp
// but not jagged. abs(): a fall that curls under at its foot faces a little down. mix() picks the
// colour. Lit again (the debug view is over); specular_disabled: no shiny highlight (a cartoon has
// none, and it is cheaper).
shader_type spatial;
render_mode cull_disabled, specular_disabled;  // NEW: specular_disabled

uniform vec3 top_color : source_color = vec3(0.13, 0.5, 0.95);  // NEW: the flat water
uniform vec3 fall_color : source_color = vec3(0.2, 0.62, 1.0);  // NEW: a fall, lighter
uniform float fall_start : hint_range(0.0, 1.0) = 0.6;  // NEW: normal.y below this: a fall

varying vec3 world_normal;

void vertex() {
	world_normal = normalize((MODEL_MATRIX * vec4(NORMAL, 0.0)).xyz);
}

void fragment() {
	vec3 n = normalize(world_normal);
	float fall = 1.0 - smoothstep(fall_start - 0.05, fall_start + 0.05, abs(n.y));  // NEW: 0 flat, 1 a fall
	vec3 colour = mix(top_color, fall_color, fall);  // NEW
	ALBEDO = colour;
}
