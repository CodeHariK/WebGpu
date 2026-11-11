// STEP 3 — dark lines: a few thin dark vertical lines down the whole sheet.
// The same Wave Texture as step 2, now on the box coordinates b (across, height): wider bands
// (sin of x times 1.4 x 20 goes round about 4.5 times across the sheet), barely bent (distortion
// 0.3), so they run almost straight down.
// This time the bottom of each band is kept: where the wave is under 0.005 (a sliver around each
// trough, about a twentieth of a band) the colour is halved. Lines that thin are about a pixel
// wide on screen, so fwidth again keeps their edges smooth instead of broken. They don't move.
shader_type spatial;
render_mode unshaded, cull_disabled;

#include "res://shader_lib/liquid_noise.gdshaderinc"

// Colours are linear (Blender's numbers), not picked: the grade pushes them past 1.
uniform vec3 deep_color = vec3(0.007, 0.060, 0.072); // layer 1, upper tone
uniform vec3 deep_color_2 = vec3(0.031, 0.111, 0.126); // layer 1, lower tone
uniform float deep_value = 1.65; // layer 1's brightness
uniform vec3 light_color = vec3(0.087, 0.317, 0.320); // layer 2
uniform vec3 aqua_color = vec3(0.214, 0.6, 0.539); // layer 3
uniform float streak_brightness = 2.4; // step 2: the streaks, x the colour
uniform float line_darkness = 0.5;  // NEW: step 3: the dark lines, x the colour
uniform vec3 grade = vec3(0.06, 1.7, 1.9); // hue shift, saturation x, value x
uniform vec3 box_min = vec3(-3.92, 0.0, -7.79); // the sheet's box, Godot's axes
uniform vec3 box_size = vec3(7.96, 13.49, 9.19);

varying vec3 b; // Generated, Blender's order: across, depth, height

void vertex() {
	vec3 p = (VERTEX - box_min) / box_size; // Godot's axes: x across, y up, z toward the front
	b = vec3(p.x, 1.0 - p.z, p.y);
}

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

// noise of 3 sizes added (fBM), 0..1: soft blobs with a little detail.
float fbm(vec2 p) {
	return noise(p) * 0.57 + noise(p * 2.0 + 3.1) * 0.29 + noise(p * 4.0 + 7.7) * 0.14;
}

// value noise in 3D, 0..1, so a pattern can change over time (a slice moving through it).
float noise3(vec3 p) {
	vec3 i = floor(p);
	vec3 f = p - i;
	vec3 u = f * f * (3.0 - 2.0 * f);
	float n000 = hash33(i).x;
	float n100 = hash33(i + vec3(1.0, 0.0, 0.0)).x;
	float n010 = hash33(i + vec3(0.0, 1.0, 0.0)).x;
	float n110 = hash33(i + vec3(1.0, 1.0, 0.0)).x;
	float n001 = hash33(i + vec3(0.0, 0.0, 1.0)).x;
	float n101 = hash33(i + vec3(1.0, 0.0, 1.0)).x;
	float n011 = hash33(i + vec3(0.0, 1.0, 1.0)).x;
	float n111 = hash33(i + vec3(1.0, 1.0, 1.0)).x;
	return mix(mix(mix(n000, n100, u.x), mix(n010, n110, u.x), u.y), mix(mix(n001, n101, u.x), mix(n011, n111, u.x), u.y), u.z);
}

// Blender's Wave Texture (bands across x, sine): stripes 0..1, each band bent sideways by noise.
float wave(vec2 p, float distortion) {
	float n = p.x * 20.0 + distortion * (fbm(p) * 2.0 - 1.0);
	return 0.5 + 0.5 * sin(n - PI * 0.5);
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
	// layer 1: the height line 0.86 - height, bent toward the cells' distance (24 %), cut at 0.467:
	// above it the upper tone, below the lower one
	float d1 = cells(vec3(b.x * 1.29, b.y, b.z + 0.296 * TIME) * 7.0);
	float line1 = mix(0.86 - b.z, d1, 0.242);
	vec3 colour = mix(deep_color, deep_color_2, step(0.467, line1)) * deep_value;

	// layer 2: light teal where another bent height line is under 0.382 (the top part)
	float d2 = cells((vec3(b.x * 1.19, b.y, b.z + 0.129 * TIME) + vec3(8.5, 0.0, 2.28)) * 7.2);
	float line2 = mix(0.92 - b.z, d2, 0.183);
	colour = mix(light_color, colour, step(0.382, line2));

	// layer 3: aqua circles, the middles of cells (closer than 0.487 to a point), in a soft round
	// patch near the top (UV: v 0.84, toward the right), its edge wobbled by noise
	float d3 = cells((vec3(b.x, b.y * 3.22, b.z + 0.245 * TIME) + vec3(0.18, 0.0, 0.0)) * 5.4);
	float circle = step(d3, 0.487);
	vec2 uv = vec2(UV.x, 1.0 - UV.y); // Blender's UV (glTF flips v)
	vec2 patch_p = vec2(uv.x - 0.93, uv.y * 5.16 - 4.34);
	patch_p = mix(patch_p, vec2(noise(b.xz * 4.1)), 0.445);
	float patch_mask = pow(max(1.0 - length(patch_p), 0.0), 2.0);
	patch_mask = clamp((patch_mask - 0.14) / 0.86, 0.0, 1.0);
	colour = mix(colour, aqua_color, step(0.255, circle * patch_mask));

	// streaks: the crests of bent stripes across the sheet (u), thin lines; static
	float stripes = wave(uv * 2.38, 3.1);
	float edge = fwidth(stripes);  // how much it changes over one pixel
	float lines = smoothstep(0.98 - edge, 0.98 + edge, stripes);
	// the mask: a wave turned on its side (bands up the sheet, v), sliding down
	// 1.2 sheets a second, its crests times a slowly changing noise, cut at 0.173
	float sideways = wave(vec2(-uv.y - 8.56 - 1.195 * TIME, uv.x) * 0.36, 4.18);
	float crests = clamp((sideways - 0.86) / 0.14, 0.0, 1.0);
	float blobs = noise3(vec3(uv.x + 8.5, uv.y * 4.0, 3.0 + 0.27 * TIME) * 4.0);
	blobs = clamp((blobs - 0.04) / 0.96, 0.0, 1.0);
	float mask = step(0.173, crests * blobs);
	colour = mix(colour, colour * streak_brightness, lines * mask);  // 2.4 x brighter

	// NEW: dark lines: the troughs of nearly straight stripes, 4 or 5 across the sheet; static
	float grain = wave(vec2((b.x + 0.94) * 1.4, b.z * 1.4), 0.3);  // NEW
	float grain_edge = fwidth(grain);  // NEW
	float dark = 1.0 - smoothstep(0.005 - grain_edge, 0.005 + grain_edge, grain);  // NEW: 1 in a trough
	colour = mix(colour, colour * line_darkness, dark);  // NEW: half as bright

	// grade: hue + 0.06, saturation x 1.7, value x 1.9
	vec3 hsv = rgb_to_hsv(colour);
	hsv = vec3(fract(hsv.x + grade.x), clamp(hsv.y * grade.y, 0.0, 1.0), hsv.z * grade.z);
	ALBEDO = hsv_to_rgb(hsv);
}
