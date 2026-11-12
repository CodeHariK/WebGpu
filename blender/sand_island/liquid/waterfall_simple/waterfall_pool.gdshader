// The pool under the waterfall, simple: the stylized waterfall's pool material ("Material" on
// "Plane.005"), written by hand:
//   bands    dark at the back, teal in the middle, dimmer at the front: lines across the pool
//            bent by voronoi cells
//   glints   aqua dashes: noise stretched sideways, kept where it is high, in a soft round patch
//   webs     three cell-border webs near the falls: a dark one, a white one, a brighter one
//   tiers    six brightness levels, 1.7 x at the back to 0.35 x at the front
//   light    Blender's sun on a flat pool: about two thirds
//   grade    hue + 0.07, saturation x 1.5, value x 2
// Built step by step in pool_steps/ (the last step is this file).
//
// Coordinates: b is Blender's Generated, here the pool's UV (b.x across, b.y front 0 .. back 1,
// b.z 0.5: a flat plane's height).
// The noises are baked: each is one read of a 3D texture (noise/, tools/bake_waterfall_noise.py).
shader_type spatial;
render_mode unshaded, cull_disabled;

// the noises, baked by tools/bake_waterfall_noise.py into two 3D textures (noise/) that
// tile every NOISE_PERIOD noise units; set once for the project (Project Settings > Shader
// Globals), so no material needs them
global uniform sampler3D waterfall_noise_a : filter_linear, repeat_enable;  // F1, borders, value fBMs
global uniform sampler3D waterfall_noise_b : filter_linear, repeat_enable;  // Perlin fBMs, border distance
const float NOISE_PERIOD = 6.0;  // noise units the textures cover before they repeat

// Colours are linear (Blender's numbers), not picked: the grade pushes them past 1.
uniform vec3 teal_color = vec3(0.114, 0.319, 0.305);  // the middle of the pool
uniform vec3 back_color = vec3(0.034, 0.151, 0.138);  // under the falls
uniform vec3 glint_color = vec3(0.334, 0.779, 0.732);  // step 2: the aqua glints
uniform float web_shade = 0.283;  // step 3: how much the dark web darkens
uniform float web_white = 1.65;  // step 3: the white web (past white: the grade pushes it)
uniform float web_bright = 3.0;  // step 3: the brightest web
uniform float back_brightness = 1.7;  // step 4: the back of the pool, x the colour
uniform float front_brightness = 0.35;  // step 4: the front
uniform float sun_light = 0.66;  // Blender's sun on a flat pool (3.17 x cos 53 deg / pi + world 0.05)
uniform vec3 grade = vec3(0.07, 1.5, 2.0);  // hue shift, saturation x, value x

// Voronoi F1, baked: noise_a's red (it was the nearest of 27 cells' points, worked out per pixel).
float cells(vec3 p) {
	return texture(waterfall_noise_a, p / NOISE_PERIOD).r;
}

// gradient (Perlin) noise in 3 sizes (fBM), 0..1 around 0.5, baked: roughness 0.5 is noise_b's
// red, 0.375 its green; both stored 2 x as far from 0.5 (they stay between 0.25 and 0.75).
float fbm3(vec3 p, float roughness) {
	vec4 n = texture(waterfall_noise_b, p / NOISE_PERIOD);
	float stored = roughness > 0.45 ? n.r : n.g;
	return 0.5 + (stored - 0.5) * 0.5;
}

// Blender's Quadratic Sphere gradient: 1 at the centre, 0 one unit away, squared.
float sphere(vec3 p) {
	float r = max(1.0 - length(p), 0.0);
	return r * r;
}

// F1 minus Smooth F1 (the cells' borders), baked: noise_a's green, stored x 2.
float cell_edges(vec3 p) {
	return texture(waterfall_noise_a, p / NOISE_PERIOD).g * 0.5;
}

// one web: the cells' borders where they are over `low`, times a round patch near the falls,
// kept where that is over 0.0227: 1 on the web, 0 off it.
float web(vec3 p, float low, vec3 patch_p) {
	float edges = clamp((cell_edges(p * 5.0) - low) / (1.0 - low), 0.0, 1.0);
	float near_falls = clamp((sphere(patch_p) + 0.03) / 1.08, 0.0, 1.0);
	return step(0.0227, edges * near_falls);
}

vec3 rgb_to_hsv(vec3 c) {
	vec4 k = vec4(0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0);
	vec4 p = mix(vec4(c.bg, k.wz), vec4(c.gb, k.xy), step(c.b, c.g));
	vec4 q = mix(vec4(p.xyw, c.r), vec4(c.r, p.yzx), step(p.x, c.r));
	float d = q.x - min(q.w, q.y);
	return vec3(abs(q.z + (q.w - q.y) / (6.0 * d + 1e-10)), d / (q.x + 1e-10), q.x);
}

vec3 hsv_to_rgb(vec3 c) {
	vec3 p = abs(fract(c.xxx + vec3(1.0, 2.0 / 3.0, 1.0 / 3.0)) * 6.0 - 3.0);
	return c.z * mix(vec3(1.0), clamp(p - 1.0, 0.0, 1.0), c.y);
}

void fragment() {
	vec3 b = vec3(UV.x, 1.0 - UV.y, 0.5);  // Blender's Generated (glTF flips v)

	// middle: the line 0.7 - b.y, bent toward cells; in front of it, the teal at half
	float d1 = cells((vec3(b.x * 2.0, b.y, b.z) + vec3(8.5, 0.0, 2.28 + 0.129 * TIME)) * 8.5);
	float front = step(0.427, mix(0.7 - b.y, d1, 0.25));  // 1 in front of the line
	vec3 colour = teal_color * mix(1.0, 0.5, front);

	// back: the line 1.05 - b.y, bent toward other cells; behind it, the dark colour
	float d2 = cells((vec3(b.x * 1.9, b.y, b.z) + vec3(8.5, 0.0, 3.003 + 0.269 * TIME)) * 7.9);
	float back = 1.0 - step(0.427, mix(1.05 - b.y, d2, 0.25));  // 1 behind the line
	colour = mix(colour, back_color, back);

	// glints: noise stretched along x (squeezed in y: 3.44 x), kept where it is high, in a
	// soft round patch in the middle of the pool; two of them, drifting opposite ways
	float patch_mask = sphere(vec3(b.x, b.y, b.z) * 0.94 + vec3(-0.48, -0.42, 0.0));
	float wide = fbm3((vec3(b.x, b.y * 3.44, b.z) + vec3(0.0, 0.0, 0.81 + 0.224 * TIME)) * 5.76, 0.375);
	float wide_glint = clamp((wide - 0.47) / 0.63, 0.0, 1.0) * clamp(patch_mask / 1.05, 0.0, 1.0);
	float thin = fbm3((vec3(b.x * 4.95, b.y * 3.44, b.z) + vec3(0.0, 0.0, 1.729 - 0.441 * TIME)) * 5.76, 0.375);
	float thin_glint = clamp((thin - 0.59) / 0.51, 0.0, 1.0) * clamp((patch_mask + 0.18) / 1.23, 0.0, 1.0);
	colour = mix(colour, glint_color, step(0.0545, thin_glint));
	colour = mix(colour, glint_color, step(0.0545, wide_glint));

	// webs: three cell-border webs near the falls, each its own cells (the cells nudged 20 %
	// toward one still noise)
	float nudge = fbm3(b * 5.0, 0.5);
	vec3 shade_p = mix(b * 2.84 + vec3(0.0, 1.9, 0.0), vec3(nudge), 0.2);
	vec3 white_p = mix(b * vec3(3.9, 2.74, 2.74) + vec3(0.0, 1.96, 0.0), vec3(nudge), 0.2);
	vec3 bright_p = mix(b * vec3(4.0, 2.84, 2.84) + vec3(0.0, 1.96, 0.0), vec3(nudge), 0.2);
	colour *= 1.0 - web_shade * web(shade_p, 0.0, b * 0.94 + vec3(-0.48, -0.83, 0.0));  // a dark web
	colour = mix(colour, vec3(web_white), web(white_p, 0.01, b * vec3(0.91, 0.94, 0.94) + vec3(-0.47, -0.89, 0.0)));
	colour = mix(colour, vec3(web_bright), web(bright_p, 0.08, b * 0.94 + vec3(-0.48, -0.98, 0.0)));

	// tiers: a line across the pool (1.03 - b.y) pulled 34 % toward a still noise, cut into 6
	// levels: 1.7 x at the back (under the falls) down to 0.35 x at the front
	float tier_line = mix(1.03 - b.y, fbm3(vec3(b.x, b.y, b.z + 1.0) * 5.5, 0.5), 0.342);
	float tier = clamp(floor(clamp(tier_line, 0.0, 1.0) * 6.0) / 5.0, 0.0, 1.0);  // 0, 0.2 .. 1
	colour *= mix(back_brightness, front_brightness, tier);

	colour *= sun_light;

	// grade: hue + 0.07, saturation x 1.5, value x 2
	vec3 hsv = rgb_to_hsv(colour);
	hsv = vec3(fract(hsv.x + grade.x), clamp(hsv.y * grade.y, 0.0, 1.0), hsv.z * grade.z);
	ALBEDO = hsv_to_rgb(hsv);
}
