// STEP 1 — the pool in bands: dark at the back, teal in the middle, dimmer at the front.
// The pool is one flat quad; its UV is Blender's Generated here (b.x across, b.y from the front
// (0) to the back under the falls (1); a flat plane's height b.z is 0.5). Like the sheet's step 1,
// a line (this time across the pool, by b.y) is bent toward a voronoi's distance and cut, two times:
//   middle   the line 0.7 - b.y bent 25 % toward cells: behind it the teal, in front of it the
//            teal at half (the front of the pool is dimmer)
//   back     the line 1.05 - b.y bent 25 % toward other cells: behind it (toward the falls) a
//            dark green-blue
//   light    Blender lights it with the sun: about two thirds of its colour, from above
//   grade    hue + 0.07, saturation x 1.5, value x 2 (the pool's own last push)
// The cells change slowly (Blender moves them up through a 3D voronoi: the pool is flat, so they
// don't slide, they bubble in place).
shader_type spatial;
render_mode unshaded, cull_disabled;

// Colours are linear (Blender's numbers), not picked: the grade pushes them past 1.
uniform vec3 teal_color = vec3(0.114, 0.319, 0.305);  // NEW: the middle of the pool
uniform vec3 back_color = vec3(0.034, 0.151, 0.138);  // NEW: under the falls
uniform float sun_light = 0.66;  // NEW: Blender's sun on a flat pool (3.17 x cos 53 deg / pi + world 0.05)
uniform vec3 grade = vec3(0.07, 1.5, 2.0);  // NEW: hue shift, saturation x, value x

// A random point in each cell of a 3D grid (Dave Hoskins' hash, no sin).
vec3 hash33(vec3 p) {  // NEW
	p = fract(p * vec3(0.1031, 0.1030, 0.0973));
	p += dot(p, p.yxz + 33.33);
	return fract((p.xxy + p.yxx) * p.zyx);
}

// Voronoi F1: the distance to the nearest of those points, 0 at a point, about 1 at most.
float cells(vec3 p) {  // NEW
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

vec3 rgb_to_hsv(vec3 c) {  // NEW
	vec4 k = vec4(0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0);
	vec4 p = mix(vec4(c.bg, k.wz), vec4(c.gb, k.xy), step(c.b, c.g));
	vec4 q = mix(vec4(p.xyw, c.r), vec4(c.r, p.yzx), step(p.x, c.r));
	float d = q.x - min(q.w, q.y);
	return vec3(abs(q.z + (q.w - q.y) / (6.0 * d + 1e-10)), d / (q.x + 1e-10), q.x);
}

vec3 hsv_to_rgb(vec3 c) {  // NEW
	vec3 p = abs(fract(c.xxx + vec3(1.0, 2.0 / 3.0, 1.0 / 3.0)) * 6.0 - 3.0);
	return c.z * mix(vec3(1.0), clamp(p - 1.0, 0.0, 1.0), c.y);
}

void fragment() {
	vec3 b = vec3(UV.x, 1.0 - UV.y, 0.5);  // NEW: Blender's Generated (glTF flips v)

	// NEW: middle: the line 0.7 - b.y, bent toward cells; in front of it, the teal at half
	float d1 = cells((vec3(b.x * 2.0, b.y, b.z) + vec3(8.5, 0.0, 2.28 + 0.129 * TIME)) * 8.5);  // NEW
	float front = step(0.427, mix(0.7 - b.y, d1, 0.25));  // NEW: 1 in front of the line
	vec3 colour = teal_color * mix(1.0, 0.5, front);  // NEW

	// NEW: back: the line 1.05 - b.y, bent toward other cells; behind it, the dark colour
	float d2 = cells((vec3(b.x * 1.9, b.y, b.z) + vec3(8.5, 0.0, 3.003 + 0.269 * TIME)) * 7.9);  // NEW
	float back = 1.0 - step(0.427, mix(1.05 - b.y, d2, 0.25));  // NEW: 1 behind the line
	colour = mix(colour, back_color, back);  // NEW

	colour *= sun_light;  // NEW

	// NEW: grade: hue + 0.07, saturation x 1.5, value x 2
	vec3 hsv = rgb_to_hsv(colour);  // NEW
	hsv = vec3(fract(hsv.x + grade.x), clamp(hsv.y * grade.y, 0.0, 1.0), hsv.z * grade.z);  // NEW
	ALBEDO = hsv_to_rgb(hsv);  // NEW
}
