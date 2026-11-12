// STEP 3 — lace webs near the falls: the sheet's cell-border web, three times over.
// Each web is the cells' borders (voronoi F1 minus Smooth F1, as in the sheet's step 6) on its
// own cells, nudged by one shared still noise, inside its own round patch centred under the falls
// (the back of the pool): the further out, the more of the web fades. In order:
//   dark    the colour 28 % darker along the borders: a faint shadow web, widest
//   white   1.65 (past white) on slightly smaller cells, a little closer in
//   bright  3.0 on a web thinned a little (its borders cut from 0.08), closest to the falls
// The webs overlap with slightly different cells, so the white lace looks thick and layered where
// the falls land. Nothing moves.
shader_type spatial;
render_mode unshaded, cull_disabled;

// Colours are linear (Blender's numbers), not picked: the grade pushes them past 1.
uniform vec3 teal_color = vec3(0.114, 0.319, 0.305);  // the middle of the pool
uniform vec3 back_color = vec3(0.034, 0.151, 0.138);  // under the falls
uniform vec3 glint_color = vec3(0.334, 0.779, 0.732);  // step 2: the aqua glints
uniform float web_shade = 0.283;  // NEW: step 3: how much the dark web darkens
uniform float web_white = 1.65;  // NEW: step 3: the white web (past white: the grade pushes it)
uniform float web_bright = 3.0;  // NEW: step 3: the brightest web
uniform float sun_light = 0.66;  // Blender's sun on a flat pool (3.17 x cos 53 deg / pi + world 0.05)
uniform vec3 grade = vec3(0.07, 1.5, 2.0);  // hue shift, saturation x, value x

// A random point in each cell of a 3D grid (Dave Hoskins' hash, no sin).
vec3 hash33(vec3 p) {
	p = fract(p * vec3(0.1031, 0.1030, 0.0973));
	p += dot(p, p.yxz + 33.33);
	return fract((p.xxy + p.yxx) * p.zyx);
}

// Voronoi F1: the distance to the nearest of those points, 0 at a point, about 1 at most.
float cells(vec3 p) {
	vec3 cell = floor(p);
	vec3 local_p = p - cell;
	float nearest = 8.0;
	for (int k = -1; k <= 1; k++) {
		for (int j = -1; j <= 1; j++) {
			for (int i = -1; i <= 1; i++) {
				vec3 offset = vec3(float(i), float(j), float(k));
				nearest = min(nearest, length(offset + hash33(cell + offset) - local_p));
			}
		}
	}
	return nearest;
}

// gradient (Perlin) noise in 3D, about -1..1, like Blender's: every whole point gets a random
// slope, and between them the slopes are blended. Most of it stays near 0 (unlike a noise of
// random heights), so Blender's cuts (keep it above 0.59, ...) keep as little as they do there.
float gnoise3(vec3 p) {
	vec3 i = floor(p);
	vec3 f = p - i;
	vec3 u = f * f * f * (f * (f * 6.0 - 15.0) + 10.0);
	float n000 = dot(hash33(i) * 2.0 - 1.0, f);
	float n100 = dot(hash33(i + vec3(1.0, 0.0, 0.0)) * 2.0 - 1.0, f - vec3(1.0, 0.0, 0.0));
	float n010 = dot(hash33(i + vec3(0.0, 1.0, 0.0)) * 2.0 - 1.0, f - vec3(0.0, 1.0, 0.0));
	float n110 = dot(hash33(i + vec3(1.0, 1.0, 0.0)) * 2.0 - 1.0, f - vec3(1.0, 1.0, 0.0));
	float n001 = dot(hash33(i + vec3(0.0, 0.0, 1.0)) * 2.0 - 1.0, f - vec3(0.0, 0.0, 1.0));
	float n101 = dot(hash33(i + vec3(1.0, 0.0, 1.0)) * 2.0 - 1.0, f - vec3(1.0, 0.0, 1.0));
	float n011 = dot(hash33(i + vec3(0.0, 1.0, 1.0)) * 2.0 - 1.0, f - vec3(0.0, 1.0, 1.0));
	float n111 = dot(hash33(i + vec3(1.0, 1.0, 1.0)) * 2.0 - 1.0, f - vec3(1.0, 1.0, 1.0));
	return mix(mix(mix(n000, n100, u.x), mix(n010, n110, u.x), u.y), mix(mix(n001, n101, u.x), mix(n011, n111, u.x), u.y), u.z);
}

// gnoise3 in 3 sizes (Blender's fBM, its "Detail 2"), back to 0..1 around 0.5; roughness:
// how much each smaller size counts.
float fbm3(vec3 p, float roughness) {
	float amp = 1.0;
	float sum = 0.0;
	float total = 0.0;
	for (int i = 0; i < 3; i++) {
		sum += gnoise3(p) * amp;
		total += amp;
		amp *= roughness;
		p *= 2.0;
	}
	return 0.5 + 0.5 * sum / total;
}

// Blender's Quadratic Sphere gradient: 1 at the centre, 0 one unit away, squared.
float sphere(vec3 p) {
	float r = max(1.0 - length(p), 0.0);
	return r * r;
}

// NEW: voronoi F1 minus Smooth F1 (the sheet's step 6): 0 inside the cells, rising along their
// borders. Both from one loop over the 27 nearby cells.
float cell_edges(vec3 p) {  // NEW
	vec3 cell = floor(p);
	vec3 local_p = p - cell;
	float nearest = 8.0;
	float smooth_nearest = 8.0;
	for (int k = -1; k <= 1; k++) {
		for (int j = -1; j <= 1; j++) {
			for (int i = -1; i <= 1; i++) {
				vec3 offset = vec3(float(i), float(j), float(k));
				float d = length(offset + hash33(cell + offset) - local_p);
				nearest = min(nearest, d);
				float h = smoothstep(0.0, 1.0, 0.5 + (smooth_nearest - d));
				smooth_nearest = mix(smooth_nearest, d, h) - 0.5 * h * (1.0 - h);
			}
		}
	}
	return nearest - smooth_nearest;
}

// NEW: one web: the cells' borders where they are over `low`, times a round patch near the falls,
// kept where that is over 0.0227: 1 on the web, 0 off it.
float web(vec3 p, float low, vec3 patch_p) {  // NEW
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

	// NEW: webs: three cell-border webs near the falls, each its own cells (the cells nudged 20 %
	// toward one still noise)
	float nudge = fbm3(b * 5.0, 0.5);  // NEW
	vec3 shade_p = mix(b * 2.84 + vec3(0.0, 1.9, 0.0), vec3(nudge), 0.2);  // NEW
	vec3 white_p = mix(b * vec3(3.9, 2.74, 2.74) + vec3(0.0, 1.96, 0.0), vec3(nudge), 0.2);  // NEW
	vec3 bright_p = mix(b * vec3(4.0, 2.84, 2.84) + vec3(0.0, 1.96, 0.0), vec3(nudge), 0.2);  // NEW
	colour *= 1.0 - web_shade * web(shade_p, 0.0, b * 0.94 + vec3(-0.48, -0.83, 0.0));  // NEW: a dark web
	colour = mix(colour, vec3(web_white), web(white_p, 0.01, b * vec3(0.91, 0.94, 0.94) + vec3(-0.47, -0.89, 0.0)));  // NEW
	colour = mix(colour, vec3(web_bright), web(bright_p, 0.08, b * 0.94 + vec3(-0.48, -0.98, 0.0)));  // NEW

	colour *= sun_light;

	// grade: hue + 0.07, saturation x 1.5, value x 2
	vec3 hsv = rgb_to_hsv(colour);
	hsv = vec3(fract(hsv.x + grade.x), clamp(hsv.y * grade.y, 0.0, 1.0), hsv.z * grade.z);
	ALBEDO = hsv_to_rgb(hsv);
}
