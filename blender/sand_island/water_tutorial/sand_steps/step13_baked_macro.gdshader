// STEP 13 — the big soft noises baked too: ONE macro texture read instead of four noise2().
// Step 12 still called noise2() four times per pixel: sand tone, the wobble of the wet line, the
// grass patches and the bend of the bank stripes. They are blobs 1–4 m wide, too big for the 4 m
// ripple tile (they would repeat in a visible grid), so they get their own tile: 80 m, 512².
// sand_texture.blend's MacroTile makes them as Blender noise (tileable, round blobs; tweak the
// SandMacro node group) and bake_sand_macro.py packs them as
//   R = tone · G = wet wobble · B = grass patches · A = stripe bend (stored as 0.5 + 0.5·v so
//   the PNG is never transparent; the shader undoes that with a·2 − 1)
// The shader now uses only 2 texture reads in total (ripples + macro) and no noise texture,
// hash() or noise2() at all. Everything that depends on THIS island (height, slope, wave) stays
// computed.
shader_type spatial;
render_mode specular_disabled;

group_uniforms heights;
uniform float water_level = 0.0;
uniform float grass_height = 1.45;     // above water
uniform float wet_height = 0.07;       // wet band reaches this high (plus the wave)
uniform float wave_reach = 0.05;       // how far the wet band moves with the waves
uniform float wave_speed = 0.9;

group_uniforms colours;
uniform vec3 sand_dry : source_color = vec3(0.98, 0.86, 0.62);
uniform vec3 sand_light : source_color = vec3(1.0, 0.92, 0.72);
uniform vec3 sand_wet : source_color = vec3(0.92, 0.76, 0.54);
uniform vec3 seabed : source_color = vec3(0.62, 0.78, 0.7);
uniform vec3 grass : source_color = vec3(0.42, 0.74, 0.3);
uniform vec3 grass_dark : source_color = vec3(0.32, 0.62, 0.24);
uniform vec3 bank : source_color = vec3(0.92, 0.55, 0.3);
uniform vec3 bank_stripe : source_color = vec3(0.98, 0.7, 0.42);

group_uniforms detail;
uniform float ripple_spacing = 0.55;   // metres between ripple lines
uniform float ripple_strength = 0.35;  // how much ripples tilt the light
uniform float ripple_shade = 0.05;     // how much ripples darken/lighten the colour

group_uniforms lighting;
uniform vec3 shadow_tint : source_color = vec3(0.7, 0.68, 0.9);
uniform float shadow_light : hint_range(0.0, 1.0) = 0.35;
uniform float band_lit = 0.25;
uniform float band_softness = 0.12;

varying vec3 world_pos;
varying vec3 world_normal;

uniform sampler2D sand_tex : filter_linear_mipmap, repeat_enable;    // sand_texture/sand_ripples.png (data)
uniform float sand_tile = 4.0;     // metres per repeat of the ripple tile
uniform float slope_range = 8.0;   // G, B store slope / (2 × this) + 0.5 (the bake's S)
uniform sampler2D sand_macro : filter_linear_mipmap, repeat_enable;  // NEW: sand_texture/sand_macro.png (data)
uniform float macro_tile = 80.0;   // NEW: metres per repeat of the macro tile (the bake's TILE)

void vertex() {
	world_pos = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;
	world_normal = normalize((MODEL_MATRIX * vec4(NORMAL, 0.0)).xyz);
}

void fragment() {
	float h = world_pos.y - water_level;
	float up = world_normal.y;
	vec2 p = world_pos.xz;

	vec4 macro = texture(sand_macro, p / macro_tile);    // NEW: ONE read: tone, wet, patches, stripes

	// --- sand
	float tone = macro.r;                                // NEW: was noise2(p * 0.25)
	vec3 sand = mix(sand_dry, sand_light, smoothstep(0.35, 0.75, tone));
	vec4 baked = texture(sand_tex, p / sand_tile);       // ripple height, slope, grain (step 12)
	float r = baked.r;
	sand *= 1.0 - ripple_shade + ripple_shade * 2.0 * r;
	float grain = step(baked.a, 0.75);                    // A: 1 none, 0.5 dark, 0.25 light speck
	sand = mix(sand, sand * (baked.a < 0.375 ? 1.08 : 0.88), grain);

	float wave = sin(TIME * wave_speed + p.x * 0.15) * 0.5 + 0.5;
	float wet_top = wet_height + wave * wave_reach + (macro.g - 0.5) * 0.04;  // NEW: was noise2(p * 0.8)
	float wet = 1.0 - smoothstep(wet_top - 0.04, wet_top, h);
	sand = mix(sand, sand_wet * (0.95 + 0.1 * r), wet);
	sand = mix(sand, seabed, smoothstep(0.0, -0.8, h));

	// --- grass and bank
	float patches = smoothstep(0.45, 0.6, macro.b);  // NEW: was noise2(p * 0.5)
	vec3 lawn = mix(grass, grass_dark, patches * 0.7);
	float stripes = smoothstep(0.55, 0.75, sin(world_pos.y * 9.0 + (macro.a * 2.0 - 1.0) * 3.0) * 0.5 + 0.5);  // NEW: was noise2(p * 0.6)
	vec3 earth = mix(bank, bank_stripe, stripes * 0.6);

	float is_grass = smoothstep(grass_height - 0.08, grass_height + 0.02, h) * smoothstep(0.6, 0.8, up);
	float is_bank = (1.0 - smoothstep(0.65, 0.85, up)) * smoothstep(wet_height + 0.1, wet_height + 0.25, h);
	vec3 colour = mix(sand, earth, is_bank);
	colour = mix(colour, lawn, is_grass);

	// ripples tilt the normal on flat sand only
	float flat_sand = (1.0 - is_grass) * (1.0 - is_bank) * smoothstep(0.85, 0.97, up);
	vec2 slope = (baked.gb * 2.0 - 1.0) * slope_range;   // the baked slope (step 12)
	vec3 n = normalize(world_normal - vec3(slope.x, 0.0, slope.y) * 0.02 * ripple_strength * flat_sand);
	NORMAL = normalize((VIEW_MATRIX * vec4(n, 0.0)).xyz);

	ALBEDO = colour;
	ROUGHNESS = 1.0;
}

void light() {
	float ndl = dot(NORMAL, LIGHT) * ATTENUATION;
	float lit = smoothstep(band_lit - band_softness, band_lit + band_softness, ndl);
	DIFFUSE_LIGHT += LIGHT_COLOR / PI * mix(shadow_tint * shadow_light, vec3(1.0), lit);
}
