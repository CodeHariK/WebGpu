// STEP 3 — boil: the tube swells into lumps that keep rolling down.
// The vertex shader moves the mesh: each vertex is pushed along its normal by a big noise (a lump
// about 3 m across, pushing up to about a metre), out where the noise is over 0.5 and in where it
// is under. The noise is read at the vertex's position plus a height that grows with time, so the
// lumps sink 2.7 m a second, as Blender's moving empty makes them (its Displace follows an empty
// sliding down). b is worked out before the push: the white / blue line stays where it was on the
// smooth tube, so a lump keeps its colours as it bulges. No normals are rebuilt: the colours don't
// use light.
shader_type spatial;
render_mode unshaded, cull_disabled;

uniform vec3 foam_color = vec3(2.74, 2.6, 2.75);  // linear, past white
uniform vec3 under_color = vec3(0.038, 0.471, 0.66);
uniform float tube_length = 10.0;  // froth_tube.gd's length
uniform float tube_radius = 1.6;  // and radius
uniform float lump_strength = 8.0;  // NEW: step 3: how far the lumps push (metres per noise unit)
uniform float lump_size = 3.0;  // NEW: step 3: metres a lump (noise scale)
uniform float lump_fall = 2.7;  // NEW: step 3: metres a second the lumps sink

// gradient (Perlin) noise in 3D, about -1..1, like Blender's (the pool's step 2).
vec3 hash33(vec3 p) {
	p = fract(p * vec3(0.1031, 0.1030, 0.0973));
	p += dot(p, p.yxz + 33.33);
	return fract((p.xxy + p.yxx) * p.zyx);
}

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

// gnoise3 in 3 sizes (Blender's fBM), 0..1 around 0.5.
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

varying vec3 b;  // the tube's box, 0..1, Blender's order: along, depth, height

void vertex() {
	b = vec3(VERTEX.x / tube_length + 0.5, 0.5 - VERTEX.z / (2.0 * tube_radius), 0.5 + VERTEX.y / (2.0 * tube_radius));
	// NEW: lumps: push each vertex out (or in) along its normal by a big noise that sinks
	// through the tube; the colour stays on the smooth tube's b, so each lump carries its white
	float lump = fbm3((VERTEX + vec3(0.0, lump_fall * TIME, 0.0)) / lump_size, 0.5);  // NEW
	VERTEX += NORMAL * (lump - 0.5) * lump_strength;  // NEW
}

void fragment() {
	float line = -0.087 * b.x + 0.996 * b.z - 0.47;  // Blender: the box turned 95 deg, moved 0.47
	// the line pulled 31 % toward a noise drifting up through 3D (squeezed along the tube)
	float churn = fbm3((b * vec3(5.1, 2.5, 1.0) + vec3(0.0, 0.0, 0.3675 * TIME)) * 5.0, 0.5);
	line = mix(line, churn, 0.314);
	ALBEDO = mix(under_color, foam_color, step(0.241, line));  // white above the line
}
