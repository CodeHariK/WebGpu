// STEP 4 — ridges: small creases over the lumps. The finished froth.
// Blender's second Displace on the foam is a Wood texture (band noise): bands running diagonally
// (x + y + z, ten to a 2 pi), each bent hard by a fine noise (0.29 m, times 15). Blender shapes
// them as a saw tooth on a coarse mesh that it then smooths; on this fine mesh a saw tooth comes
// out as spikes, so the bands here are rounded (a cosine). Pushing the surface along its normal
// by that (up to 0.15 m) creases the lumps. They don't move; the lumps roll through them.
// This is the whole of Blender's bottom foam (waterfall_froth.gdshader).
shader_type spatial;
render_mode unshaded, cull_disabled;

uniform vec3 foam_color = vec3(2.74, 2.6, 2.75);  // linear, past white
uniform vec3 under_color = vec3(0.038, 0.471, 0.66);
uniform float tube_length = 10.0;  // froth_tube.gd's length
uniform float tube_radius = 1.6;  // and radius
uniform float lump_strength = 8.0;  // step 3: how far the lumps push (metres per noise unit)
uniform float lump_size = 3.0;  // step 3: metres a lump (noise scale)
uniform float lump_fall = 2.7;  // step 3: metres a second the lumps sink
uniform float ridge_strength = 0.15;  // NEW: step 4: how far the ridges push

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
	// lumps: push each vertex out (or in) along its normal by a big noise that sinks
	// through the tube; the colour stays on the smooth tube's b, so each lump carries its white
	float lump = fbm3((VERTEX + vec3(0.0, lump_fall * TIME, 0.0)) / lump_size, 0.5);
	VERTEX += NORMAL * (lump - 0.5) * lump_strength;
	// NEW: ridges: Blender's Wood texture (band noise): bands across the tube, bent hard by a
	// fine noise (turbulence 15), pushing the surface into small creases; still
	float bands = (VERTEX.x + VERTEX.y + VERTEX.z) * 10.0 + 15.3 * fbm3(VERTEX / 0.29, 0.5);  // NEW
	VERTEX += NORMAL * (0.5 - 0.5 * cos(bands)) * ridge_strength;  // NEW: rounded bands, 0..1
}

void fragment() {
	float line = -0.087 * b.x + 0.996 * b.z - 0.47;  // Blender: the box turned 95 deg, moved 0.47
	// the line pulled 31 % toward a noise drifting up through 3D (squeezed along the tube)
	float churn = fbm3((b * vec3(5.1, 2.5, 1.0) + vec3(0.0, 0.0, 0.3675 * TIME)) * 5.0, 0.5);
	line = mix(line, churn, 0.314);
	ALBEDO = mix(under_color, foam_color, step(0.241, line));  // white above the line
}
