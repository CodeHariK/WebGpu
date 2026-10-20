// One dance-floor tile: dark smoked glass with a neon rim and a glowing core.
// Per tile (MultiMesh custom data): rgb = light colour, a = brightness (0 = off).
shader_type spatial;
render_mode specular_schlick_ggx;

uniform vec3 half_size = vec3(1.05, 0.12, 1.05); // tile half extents (matches the BoxMesh)
uniform vec3 glass : source_color = vec3(0.035, 0.02, 0.06);
uniform float rim_width = 0.07; // metres of neon edge
uniform float energy = 4.0;
uniform float idle_glow = 0.08; // how much a dark tile still shows its rim

varying vec3 local;
varying vec4 custom;

void vertex() {
	local = VERTEX;
	custom = INSTANCE_CUSTOM;
}

void fragment() {
	vec3 light = custom.rgb;
	float on = custom.a;
	bool top = local.y > half_size.y * 0.9;

	// Distance (m) from the nearest edge of the top face.
	vec2 edge = half_size.xz - abs(local.xz);
	float to_edge = min(edge.x, edge.y);
	float rim = 1.0 - smoothstep(rim_width * 0.6, rim_width, to_edge);
	// Core: brightest in the middle, a soft square falloff toward the rim.
	vec2 q = abs(local.xz) / half_size.xz;
	float core = 1.0 - smoothstep(0.15, 0.95, max(q.x, q.y));
	// Faint inner square line, like the frosted panels in the concept.
	float inner = 1.0 - smoothstep(0.0, 0.02, abs(max(q.x, q.y) - 0.62));

	ALBEDO = glass;
	ROUGHNESS = 0.12;
	METALLIC = 0.0;
	SPECULAR = 0.7;
	float glow = 0.0;
	if (top) {
		glow = rim * (idle_glow + on * 1.6) + core * on * 0.55 + inner * on * 0.6;
	} else {
		glow = on * 0.25; // the tile's sides glow a little
	}
	EMISSION = light * glow * energy;
}
