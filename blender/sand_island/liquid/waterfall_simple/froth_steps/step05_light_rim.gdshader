// STEP 5 — light and a rim (beyond Blender: its foam has neither). Shapes you can read.
// Unlit, the froth is a flat white cut-out: only its outline shows the lumps. Two additions:
//   light   the vertex shader has moved the surface, so its NORMAL is the smooth tube's, not the
//           lumps'. The real slope comes from the screen: dFdx / dFdy are how the position changes
//           from one pixel to the next (across, down); crossed, they give the way the surface
//           faces right here (a flat slope per triangle). It is taken on the surface with its lumps
//           but without step 4's ridges: their fine creases would speckle the light. Facing the
//           sun (Blender's sun's direction): the colour; facing away: 45 % of it, a crisp edge
//           between (toon light, like the water track's step 3)
//   rim     a dark blue band where the froth meets the pool: the pushed vertex's height under
//           0.45 m (the tube's centre is at the pool's level)
shader_type spatial;
render_mode unshaded, cull_disabled;

uniform vec3 foam_color = vec3(2.74, 2.6, 2.75);  // linear, past white
uniform vec3 under_color = vec3(0.038, 0.471, 0.66);
uniform float tube_length = 10.0;  // froth_tube.gd's length
uniform float tube_radius = 1.6;  // and radius
uniform float lump_strength = 8.0;  // step 3: how far the lumps push (metres per noise unit)
uniform float lump_size = 3.0;  // step 3: metres a lump (noise scale)
uniform float lump_fall = 2.7;  // step 3: metres a second the lumps sink
uniform float ridge_strength = 0.15;  // step 4: how far the ridges push
uniform vec3 sun_direction = vec3(-0.777, 0.602, 0.370);  // NEW: step 5: toward the sun (Blender's sun, Godot's axes)
uniform float shade = 0.45;  // NEW: step 5: the side away from the sun, x the colour
uniform float rim_height = 0.45;  // NEW: step 5: metres above the pool that get the dark rim
uniform vec3 rim_color = vec3(0.02, 0.25, 0.4);  // NEW: step 5: where the froth meets the water

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

varying vec3 b;
varying vec3 lumps_only;  // NEW: the surface with its lumps but not its ridges, in view space (for the light)
varying float above_water;  // NEW: the pushed vertex's height over the pool (the tube's centre is at the pool's level)  // the tube's box, 0..1, Blender's order: along, depth, height

void vertex() {
	b = vec3(VERTEX.x / tube_length + 0.5, 0.5 - VERTEX.z / (2.0 * tube_radius), 0.5 + VERTEX.y / (2.0 * tube_radius));
	// lumps: push each vertex out (or in) along its normal by a big noise that sinks
	// through the tube; the colour stays on the smooth tube's b, so each lump carries its white
	float lump = fbm3((VERTEX + vec3(0.0, lump_fall * TIME, 0.0)) / lump_size, 0.5);
	VERTEX += NORMAL * (lump - 0.5) * lump_strength;
	lumps_only = (MODELVIEW_MATRIX * vec4(VERTEX, 1.0)).xyz;  // NEW
	// ridges: Blender's Wood texture (band noise): bands across the tube, bent hard by a
	// fine noise (turbulence 15), pushing the surface into small creases; still
	float bands = (VERTEX.x + VERTEX.y + VERTEX.z) * 10.0 + 15.3 * fbm3(VERTEX / 0.29, 0.5);
	VERTEX += NORMAL * (0.5 - 0.5 * cos(bands)) * ridge_strength;  // rounded bands, 0..1
	above_water = VERTEX.y;  // NEW
}

void fragment() {
	float line = -0.087 * b.x + 0.996 * b.z - 0.47;  // Blender: the box turned 95 deg, moved 0.47
	// the line pulled 31 % toward a noise drifting up through 3D (squeezed along the tube)
	float churn = fbm3((b * vec3(5.1, 2.5, 1.0) + vec3(0.0, 0.0, 0.3675 * TIME)) * 5.0, 0.5);
	line = mix(line, churn, 0.314);
	vec3 colour = mix(under_color, foam_color, step(0.241, line));  // NEW: (step 1 - 4's colour)

	// NEW: the pushed surface's real slope: how its position changes from this pixel to the next,
	// across and down the screen; the cross of those two faces out of the surface (in view space)
	vec3 facing = normalize(cross(dFdx(lumps_only), dFdy(lumps_only)));  // NEW
	vec3 to_sun = normalize((VIEW_MATRIX * vec4(sun_direction, 0.0)).xyz);  // NEW: the sun, in view space too
	float lit = smoothstep(-0.05, 0.05, dot(facing, to_sun));  // NEW: lit or not, a crisp edge
	colour *= mix(shade, 1.0, lit);  // NEW
	// NEW: a dark rim along the water line
	colour = mix(colour, rim_color, 1.0 - smoothstep(rim_height * 0.8, rim_height, above_water));  // NEW
	ALBEDO = colour;  // white above the line
}
