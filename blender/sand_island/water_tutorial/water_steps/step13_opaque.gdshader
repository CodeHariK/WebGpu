// STEP 13 — opaque water: fake the see-through instead of blending.
// Since step 4, blend_mix made the water transparent, and that was the most expensive line of the
// whole shader (+1 ms): a transparent surface is drawn in a separate pass after everything else,
// writes no depth, and every pixel of sea bed UNDER it must be fully shaded first, then blended.
// But our water is 55% opaque at the shore and 95% opaque out deep, so we hardly see the bed.
// Now the water is solid (no blend_mix) and mixes in a "bed" colour itself, by the same ALPHA
// amount as before: shallow wet sand → greenish sea bed, like the sand shader paints under water
// (depth 0.4 = 0.8 m = where the sand shader reaches its seabed colour). The GPU's depth test then
// skips every bed pixel under the water before it is shaded.
// Trade-off: you no longer see the real bed (its ripples and shape) through shallow water; the foam
// line at the shore hides the hard edge where water meets sand.
shader_type spatial;
render_mode unshaded, cull_back;  // NEW: no blend_mix = opaque, drawn with the solid objects

uniform vec3 shore_colour : source_color = vec3(0.6, 0.93, 0.9);
uniform vec3 shallow_colour : source_color = vec3(0.25, 0.75, 0.88);
uniform vec3 deep_colour : source_color = vec3(0.12, 0.42, 0.85);
uniform vec3 bed_shallow : source_color = vec3(0.88, 0.8, 0.6);   // NEW: wet sand just under the water
uniform vec3 bed_deep : source_color = vec3(0.5, 0.68, 0.64);     // NEW: sea bed further out
uniform float shallow_end = 0.18;
uniform float deep_start = 0.45;
uniform vec3 foam_colour : source_color = vec3(1.0);
uniform float foam_width = 0.012;
uniform float wash = 0.015;
uniform float wave_speed = 0.9;
uniform float line_speed = 0.12;
uniform float line_count = 3.0;
uniform float sparkle_density = 0.004;
uniform float ripple_height = 0.03;

varying float depth;
varying vec3 world_pos;

uniform sampler2D noise_tex : filter_linear, repeat_enable;  // textures/noise_128.png: now only for the sparkles
const float NOISE_SIZE = 128.0;
uniform sampler2D water_macro : filter_linear_mipmap, repeat_enable;  // water_texture/water_macro.png
uniform float macro_tile = 40.0;                     // metres per repeat (the bake's TILE)
uniform vec2 macro_drift = vec2(0.004, 0.0025);      // tiles per second the noise drifts

// hash: one random number per grid cell, read straight from one texel (texelFetch = no
// filtering). Replaces ~10 multiplies; mainly here so ALL randomness comes from the texture.
float hash(vec3 cell) {
	ivec2 texel = ivec2(mod(cell.xz + cell.y * vec2(31.0, 17.0), NOISE_SIZE));
	return texelFetch(noise_tex, texel, 0).r;
}

void vertex() {
	depth = COLOR.r;
	world_pos = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;
	float swell = sin(TIME * 1.3 + world_pos.x * 0.6) * cos(TIME * 1.1 + world_pos.z * 0.5);
	VERTEX.y += swell * ripple_height * smoothstep(0.05, 0.3, depth);
}

void fragment() {
	vec2 p = world_pos.xz;
	vec4 macro = texture(water_macro, p / macro_tile + TIME * macro_drift);  // ONE read, 4 noises (step 12)
	float wobble_noise = mix(macro.r, macro.g, sin(TIME * 0.6) * 0.5 + 0.5);  // cross-fade 2 versions
	float wobble = (wobble_noise - 0.5) * 0.08;
	float d = depth + wobble;
	vec3 colour = mix(shore_colour, shallow_colour, smoothstep(0.02, shallow_end, d));
	colour = mix(colour, deep_colour, smoothstep(deep_start - 0.08, deep_start + 0.08, d));
	float wave = sin(TIME * wave_speed + p.x * 0.15) * 0.5 + 0.5;
	float edge = foam_width + wave * wash;
	float foam = 1.0 - smoothstep(edge - 0.004, edge, depth + wobble * 0.15);
	float lines = fract(d * line_count / deep_start - TIME * line_speed);
	float line = smoothstep(0.0, 0.03, lines) * (1.0 - smoothstep(0.05, 0.09, lines));
	float line_noise = mix(macro.b, macro.a * 2.0 - 1.0, sin(TIME * 0.9 + 1.0) * 0.5 + 0.5);
	line *= step(0.45, line_noise) * (1.0 - smoothstep(0.1, deep_start, d));
	foam = max(foam, line * 0.8);

	vec3 cell = floor(vec3((p + TIME * vec2(0.3, 0.2)) * 9.0, floor(TIME * 2.0)));
	float sparkle = step(1.0 - sparkle_density, hash(cell)) * smoothstep(0.15, 0.4, d);

	// NEW: what used to be ALPHA now decides how much of the (faked) bed shows through
	float opacity = mix(0.55, 0.95, smoothstep(0.0, 0.35, depth));
	vec3 bed = mix(bed_shallow, bed_deep, smoothstep(0.0, 0.4, depth));
	colour = mix(bed, colour, opacity);
	ALBEDO = mix(colour, foam_colour, max(foam, sparkle));  // no ALPHA: solid
}
