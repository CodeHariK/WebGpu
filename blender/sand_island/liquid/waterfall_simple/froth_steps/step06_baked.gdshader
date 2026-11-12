// STEP 6 — baked noise, as the sheet's step 10: the lumps, ridges and churn read the Perlin fBM
// from a 3D texture (noise_b red, noise/, tools/bake_waterfall_noise.py) instead of working out
// 3 sizes of noise from 8 corners each, in the vertex shader and for every pixel.
shader_type spatial;
render_mode unshaded, cull_disabled;

// NEW: the noises, baked by tools/bake_waterfall_noise.py into two 3D textures (noise/) that
// tile every NOISE_PERIOD noise units; set once for the project (Project Settings > Shader
// Globals), so no material needs them
global uniform sampler3D waterfall_noise_b : filter_linear, repeat_enable;  // NEW: Perlin fBMs, border distance
const float NOISE_PERIOD = 6.0;  // NEW: noise units the textures cover before they repeat

uniform vec3 foam_color = vec3(2.74, 2.6, 2.75);  // linear, past white
uniform vec3 under_color = vec3(0.038, 0.471, 0.66);
uniform float tube_length = 10.0;  // froth_tube.gd's length
uniform float tube_radius = 1.6;  // and radius
uniform float lump_strength = 8.0;  // step 3: how far the lumps push (metres per noise unit)
uniform float lump_size = 3.0;  // step 3: metres a lump (noise scale)
uniform float lump_fall = 2.7;  // step 3: metres a second the lumps sink
uniform float ridge_strength = 0.15;  // step 4: how far the ridges push
uniform vec3 sun_direction = vec3(-0.777, 0.602, 0.370);  // step 5: toward the sun (Blender's sun, Godot's axes)
uniform float shade = 0.45;  // step 5: the side away from the sun, x the colour
uniform float rim_height = 0.45;  // step 5: metres above the pool that get the dark rim
uniform vec3 rim_color = vec3(0.02, 0.25, 0.4);  // step 5: where the froth meets the water

// gradient (Perlin) noise in 3 sizes (fBM), 0..1 around 0.5, baked: roughness 0.5 is noise_b's
// red, 0.375 its green; both stored 2 x as far from 0.5 (they stay between 0.25 and 0.75).
float fbm3(vec3 p, float roughness) {
	vec4 n = texture(waterfall_noise_b, p / NOISE_PERIOD);  // NEW
	float stored = roughness > 0.45 ? n.r : n.g;  // NEW
	return 0.5 + (stored - 0.5) * 0.5;  // NEW
}

varying vec3 b;
varying vec3 lumps_only;  // the surface with its lumps but not its ridges, in view space (for the light)
varying float above_water;  // the pushed vertex's height over the pool (the tube's centre is at the pool's level)  // the tube's box, 0..1, Blender's order: along, depth, height

void vertex() {
	b = vec3(VERTEX.x / tube_length + 0.5, 0.5 - VERTEX.z / (2.0 * tube_radius), 0.5 + VERTEX.y / (2.0 * tube_radius));
	// lumps: push each vertex out (or in) along its normal by a big noise that sinks
	// through the tube; the colour stays on the smooth tube's b, so each lump carries its white
	float lump = fbm3((VERTEX + vec3(0.0, lump_fall * TIME, 0.0)) / lump_size, 0.5);
	VERTEX += NORMAL * (lump - 0.5) * lump_strength;
	lumps_only = (MODELVIEW_MATRIX * vec4(VERTEX, 1.0)).xyz;
	// ridges: Blender's Wood texture (band noise): bands across the tube, bent hard by a
	// fine noise (turbulence 15), pushing the surface into small creases; still
	float bands = (VERTEX.x + VERTEX.y + VERTEX.z) * 10.0 + 15.3 * fbm3(VERTEX / 0.29, 0.5);
	VERTEX += NORMAL * (0.5 - 0.5 * cos(bands)) * ridge_strength;  // rounded bands, 0..1
	above_water = VERTEX.y;
}

void fragment() {
	float line = -0.087 * b.x + 0.996 * b.z - 0.47;  // Blender: the box turned 95 deg, moved 0.47
	// the line pulled 31 % toward a noise drifting up through 3D (squeezed along the tube)
	float churn = fbm3((b * vec3(5.1, 2.5, 1.0) + vec3(0.0, 0.0, 0.3675 * TIME)) * 5.0, 0.5);
	line = mix(line, churn, 0.314);
	vec3 colour = mix(under_color, foam_color, step(0.241, line));  // (step 1 - 4's colour)

	// the pushed surface's real slope: how its position changes from this pixel to the next,
	// across and down the screen; the cross of those two faces out of the surface (in view space)
	vec3 facing = normalize(cross(dFdx(lumps_only), dFdy(lumps_only)));
	vec3 to_sun = normalize((VIEW_MATRIX * vec4(sun_direction, 0.0)).xyz);  // the sun, in view space too
	float lit = smoothstep(-0.05, 0.05, dot(facing, to_sun));  // lit or not, a crisp edge
	colour *= mix(shade, 1.0, lit);
	// a dark rim along the water line
	colour = mix(colour, rim_color, 1.0 - smoothstep(rim_height * 0.8, rim_height, above_water));
	ALBEDO = colour;  // white above the line
}
