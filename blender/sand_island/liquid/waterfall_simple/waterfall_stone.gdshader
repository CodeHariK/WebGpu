// The stones round the waterfall, simple: the stylized waterfall's "rocks" material, written by
// hand, on stone_mesh.gd's stones (scattered by stone_scatter.gd):
//   tones    lit or shaded, one cut on how much the stone faces the sun: two dark teal-greys
//   cracks   a voronoi's cell borders, broken up by noise: thin lighter lines
// Built step by step in stone_steps/ (the last step is this file).
shader_type spatial;
render_mode unshaded;

uniform vec3 lit_color = vec3(0.016, 0.037, 0.041);  // linear, Blender's
uniform vec3 shade_color = vec3(0.0086, 0.0206, 0.0238);
uniform float value = 1.3;  // the last brightening (Blender's Hue / Saturation / Value)
uniform vec3 sun_direction = vec3(-0.777, 0.602, 0.370);  // toward the sun, Godot's axes
uniform float lit_cut = 0.53;  // facing the sun more than this (cos): lit
uniform vec3 crack_color = vec3(0.0497, 0.0832, 0.1001);  // step 3: lighter, bluer
uniform float cells = 3.86;  // step 3: cells across a stone

varying vec3 world_normal;
varying vec3 generated;  // Blender's Generated: the stone's own position, 0..1 over it

// a random point in each cell of a 3D grid (Dave Hoskins' hash, no sin).
vec3 hash33(vec3 p) {
	p = fract(p * vec3(0.1031, 0.1030, 0.0973));
	p += dot(p, p.yxz + 33.33);
	return fract((p.xxy + p.yxx) * p.zyx);
}

// voronoi "distance to edge": how far to the border between the nearest cell and the next
// (0 on a border). First find the nearest point; then, against every neighbour, the distance to
// the plane half way between the two (that plane is their border).
float edge_distance(vec3 p) {
	vec3 cell = floor(p);
	vec3 local_p = p - cell;
	vec3 to_nearest = vec3(0.0);
	float nearest = 8.0;
	for (int k = -1; k <= 1; k++) {
		for (int j = -1; j <= 1; j++) {
			for (int i = -1; i <= 1; i++) {
				vec3 offset = vec3(float(i), float(j), float(k));
				vec3 to_point = offset + hash33(cell + offset) - local_p;
				float d = dot(to_point, to_point);
				if (d < nearest) {
					nearest = d;
					to_nearest = to_point;
				}
			}
		}
	}
	float border = 8.0;
	for (int k = -1; k <= 1; k++) {
		for (int j = -1; j <= 1; j++) {
			for (int i = -1; i <= 1; i++) {
				vec3 offset = vec3(float(i), float(j), float(k));
				vec3 to_point = offset + hash33(cell + offset) - local_p;
				vec3 across = to_point - to_nearest;
				if (dot(across, across) > 0.0001) {
					border = min(border, dot((to_nearest + to_point) * 0.5, normalize(across)));
				}
			}
		}
	}
	return border;
}

// gradient (Perlin) noise in 3D, about -1..1, and 3 sizes of it (fBM), 0..1 around 0.5.
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

float fbm3(vec3 p) {
	return 0.5 + 0.5 * (gnoise3(p) + gnoise3(p * 2.0) * 0.5 + gnoise3(p * 4.0) * 0.25) / 1.75;
}

void vertex() {
	world_normal = normalize(MODEL_NORMAL_MATRIX * NORMAL);
	generated = VERTEX * 0.5 + 0.5;  // the stone is built at radius about 1
}

void fragment() {
	float facing = dot(normalize(world_normal), normalize(sun_direction));  // 1 facing the sun
	vec3 colour = facing >= lit_cut ? lit_color : shade_color;
	// cracks: the cells' borders (plus a noise, so they break up), lifted (power 0.56) and
	// cut near 0: on a border the crack colour
	float border = edge_distance((generated + vec3(0.61, 0.0, 0.0)) * cells);
	float breakup = clamp((fbm3(generated * 5.0) - 0.5) / 0.5, 0.0, 1.0);
	float crack = step(pow(border + breakup, 0.56), 0.1045);  // 1 on a crack
	colour = mix(colour, crack_color, crack);
	ALBEDO = colour * value;
}
