// STEP 4 — baked noise, as the sheet's step 10: the cracks read from a 3D texture (noise/,
// tools/bake_waterfall_noise.py) instead of worked out for every pixel.
//   edge_distance   the distance to the nearest cell border (two passes over 27 cells)   noise_b blue
//   fbm3            Perlin noise, 3 sizes (the break-up)                                 noise_b red
shader_type spatial;
render_mode unshaded;

// NEW: the noises, baked by tools/bake_waterfall_noise.py into two 3D textures (noise/) that
// tile every NOISE_PERIOD noise units; set once for the project (Project Settings > Shader
// Globals), so no material needs them
global uniform sampler3D waterfall_noise_b : filter_linear, repeat_enable;  // NEW: Perlin fBMs, border distance
const float NOISE_PERIOD = 6.0;  // NEW: noise units the textures cover before they repeat
const float border_bias = 0.016;  // NEW: the blending's round-off at a border, taken back off

uniform vec3 lit_color = vec3(0.016, 0.037, 0.041);  // linear, Blender's
uniform vec3 shade_color = vec3(0.0086, 0.0206, 0.0238);
uniform float value = 1.3;  // the last brightening (Blender's Hue / Saturation / Value)
uniform vec3 sun_direction = vec3(-0.777, 0.602, 0.370);  // toward the sun, Godot's axes
uniform float lit_cut = 0.53;  // facing the sun more than this (cos): lit
uniform vec3 crack_color = vec3(0.0497, 0.0832, 0.1001);  // step 3: lighter, bluer
uniform float cells = 3.86;  // step 3: cells across a stone

varying vec3 world_normal;
varying vec3 generated;  // Blender's Generated: the stone's own position, 0..1 over it

// voronoi "distance to edge", baked: noise_b's blue, stored x 2 (it took two passes over 27 cells).
// It falls to 0 in a sharp V on a border; texels 1/16 of a cell apart, blended, round the V off
// (up to about 0.03 above 0 between them), so the thin cracks broke into dots: border_bias
// takes that back off.
float edge_distance(vec3 p) {
	return max(texture(waterfall_noise_b, p / NOISE_PERIOD).b * 0.5 - border_bias, 0.0);  // NEW
}

// gradient (Perlin) noise in 3 sizes (fBM), 0..1 around 0.5, baked: noise_b's red, stored 2 x as
// far from 0.5 (it stays between 0.25 and 0.75).
float fbm3(vec3 p) {
	return 0.5 + (texture(waterfall_noise_b, p / NOISE_PERIOD).r - 0.5) * 0.5;  // NEW
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
