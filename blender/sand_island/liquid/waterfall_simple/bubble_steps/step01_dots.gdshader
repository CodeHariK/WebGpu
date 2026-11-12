// STEP 1 — bubbles: small white dots in a round patch, popping in and out.
// Blender's "bubble" material, on a few flat 2 x 2 squares standing in front of the falls (the
// scene's Bubbles). A voronoi on the square's own position (about 4 cells across), and where a
// pixel is within 0.2 of its cell's point: a dot (a bubble). A round patch keeps them to the
// middle (the sphere gradient of the sheet's step 4, on Blender's Generated: here the square's
// position * 0.5 + 0.5; a flat square's height there is 0.5, so the sphere is cut through near
// its middle). Everything else is cut away (alpha scissor). The voronoi's third dimension slides
// with time (0.92 a second): its points pass through the square's slice, so dots appear, swell,
// shrink and vanish where they please.
shader_type spatial;
render_mode unshaded, cull_disabled;

uniform vec3 bubble_color = vec3(1.3);  // NEW: linear, a little past white

// NEW: a random point in each cell of a 3D grid (Dave Hoskins' hash, no sin).
vec3 hash33(vec3 p) {  // NEW
	p = fract(p * vec3(0.1031, 0.1030, 0.0973));
	p += dot(p, p.yxz + 33.33);
	return fract((p.xxy + p.yxx) * p.zyx);
}

// NEW: voronoi F1: the distance to the nearest point.
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

varying vec3 object_p;  // NEW: the square's own position (Blender's Object coordinates)

void vertex() {
	object_p = VERTEX;  // NEW
}

void fragment() {
	float dot_in = step(cells((object_p + vec3(0.0, 0.0, 0.92 * TIME)) * 1.93), 0.2);  // NEW: 1 on a bubble
	vec3 q = (object_p * 0.5 + 0.5) + vec3(-0.5, -0.48, -0.07);  // NEW: Generated, moved
	float r = max(1.0 - length(q), 0.0);  // NEW: the sphere gradient
	float in_patch = step(0.141, r * r);  // NEW
	ALBEDO = bubble_color;  // NEW
	ALPHA = dot_in * in_patch;  // NEW
	ALPHA_SCISSOR_THRESHOLD = 0.5;  // NEW
}
