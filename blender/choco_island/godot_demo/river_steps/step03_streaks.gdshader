// Step 3 — streaks. Swap the bands for noise that is squashed across the river and stretched
// along it (streak_length metres), so it reads as long milk-chocolate streaks riding the flow.
shader_type spatial;
render_mode cull_disabled;

uniform vec3 syrup : source_color = vec3(0.44, 0.22, 0.1);
uniform vec3 milk : source_color = vec3(0.68, 0.42, 0.22);
uniform float flow_speed = 1.2;          // metres per second downstream
uniform float streak_length = 3.0;       // metres a streak stretches along the flow
uniform float streak_amount : hint_range(0.0, 1.0) = 0.8;

float hash(vec3 p) {
	p = fract(p * 0.3183099 + 0.1);
	p *= 17.0;
	return fract(p.x * p.y * p.z * (p.x + p.y + p.z));
}

float noise(vec3 x) {
	vec3 i = floor(x);
	vec3 f = fract(x);
	f = f * f * (3.0 - 2.0 * f);
	return mix(mix(mix(hash(i), hash(i + vec3(1, 0, 0)), f.x), mix(hash(i + vec3(0, 1, 0)), hash(i + vec3(1, 1, 0)), f.x), f.y),
			   mix(mix(hash(i + vec3(0, 0, 1)), hash(i + vec3(1, 0, 1)), f.x), mix(hash(i + vec3(0, 1, 1)), hash(i + vec3(1, 1, 1)), f.x), f.y), f.z);
}

// UV → flow space: the downstream axis slides back over time, so patterns ride the current.
vec2 flow(vec2 uv) {
	return vec2(uv.x, uv.y - TIME * flow_speed);
}

// Long streaks: noise squashed across and stretched along the flow. 0..1.
float streaks(vec2 f) {
	return noise(vec3(f.x * 6.0, f.y / streak_length, 0.0)) * 0.7
		 + noise(vec3(f.x * 14.0, f.y / (streak_length * 0.4), 3.0)) * 0.3;
}

void fragment() {
	vec2 f = flow(UV);

	// syrup with milk-chocolate streaks
	float s = streaks(f);
	vec3 colour = mix(syrup, milk, smoothstep(0.42, 0.75, s) * streak_amount);

	ALBEDO = colour;
	ROUGHNESS = 0.25;
	SPECULAR = 0.3;
}
