// STEP 9 — the paint is BAKED: Rock.blend's own material, two numbers, two colours.
// Steps 5 and 6 made washes and strokes with noise in the shader. Blender can afford much richer
// paint: RockMat mixes #8C8C8C → #FFFFFF by an edge mask (two inside-AO nodes) and multiplies a
// pattern on top (Voronoi cells × tiny dots, noise, waves). Those two numbers are baked once into
// rock_rg.png (1024²): R = the edge mask, G = the pattern multiplier, and the colour is rebuilt here
// as mix(paint_dark, paint_light, R) × G, exactly as in Blender. Change the two colours and the same
// texture becomes another kind of rock. R already holds the edges, so step 7's highlight is dropped.
// Light, warm tops, moss and the dark foot stay in the shader: they depend on how a rock is placed.
shader_type spatial;

#include "res://shader_lib/hash.gdshaderinc"
#include "res://shader_lib/texture_noise.gdshaderinc"

uniform sampler2D rock_rg : hint_default_black, filter_linear_mipmap;  // NEW: rock_rg.png (data): R edges, G pattern
uniform vec3 paint_dark : source_color = vec3(0.549, 0.549, 0.549);  // NEW: #8C8C8C, where R = 0
uniform vec3 paint_light : source_color = vec3(1.0);                 // NEW: #FFFFFF, where R = 1
uniform float facet_grid = 24.0;      // 24 for flat-shaded rocks, ~5 for smooth meshes
uniform float facet_value = 0.16;
uniform float facet_hue = 0.1;
uniform vec3 top_tint : source_color = vec3(1.0, 0.97, 0.88);
uniform vec3 under_tint : source_color = vec3(0.78, 0.74, 0.9);
uniform vec3 edge_colour : source_color = vec3(1.0, 0.95, 0.84);  // warm highlight
uniform float edge_strength : hint_range(0.0, 1.0) = 0.55;
uniform vec3 moss_colour : source_color = vec3(0.47, 0.64, 0.3);
uniform vec3 moss_light : source_color = vec3(0.62, 0.76, 0.36);
uniform float moss_amount : hint_range(0.0, 1.0) = 1.0;            // 0 bare rock … 1 mossy tops
uniform float ground_y = 0.0;                                      // world height of the ground
uniform float foot_height = 0.6;                                   // metres of darkening above it
uniform vec3 foot_tint : source_color = vec3(0.62, 0.66, 0.58);    // × colour right at the ground
uniform vec3 lit_colour : source_color = vec3(1.0, 0.95, 0.85);   // warm sun
uniform vec3 mid_colour : source_color = vec3(0.8, 0.8, 0.88);    // cool half-tone
uniform vec3 shadow_tint : source_color = vec3(0.6, 0.58, 0.8);
uniform float shadow_light : hint_range(0.0, 1.0) = 0.35;
uniform float band_mid = 0.05;          // N·L where shadow turns to half-tone
uniform float band_lit = 0.45;          // N·L where half-tone turns to lit
uniform float band_softness = 0.05;

varying vec3 world_pos;
varying vec3 world_normal;

void vertex() {
	world_pos = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;
	world_normal = normalize((MODEL_MATRIX * vec4(NORMAL, 0.0)).xyz);
}

void fragment() {
	float up = world_normal.y;

	vec3 facet_id = floor(world_normal * facet_grid + 0.5) + floor(NODE_POSITION_WORLD * 3.0) * 7.0;
	float r1 = hash(facet_id);
	float r2 = hash(facet_id + 11.0);
	vec2 rg = texture(rock_rg, UV).rg;                                            // NEW: R edges, G pattern
	float paint = rg.g;
	vec3 colour = mix(paint_dark, paint_light, rg.r) * rg.g * (1.0 + (r1 - 0.5) * 2.0 * facet_value);  // NEW
	colour *= 1.0 + vec3(1.0, 0.0, -1.0) * facet_hue * (r2 - 0.5) * 2.0;

	colour *= mix(vec3(1.0), top_tint * 1.15, smoothstep(0.3, 0.8, up));
	colour *= mix(vec3(1.0), under_tint, smoothstep(-0.1, -0.6, up));

	// moss on upward facets, ragged edge, two greens
	float ragged = (tex_noise2(world_pos.xz * 1.6 + 30.0 * NOISE_LAYER) - 0.5) * 0.5;
	float moss = smoothstep(0.5, 0.56, (smoothstep(0.55, 0.9, up) + ragged) * moss_amount);  // 0 = bare rock
	vec3 moss_col = mix(moss_colour, moss_light, paint) * (1.0 + (r1 - 0.5) * 0.2);
	colour = mix(colour, moss_col, moss);

	// darker, greener foot (fake contact shadow)
	colour *= mix(foot_tint, vec3(1.0), smoothstep(0.0, foot_height, world_pos.y - ground_y));

	ALBEDO = colour;
	ROUGHNESS = 1.0;
	SPECULAR = 0.0;
}

void light() {
	float ndl = dot(NORMAL, LIGHT) * ATTENUATION;
	float mid = smoothstep(band_mid - band_softness, band_mid + band_softness, ndl);
	float lit = smoothstep(band_lit - band_softness, band_lit + band_softness, ndl);
	vec3 light_col = mix(shadow_tint * shadow_light, mid_colour * 0.7, mid);          // shadow → half-tone
	light_col = mix(light_col, lit_colour, lit);                                       // → lit
	DIFFUSE_LIGHT += LIGHT_COLOR / PI * light_col;
}
