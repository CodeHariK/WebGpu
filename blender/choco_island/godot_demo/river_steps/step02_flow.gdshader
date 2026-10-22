// Step 2 — which way is downstream. The mesh's UVs carry it: UV.x goes 0 → 1 across the river,
// UV.y is metres along it. flow() slides UV.y back by TIME * flow_speed, so anything drawn with
// it moves downstream. Here: 1 m bands, to see the direction and speed (falls run fast, the pool
// and sea barely drift — each surface has its own flow_speed).
shader_type spatial;
render_mode cull_disabled;

uniform vec3 syrup : source_color = vec3(0.44, 0.22, 0.1);
uniform vec3 milk : source_color = vec3(0.68, 0.42, 0.22);
uniform float flow_speed = 1.2;          // metres per second downstream

// UV → flow space: the downstream axis slides back over time, so patterns ride the current.
vec2 flow(vec2 uv) {
	return vec2(uv.x, uv.y - TIME * flow_speed);
}

void fragment() {
	vec2 f = flow(UV);
	float band = step(0.5, fract(f.y));
	ALBEDO = mix(syrup, milk, band * 0.6);
	ROUGHNESS = 0.25;
	SPECULAR = 0.3;
}
