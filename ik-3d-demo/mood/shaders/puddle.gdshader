// A rain puddle that works on mobile: an almost black mirror (very smooth, full specular), so it
// picks up the sky, the reflection probe and every neon light's highlight; rain rings ripple out
// across it by bending the normal (a grid of cells, each starting a ring at its own random time).
shader_type spatial;
render_mode specular_schlick_ggx;

uniform vec4 tint : source_color = vec4(0.02, 0.02, 0.03, 1.0);
uniform float rain = 1.0; // 0 = still water

float hash(vec2 p) {
	return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453);
}

void fragment() {
	ALBEDO = tint.rgb;
	METALLIC = 0.0;
	ROUGHNESS = 0.03;
	SPECULAR = 1.0;
	vec2 world = (INV_VIEW_MATRIX * vec4(VERTEX, 1.0)).xz * 2.5;
	vec2 cell = floor(world);
	vec2 local = fract(world) - 0.5;
	float seed = hash(cell);
	vec2 center = vec2(seed, hash(cell + 7.1)) * 0.5 - 0.25;
	float t = fract(TIME * 0.9 + seed);
	vec2 away = local - center;
	float d = length(away);
	float ring = sin((d - t * 0.45) * 55.0) * smoothstep(t * 0.45 + 0.08, t * 0.45, d) * (1.0 - t) * rain;
	vec2 tilt = (d > 0.001 ? away / d : vec2(0.0)) * ring * 0.35;
	NORMAL = normalize(NORMAL + (VIEW_MATRIX * vec4(tilt.x, 0.0, tilt.y, 0.0)).xyz);
}
