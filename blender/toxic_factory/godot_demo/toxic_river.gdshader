// Toxic river: murky polluted water with glowing green sludge slicks that spread downstream from
// the factory outfalls, dark cartoon outlines round each slick, bubbles popping in the sludge,
// dirty scum along the banks, toon light with a hard gloss, ripples and a slow swell. No textures.
//
// Same flow idea as choco_island's choco_river.gdshader:
//   UV.x   0 → 1 across the river,   UV.y   metres downstream
//   flow() slides UV.y back by TIME * flow_speed, so whatever is drawn with it rides the current.
// New here:
//   - the slicks are noise blobs cut with a threshold; the threshold drops near the outfalls
//     (world position vs. the `outfalls` uniform), so there's lots of sludge just below each pipe
//     and less and less further downstream
//   - a second, slightly lower threshold gives a dark ring round every blob (the cartoon outline)
//   - sludge glows (EMISSION), bubbles grow and pop inside it
// sludge_everywhere = true makes the whole surface sludge (the pours from the pipes, the yard pond).
shader_type spatial;
render_mode cull_disabled;

group_uniforms flow;
uniform float flow_speed = 0.9;
uniform float streak_length = 4.0;
uniform float bank_width : hint_range(0.0, 0.5) = 0.08;   // scum along the banks (0 = none)

group_uniforms colours;
uniform vec3 water : source_color = vec3(0.2, 0.38, 0.38);
uniform vec3 water_light : source_color = vec3(0.34, 0.5, 0.44);
uniform vec3 sludge : source_color = vec3(0.47, 0.96, 0.2);
uniform vec3 sludge_light : source_color = vec3(0.8, 1.0, 0.4);
uniform vec3 outline : source_color = vec3(0.12, 0.42, 0.1);
uniform vec3 scum : source_color = vec3(0.78, 0.76, 0.6);
uniform float glow = 0.6;

group_uniforms slicks;
uniform bool sludge_everywhere = false;
uniform vec2 outfalls[4];              // world (x, z) of each outfall mouth (unused ones far away)
uniform float slick_base : hint_range(0.0, 1.0) = 0.12;   // how much sludge far from any outfall
uniform float plume_length = 22.0;     // metres downstream until a plume has mostly faded
uniform float plume_width = 4.0;       // metres either side, growing downstream
uniform float outline_width = 0.035;

group_uniforms bubbles;
uniform float bubble_density : hint_range(0.0, 1.0) = 0.25;
uniform float bubble_speed = 0.7;

group_uniforms gloss;
uniform float ripple_strength = 1.0;
uniform float gloss_power = 24.0;
uniform float gloss_strength = 0.7;
uniform vec3 shadow_tint : source_color = vec3(0.6, 0.65, 0.6);
uniform float shadow_light : hint_range(0.0, 1.0) = 0.5;
uniform float band_lit = 0.2;
uniform float band_softness = 0.15;

group_uniforms wobble;
uniform float wobble_height = 0.05;
uniform float wobble_length = 4.0;

varying vec3 world_pos;

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

vec2 flow(vec2 uv) {
	return vec2(uv.x, uv.y - TIME * flow_speed);
}

float streaks(vec2 f) {
	return noise(vec3(f.x * 6.0, f.y / streak_length, 0.0)) * 0.7
		 + noise(vec3(f.x * 14.0, f.y / (streak_length * 0.4), 3.0)) * 0.3;
}

// Blobby slick pattern in flow space (round-ish blobs a few metres long). 0..1.
float blobs(vec2 f) {
	return noise(vec3(f.x * 4.0, f.y * 0.35, 11.0)) * 0.65 + noise(vec3(f.x * 9.0, f.y * 0.8, 13.0)) * 0.35;
}

// How much sludge the outfalls put here: strong just below a mouth, fading downstream (+X).
float plume(vec2 p) {
	float total = 0.0;
	for (int i = 0; i < 4; i++) {
		vec2 d = p - outfalls[i];
		float down = d.x > 0.0 ? exp(-d.x / plume_length) : exp(d.x * 0.6);
		float spread = plume_width * (1.0 + max(d.x, 0.0) * 0.08);
		total += down * exp(-(d.y * d.y) / (spread * spread));
	}
	return clamp(total, 0.0, 1.0);
}

void vertex() {
	world_pos = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;
	float roll = sin((UV.y - TIME * flow_speed) / wobble_length * TAU);
	float middle = bank_width > 0.0 ? 1.0 - abs(UV.x * 2.0 - 1.0) : 1.0;
	VERTEX.y += roll * wobble_height * (0.4 + 0.6 * middle);
}

void fragment() {
	vec2 f = flow(UV);
	float s = streaks(f);

	// 1. murky water with lighter streaks
	vec3 colour = mix(water, water_light, smoothstep(0.45, 0.8, s) * 0.7);

	// 2. sludge slicks: blobs cut by a threshold that drops near the outfalls
	float amount = sludge_everywhere ? 1.0 : clamp(slick_base + plume(world_pos.xz) * 0.75, 0.0, 0.95);
	float cut = 1.0 - amount;
	float b = blobs(f);
	float inside = sludge_everywhere ? 1.0 : smoothstep(cut, cut + 0.02, b);
	float ring = sludge_everywhere ? 0.0 : smoothstep(cut - outline_width - 0.02, cut - outline_width, b) * (1.0 - inside);
	vec3 goo = mix(sludge, sludge_light, smoothstep(0.55, 0.85, s) * 0.6);
	colour = mix(colour, outline, ring);
	colour = mix(colour, goo, inside);

	// 3. bubbles in the sludge: each cell's ring grows over its own cycle, then pops
	vec2 cells = vec2(10.0, 1.5);
	vec2 g = f * cells;
	vec2 id = floor(g);
	vec2 local = (fract(g) - 0.5) * vec2(1.0, cells.x / (cells.y * 8.0));
	float has = step(1.0 - bubble_density, hash(vec3(id, 9.0)));
	float life = fract(TIME * bubble_speed + hash(vec3(id, 3.0)));
	float r = length(local - (vec2(hash(vec3(id, 2.0)), hash(vec3(id, 4.0))) - 0.5) * 0.3);
	float size = 0.08 + 0.25 * life;
	float bubble = smoothstep(size - 0.05, size - 0.02, r) * (1.0 - smoothstep(size, size + 0.02, r)) * (1.0 - life);
	colour = mix(colour, sludge_light * 1.2, bubble * has * inside);

	// 4. dirty scum along the banks
	float edge = min(UV.x, 1.0 - UV.x) + (noise(vec3(f * vec2(5.0, 0.6), 7.0)) - 0.5) * 0.05;
	float bank = bank_width > 0.0 ? 1.0 - smoothstep(bank_width * 0.5, bank_width, edge) : 0.0;
	colour = mix(colour, scum, bank * 0.85 * (1.0 - inside));

	// 5. ripples tilt the normal so the gloss wriggles
	float e = 0.02;
	float dx = streaks(f + vec2(e, 0.0)) - s;
	float dy = streaks(f + vec2(0.0, e * 4.0)) - s;
	NORMAL_MAP = vec3(0.5 - dx / e * 0.02 * ripple_strength, 0.5 - dy / (e * 4.0) * 0.2 * ripple_strength, 1.0);

	ALBEDO = colour;
	EMISSION = goo * glow * inside;
	ROUGHNESS = 0.25;
	SPECULAR = 0.3;
}

void light() {
	float ndl = dot(NORMAL, LIGHT) * ATTENUATION;
	float lit = smoothstep(band_lit - band_softness, band_lit + band_softness, ndl);
	DIFFUSE_LIGHT += LIGHT_COLOR / PI * mix(shadow_tint * shadow_light, vec3(1.0), lit);
	float spec = pow(max(dot(NORMAL, normalize(LIGHT + VIEW)), 0.0), gloss_power);
	SPECULAR_LIGHT += LIGHT_COLOR * smoothstep(0.3, 0.4, spec) * gloss_strength * ATTENUATION;
}
