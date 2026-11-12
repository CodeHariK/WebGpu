// STEP 2 — wobble: the squares crumple, scattering the bubbles in depth. The finished bubbles.
// Blender's bubble squares are subdivided and pushed along their normal by a cloud noise (crumples
// 0.41 m across) seen from an empty sinking 2.2 m a second, so each square bends back and forth
// and its dots end up at different depths, drifting like real bubbles. Here the vertex shader
// does it with Perlin noise (2 sizes) in world space, read higher up as time goes on. The dots
// are taken from the flat square (object_p is set before the push), so they ride along with it.
// (waterfall_bubble.gdshader is this step.)
shader_type spatial;
render_mode unshaded, cull_disabled;

uniform vec3 bubble_color = vec3(1.3);  // linear, a little past white
uniform float wobble_strength = 0.5;  // NEW: step 2: metres per noise unit
uniform float wobble_size = 0.41;  // NEW: step 2: metres a crumple (Blender's Clouds size)
uniform float wobble_fall = 2.2;  // NEW: step 2: metres a second the crumples sink

// a random point in each cell of a 3D grid (Dave Hoskins' hash, no sin).
vec3 hash33(vec3 p) {
	p = fract(p * vec3(0.1031, 0.1030, 0.0973));
	p += dot(p, p.yxz + 33.33);
	return fract((p.xxy + p.yxx) * p.zyx);
}

// voronoi F1: the distance to the nearest point.
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

varying vec3 object_p;  // the square's own position (Blender's Object coordinates)

// NEW: gradient (Perlin) noise in 3D, about -1..1 (the pool's step 2).
float gnoise3(vec3 p) {  // NEW
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

void vertex() {
	object_p = VERTEX;
	// NEW: crumple the square: pushed along its normal by a noise in the world that sinks
	vec3 world = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz + vec3(0.0, wobble_fall * TIME, 0.0);  // NEW
	float n = gnoise3(world / wobble_size) + 0.5 * gnoise3(world * 2.0 / wobble_size);  // NEW
	VERTEX += NORMAL * n * wobble_strength;  // NEW
}

void fragment() {
	float dot_in = step(cells((object_p + vec3(0.0, 0.0, 0.92 * TIME)) * 1.93), 0.2);  // 1 on a bubble
	vec3 q = (object_p * 0.5 + 0.5) + vec3(-0.5, -0.48, -0.07);  // Generated, moved
	float r = max(1.0 - length(q), 0.0);  // the sphere gradient
	float in_patch = step(0.141, r * r);
	ALBEDO = bubble_color;
	ALPHA = dot_in * in_patch;
	ALPHA_SCISSOR_THRESHOLD = 0.5;
}
