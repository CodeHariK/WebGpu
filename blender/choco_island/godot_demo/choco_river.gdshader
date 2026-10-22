// Chocolate river: glossy syrup that flows downstream, with milk-chocolate streaks, darker banks,
// cream marbling, drifting bubbles, a soft toon light with a sharp candy highlight, ripples that
// catch that highlight, and slow gloopy bulges. No textures.
// (river_steps/ builds this up one idea per file.)
//
// The mesh says which way is downstream through its UVs (choco_mesh.gd):
//   UV.x   0 → 1 across the river (left bank → right bank)
//   UV.y   metres along the river, growing downstream
// The pool, the pond and the sea have no river shape: their UV is the world position
// (x / 4, z), so their syrup drifts slowly toward +Z. They set bank_width to 0.
//
// "Flow space" = UV with the downstream axis sliding back by TIME * flow_speed. Anything drawn in
// flow space moves with the syrup.
shader_type spatial;
render_mode cull_disabled;

group_uniforms flow;
uniform float flow_speed = 1.2;          // metres per second downstream
uniform float streak_length = 3.0;       // metres a streak stretches along the flow
uniform float streak_amount : hint_range(0.0, 1.0) = 0.8;
uniform float bank_width : hint_range(0.0, 0.5) = 0.18;   // 0 = no banks (pool, sea)

group_uniforms colours;
uniform vec3 syrup : source_color = vec3(0.44, 0.22, 0.1);
uniform vec3 syrup_dark : source_color = vec3(0.26, 0.12, 0.05);
uniform vec3 milk : source_color = vec3(0.68, 0.42, 0.22);
uniform vec3 cream : source_color = vec3(0.98, 0.9, 0.78);

group_uniforms marbling;
uniform float marble_lines = 3.0;        // cream lines spread across the river
uniform float marble_width = 0.12;
uniform float marble_amount : hint_range(0.0, 1.0) = 0.8;

group_uniforms bubbles;
uniform float bubble_density : hint_range(0.0, 1.0) = 0.06;
uniform float bubble_size = 0.28;

group_uniforms gloss;
uniform float ripple_strength = 1.2;
uniform float gloss_power = 24.0;
uniform float gloss_strength = 0.9;
uniform vec3 shadow_tint : source_color = vec3(0.72, 0.6, 0.75);
uniform float shadow_light : hint_range(0.0, 1.0) = 0.5;
uniform float band_lit = 0.2;
uniform float band_softness = 0.15;

group_uniforms wobble;
uniform float wobble_height = 0.04;      // metres the syrup bulges up and down
uniform float wobble_length = 2.5;       // metres between bulges

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

void vertex() {
	// A bulge rolling downstream. It depends on UV only, so the copies of a corner that belong to
	// different flat faces move together and the mesh never cracks.
	float roll = sin((UV.y - TIME * flow_speed) / wobble_length * TAU);
	float middle = bank_width > 0.0 ? 1.0 - abs(UV.x * 2.0 - 1.0) : 1.0;   // more in the middle than at the banks
	VERTEX.y += roll * wobble_height * (0.4 + 0.6 * middle);
}

void fragment() {
	vec2 f = flow(UV);

	// 1. syrup with milk-chocolate streaks
	float s = streaks(f);
	vec3 colour = mix(syrup, milk, smoothstep(0.42, 0.75, s) * streak_amount);

	// 2. darker, thicker syrup along the banks (wavy edge)
	float edge = min(UV.x, 1.0 - UV.x) + (noise(vec3(f * vec2(4.0, 0.8), 7.0)) - 0.5) * 0.08;
	float bank = bank_width > 0.0 ? 1.0 - smoothstep(0.0, bank_width, edge) : 0.0;
	colour = mix(colour, syrup_dark, bank * 0.8);

	// 3. cream marbling: lines across the river, bent by noise, only in some patches
	float bend = noise(vec3(f.x * 3.0, f.y * 0.5, 1.0)) * 0.7;
	float lines = abs(sin((f.x + bend) * marble_lines * PI));
	float line = 1.0 - smoothstep(marble_width * 0.5, marble_width, lines);
	float patch = smoothstep(0.55, 0.72, noise(vec3(f.x * 2.0, f.y * 0.15, 5.0)));
	colour = mix(colour, cream, line * patch * marble_amount * (1.0 - bank));

	// 4. bubbles: one cell grid in flow space, a few cells get a little ring
	vec2 cells = vec2(8.0, 3.0);                      // cells across (per UV), per metre along
	vec2 g = f * cells;
	vec2 id = floor(g);
	vec2 local = (fract(g) - 0.5) * vec2(1.0, cells.x / (cells.y * 2.0));  // ~round on a 2 m wide river
	float has = step(1.0 - bubble_density, hash(vec3(id, 9.0)));
	float r = length(local - (vec2(hash(vec3(id, 2.0)), hash(vec3(id, 4.0))) - 0.5) * 0.4);
	float ring = smoothstep(bubble_size - 0.08, bubble_size - 0.03, r) * (1.0 - smoothstep(bubble_size - 0.02, bubble_size, r));
	float dot_ = 1.0 - smoothstep(bubble_size * 0.25, bubble_size * 0.35, length(local - vec2(-0.08, 0.08)));
	colour = mix(colour, milk * 1.3, (ring + dot_ * 0.8) * has * (1.0 - bank));

	// 5. ripples: the streak pattern tilts the normal so the highlight wriggles along
	float e = 0.02;
	float dx = streaks(f + vec2(e, 0.0)) - s;
	float dy = streaks(f + vec2(0.0, e * 4.0)) - s;
	NORMAL_MAP = vec3(0.5 - dx / e * 0.02 * ripple_strength, 0.5 - dy / (e * 4.0) * 0.2 * ripple_strength, 1.0);

	ALBEDO = colour;
	ROUGHNESS = 0.25;
	SPECULAR = 0.3;
}

// Soft two-band toon light with a cool-pink shadow, plus a hard-edged candy highlight.
void light() {
	float ndl = dot(NORMAL, LIGHT) * ATTENUATION;
	float lit = smoothstep(band_lit - band_softness, band_lit + band_softness, ndl);
	DIFFUSE_LIGHT += LIGHT_COLOR / PI * mix(shadow_tint * shadow_light, vec3(1.0), lit);
	float spec = pow(max(dot(NORMAL, normalize(LIGHT + VIEW)), 0.0), gloss_power);
	SPECULAR_LIGHT += LIGHT_COLOR * smoothstep(0.3, 0.4, spec) * gloss_strength * ATTENUATION;
}
