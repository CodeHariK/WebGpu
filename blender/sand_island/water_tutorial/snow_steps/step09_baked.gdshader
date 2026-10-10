// STEP 9 — the same snow, baked: 3 texture reads instead of 14.
// Step 8 read the noise texture 14 times per pixel: 4 tex_noise2() (rock tone, strata, ragged
// edge, blue patches), ripple() for the drifts run 3 times to get their slope (6 more), and 4
// tex_hash() per sparkle grain (mirror? + 3 tilt numbers). snow_texture/snow_texture.blend + bake_snow_texture.py bake it all:
//   snow_drifts.png  (20 m tile)  R = drift height · G, B = drift slope in x, z
//   snow_macro.png   (80 m tile)  R = tone · G = strata bend · B = ragged edge · A = blue patches
//   snow_sparkle.png (128², one texel per grain)  R, G, B = mirror tilt · A = is it a mirror?
// A channels are stored as 0.5 + 0.5·v so the PNGs are never transparent; a · 2 − 1 undoes it.
// Height, slope, snow line and the light() maths stay computed: they depend on THIS island.
shader_type spatial;

uniform sampler2D snow_drifts : filter_linear_mipmap, repeat_enable;  // NEW: snow_texture/snow_drifts.png
uniform sampler2D snow_macro : filter_linear_mipmap, repeat_enable;   // NEW: snow_texture/snow_macro.png
uniform sampler2D snow_sparkle : filter_nearest, repeat_enable;       // NEW: snow_texture/snow_sparkle.png
uniform float drift_tile = 20.0;   // NEW: metres per repeat of snow_drifts (the bake's DRIFT_TILE)
uniform float macro_tile = 80.0;   // NEW: metres per repeat of snow_macro (the bake's MACRO_TILE)
uniform float slope_range = 4.0;   // NEW: G, B store slope / (2 × this) + 0.5 (the bake's S)

uniform float water_level = 0.0;
uniform float snow_line = 0.18;
uniform float slope_min = 0.55;
uniform float slope_full = 0.78;
uniform float edge_noise = 0.3;
uniform float snow_everywhere : hint_range(0.0, 1.0) = 0.0;
uniform vec3 snow_colour : source_color = vec3(0.86, 0.88, 0.92);
uniform vec3 snow_cool : source_color = vec3(0.74, 0.8, 0.93);
uniform vec3 snow_edge : source_color = vec3(0.62, 0.72, 0.94);
uniform vec3 rock : source_color = vec3(0.46, 0.49, 0.58);
uniform vec3 rock_light : source_color = vec3(0.62, 0.64, 0.72);
uniform vec3 rock_wet : source_color = vec3(0.28, 0.31, 0.4);
uniform vec3 seabed : source_color = vec3(0.4, 0.55, 0.62);
uniform float drift_strength = 2.5;    // how much drifts tilt the light
uniform float drift_shade = 0.035;     // how much drifts darken/lighten the colour
uniform float sparkle_density = 0.12;  // fraction of grains that are mirrors
uniform float sparkle_size = 24.0;     // grains per metre
uniform float sparkle_strength = 1.5;
uniform float sparkle_fade = 18.0;     // metres; sparkle fades out beyond this
uniform vec3 shadow_colour : source_color = vec3(0.52, 0.64, 0.92);
uniform float shadow_light : hint_range(0.0, 1.0) = 0.6;
uniform float snow_wrap : hint_range(0.0, 1.0) = 0.35;
uniform float snow_soft : hint_range(0.05, 1.0) = 0.7;
uniform vec3 rock_shadow : source_color = vec3(0.7, 0.68, 0.9);
uniform float rock_shadow_light : hint_range(0.0, 1.0) = 0.35;
uniform float band_lit = 0.25;
uniform float band_softness = 0.12;

varying vec3 world_pos;
varying vec3 world_normal;
varying float snow;
varying float glint;  // fragment → light, this pixel is a mirror grain
varying vec3 facet;   // the mirror's normal, in view space

void vertex() {
	world_pos = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;
	world_normal = normalize((MODEL_MATRIX * vec4(NORMAL, 0.0)).xyz);
}

void fragment() {
	float h = world_pos.y - water_level;
	float up = world_normal.y;
	vec2 p = world_pos.xz;

	vec4 macro = texture(snow_macro, p / macro_tile);   // NEW: ONE read: tone, strata, ragged, patches
	float tone = macro.r;                               // NEW: was tex_noise2(p * 0.4)
	float strata = smoothstep(0.55, 0.8, sin(world_pos.y * 7.0 + macro.g * 3.0) * 0.5 + 0.5);  // NEW: macro.g
	vec3 rock_col = mix(rock, rock_light, tone * 0.6 + strata * 0.4);
	rock_col = mix(rock_wet, rock_col, smoothstep(0.0, 0.14, h));
	rock_col = mix(rock_col, seabed, smoothstep(0.0, -0.8, h));

	float ragged = (macro.b - 0.5) * edge_noise;        // NEW: was tex_noise2(p * 1.3)
	float by_slope = smoothstep(slope_min, slope_full, up + ragged);
	float by_height = smoothstep(snow_line, snow_line + 0.12, h + ragged * 0.5);
	float cover = max(by_slope * by_height, snow_everywhere);
	float s = smoothstep(0.45, 0.55, cover);
	float edge = smoothstep(0.25, 0.45, cover) * (1.0 - s);

	float patches = macro.a * 2.0 - 1.0;                // NEW: was tex_noise2(p * 0.3)
	vec3 snow_col = mix(snow_colour, snow_cool, smoothstep(0.45, 0.85, patches) * 0.7);
	vec3 drift = texture(snow_drifts, p / drift_tile).rgb;      // NEW: ONE read: height + slope
	float d = drift.r;                                          // 0 trough … 1 crest
	snow_col *= 1.0 - drift_shade + drift_shade * 2.0 * d;       // troughs a touch darker

	vec3 colour = mix(rock_col, snow_edge, edge);
	colour = mix(colour, snow_col, s);

	// drifts tilt the normal on flat snow only. NEW: the slope is baked, no extra ripple() runs
	float flat_snow = s * smoothstep(0.85, 0.97, up);
	vec2 slope = (drift.gb * 2.0 - 1.0) * slope_range;
	vec3 n = normalize(world_normal - vec3(slope.x, 0.0, slope.y) * 0.02 * drift_strength * flat_snow);
	NORMAL = normalize((VIEW_MATRIX * vec4(n, 0.0)).xyz);

	// sparkle. Pick grains, give each a random mirror tilt (light() decides if it flashes)
	// NEW: one texelFetch gives all 4 random numbers of this grain (was 4 tex_hash reads)
	vec3 cell = floor(world_pos * sparkle_size);
	ivec2 texel = ivec2(mod(cell.xz + cell.y * vec2(31.0, 17.0), 128.0));
	vec4 grain = texelFetch(snow_sparkle, texel, 0);
	float is_mirror = step(1.0 - sparkle_density, grain.a * 2.0 - 1.0);
	vec3 tilt = grain.rgb - 0.5;
	facet = normalize((VIEW_MATRIX * vec4(normalize(n + tilt * 0.9), 0.0)).xyz);
	float fade = 1.0 - smoothstep(sparkle_fade * 0.5, sparkle_fade, length(world_pos - CAMERA_POSITION_WORLD));
	glint = is_mirror * s * fade;

	snow = s;
	ALBEDO = colour;
	ROUGHNESS = 1.0;
	SPECULAR = 0.0;
}

void light() {
	float ndl = dot(NORMAL, LIGHT);
	float rock_lit = smoothstep(band_lit - band_softness, band_lit + band_softness, ndl * ATTENUATION);
	vec3 rock_light_col = mix(rock_shadow * rock_shadow_light, vec3(1.0), rock_lit);

	float wrapped = (ndl + snow_wrap) / (1.0 + snow_wrap);
	float snow_lit = smoothstep(0.0, snow_soft, wrapped) * ATTENUATION;
	vec3 snow_light_col = mix(shadow_colour * shadow_light, vec3(1.0), snow_lit);

	DIFFUSE_LIGHT += LIGHT_COLOR / PI * mix(rock_light_col, snow_light_col, snow);

	// the mirror flashes when it bounces the sun into the eye (half-vector test)
	vec3 half_dir = normalize(LIGHT + VIEW);
	float flash = smoothstep(0.97, 0.99, dot(facet, half_dir));  // crisp cartoon dot
	DIFFUSE_LIGHT += LIGHT_COLOR * flash * glint * snow_lit * sparkle_strength;
}
