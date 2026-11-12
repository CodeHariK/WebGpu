// Waterfall sheet, simple: a stylized waterfall Blender scene's "waterfall" material
// (the .blend is not kept in this repo), written by hand. Blender's material is
// colour laid on by height, the edges between the colours bent by voronoi cells:
//   layer 1  two deep blues: a height line (low on the sheet) bent by cells, split in two tones
//   layer 2  a light teal over the top part: another height line, bent by other cells
//   layer 3  aqua circles (the cells' middles) in a band near the top
//   streaks  thin bright vertical lines (bent stripes' crests), shown in patches that fall
//   dark     a few thin dark vertical lines (straighter stripes' troughs) down the whole sheet
//   lip      a near-white patch over the top (its edge boiling) and stepped bright rings under it
//   tiers    four brightness tiers by height, 0.43 x at the bottom to 1.45 x at the top
//   drips    a white web (voronoi F1 minus Smooth F1: the cells' borders), shown only in a
//            couple of wandering columns
//   grade    a last hue / saturation / value push over all of it (Blender's "finalHSL" group)
//   ripples  the vertex shader wobbles the surface with a noise sliding down (Blender's Displace)
//   variety  pattern_offset (per instance) moves every pattern, so many sheets don't repeat
// The cells slide down over time: Blender moves each voronoi's Mapping up through its 3D
// texture, so the pattern falls (the material's keyframes, per second at its 21 fps).
// That is the whole material. Built step by step in steps/ (the last step is this file).
//
// Coordinates, like Blender's "Generated": the sheet's own position scaled to 0..1 over its box
// (box_min, box_size: measured from the Blender mesh when it was exported), kept in Blender's order:
// b.x across, b.y depth, b.z height (0 at the bottom, 1 at the top).
// The noises are baked: each is one read of a 3D texture (noise/, tools/bake_waterfall_noise.py).
shader_type spatial;
render_mode unshaded, cull_disabled;

#include "res://shader_lib/liquid_noise.gdshaderinc"

// the noises, baked by tools/bake_waterfall_noise.py into two 3D textures (noise/) that
// tile every NOISE_PERIOD noise units; set once for the project (Project Settings > Shader
// Globals), so no material needs them
global uniform sampler3D waterfall_noise_a : filter_linear, repeat_enable;  // F1, borders, value fBMs
const float NOISE_PERIOD = 6.0;  // noise units the textures cover before they repeat

// Colours are linear (Blender's numbers), not picked: the grade pushes them past 1.
uniform vec3 deep_color = vec3(0.007, 0.060, 0.072); // layer 1, upper tone
uniform vec3 deep_color_2 = vec3(0.031, 0.111, 0.126); // layer 1, lower tone
uniform float deep_value = 1.65; // layer 1's brightness
uniform vec3 light_color = vec3(0.087, 0.317, 0.320); // layer 2
uniform vec3 aqua_color = vec3(0.214, 0.6, 0.539); // layer 3
uniform float streak_brightness = 2.4; // step 2: the streaks, x the colour
uniform float line_darkness = 0.5;  // step 3: the dark lines, x the colour
uniform vec3 glow_color = vec3(1.2, 1.175, 1.14);  // step 4: the patch at the lip, past white
uniform float ring_brightness = 2.5;  // step 4: the rings' middle, x the colour
uniform float bottom_brightness = 0.43;  // step 5: the lowest tier, x the colour
uniform float top_brightness = 1.45;  // step 5: the highest tier
uniform float lace_edge = 0.179;  // step 6: F1 - Smooth F1 above this: lace
uniform float drip_side = 0.088;  // step 7: how white the lace is on a column's side
uniform float ripple_strength = 0.7;  // step 8: metres the surface moves per noise unit
uniform float ripple_size = 1.41;  // step 8: metres a ripple (Blender's Clouds size)
uniform float ripple_fall = 4.6;  // step 8: metres a second the ripples slide down
uniform vec3 grade = vec3(0.06, 1.7, 1.9); // hue shift, saturation x, value x
uniform vec3 box_min = vec3(-3.92, 0.0, -7.79); // the sheet's box, Godot's axes
uniform vec3 box_size = vec3(7.96, 13.49, 9.19);

varying vec3 b; // Generated, Blender's order: across, depth, height
// step 9: moves every pattern on this one sheet (an instance uniform: set per MeshInstance3D,
// not per material), so sheets side by side don't repeat each other
instance uniform vec2 pattern_offset = vec2(0.0);


// Voronoi F1, baked: noise_a's red (it was the nearest of 27 cells' points, worked out per pixel).
float cells(vec3 p) {
	return texture(waterfall_noise_a, p / NOISE_PERIOD).r;
}

// noise of 3 sizes added (fBM), 0..1, baked: a slice through noise_a's blue (the same 3 sizes).
float fbm(vec2 p) {
	return texture(waterfall_noise_a, vec3(p, 0.5) / NOISE_PERIOD).b;
}

// value noise in 3D, baked: noise_a's alpha (3 sizes, the smaller two faint: nearly one size).
float noise3(vec3 p) {
	return texture(waterfall_noise_a, p / NOISE_PERIOD).a;
}

// noise3 in 3 sizes (fBM), baked: roughness 0.5 is noise_a's blue, 0.2 its alpha (the two used).
float fbm3(vec3 p, float roughness) {
	vec4 n = texture(waterfall_noise_a, p / NOISE_PERIOD);
	return roughness > 0.35 ? n.b : n.a;
}

// Blender's Quadratic Sphere gradient: 1 at the centre, 0 one unit away, squared.
float sphere(vec3 p) {
	float r = max(1.0 - length(p), 0.0);
	return r * r;
}

// Blender's Wave Texture (bands across x, sine): stripes 0..1, each band bent sideways by noise.
float wave(vec2 p, float distortion) {
	float n = p.x * 20.0 + distortion * (fbm(p) * 2.0 - 1.0);
	return 0.5 + 0.5 * sin(n - PI * 0.5);
}

// F1 minus Smooth F1 (the cells' borders), baked: noise_a's green, stored x 2.
float cell_edges(vec3 p) {
	return texture(waterfall_noise_a, p / NOISE_PERIOD).g * 0.5;
}

vec3 rgb_to_hsv(vec3 c) {
	vec4 k = vec4(0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0);
	vec4 p = mix(vec4(c.bg, k.wz), vec4(c.gb, k.xy), step(c.b, c.g));
	vec4 q = mix(vec4(p.xyw, c.r), vec4(c.r, p.yzx), step(p.x, c.r));
	float d = q.x - min(q.w, q.y);
	return vec3(abs(q.z + (q.w - q.y) / (6.0 * d + 1e-10)), d / (q.x + 1e-10), q.x);
}

vec3 hsv_to_rgb(vec3 c) {
	vec3 p = abs(fract(c.xxx + vec3(1.0, 2.0 / 3.0, 1.0 / 3.0)) * 6.0 - 3.0);
	return c.z * mix(vec3(1.0), clamp(p - 1.0, 0.0, 1.0), c.y);
}

void vertex() {
	vec3 p = (VERTEX - box_min) / box_size; // Godot's axes: x across, y up, z toward the front
	b = vec3(p.x, 1.0 - p.z, p.y);
	b.xy += pattern_offset;  // across and depth only: the height bands stay where they are
	// ripples: the sheet pushed along its normal by a noise sliding down (b is taken first,
	// so the colours stay put on the sheet while its surface moves)
	float ripple = fbm((VERTEX.xy + pattern_offset * 7.0) / ripple_size + vec2(0.0, ripple_fall * TIME / ripple_size));  // each sheet its own ripples
	VERTEX += NORMAL * (ripple - 0.5) * ripple_strength;
}

void fragment() {
	// layer 1: the height line 0.86 - height, bent toward the cells' distance (24 %), cut at 0.467:
	// above it the upper tone, below the lower one
	float d1 = cells(vec3(b.x * 1.29, b.y, b.z + 0.296 * TIME) * 7.0);
	float line1 = mix(0.86 - b.z, d1, 0.242);
	vec3 colour = mix(deep_color, deep_color_2, step(0.467, line1)) * deep_value;

	// layer 2: light teal where another bent height line is under 0.382 (the top part)
	float d2 = cells((vec3(b.x * 1.19, b.y, b.z + 0.129 * TIME) + vec3(8.5, 0.0, 2.28)) * 7.2);
	float line2 = mix(0.92 - b.z, d2, 0.183);
	colour = mix(light_color, colour, step(0.382, line2));

	// layer 3: aqua circles, the middles of cells (closer than 0.487 to a point), in a soft round
	// patch near the top (UV: v 0.84, toward the right), its edge wobbled by noise
	float d3 = cells((vec3(b.x, b.y * 3.22, b.z + 0.245 * TIME) + vec3(0.18, 0.0, 0.0)) * 5.4);
	float circle = step(d3, 0.487);
	vec2 uv = vec2(UV.x, 1.0 - UV.y); // Blender's UV (glTF flips v)
	vec2 patch_p = vec2(uv.x - 0.93, uv.y * 5.16 - 4.34);
	patch_p = mix(patch_p, vec2(noise(b.xz * 4.1)), 0.445);
	float patch_mask = pow(max(1.0 - length(patch_p), 0.0), 2.0);
	patch_mask = clamp((patch_mask - 0.14) / 0.86, 0.0, 1.0);
	colour = mix(colour, aqua_color, step(0.255, circle * patch_mask));

	// streaks: the crests of bent stripes across the sheet (u), thin lines; static
	float stripes = wave(uv * 2.38, 3.1);
	float edge = fwidth(stripes);  // how much it changes over one pixel
	float lines = smoothstep(0.98 - edge, 0.98 + edge, stripes);
	// the mask: a wave turned on its side (bands up the sheet, v), sliding down
	// 1.2 sheets a second, its crests times a slowly changing noise, cut at 0.173
	float sideways = wave(vec2(-uv.y - 8.56 - 1.195 * TIME, uv.x) * 0.36, 4.18);
	float crests = clamp((sideways - 0.86) / 0.14, 0.0, 1.0);
	float blobs = noise3(vec3(uv.x + 8.5, uv.y * 4.0, 3.0 + 0.27 * TIME) * 4.0);
	blobs = clamp((blobs - 0.04) / 0.96, 0.0, 1.0);
	float mask = step(0.173, crests * blobs);
	colour = mix(colour, colour * streak_brightness, lines * mask);  // 2.4 x brighter

	// dark lines: the troughs of nearly straight stripes, 4 or 5 across the sheet; static
	float grain = wave(vec2((b.x + 0.94) * 1.4, b.z * 1.4), 0.3);
	float grain_edge = fwidth(grain);
	float dark = 1.0 - smoothstep(0.005 - grain_edge, 0.005 + grain_edge, grain);  // 1 in a trough
	colour = mix(colour, colour * line_darkness, dark);  // half as bright

	// the white patch: centred above the top-left (UV turned round and stretched), pulled
	// 44 % toward a boiling noise, cut at 0.214
	vec3 patch_q = vec3(0.09 - uv.x, 1.6 - 1.23 * uv.y, 0.0);
	float boil = fbm3(vec3(b.x * 5.9, b.y, b.z + 1.134 + 0.468 * TIME) * 2.7, 0.2);
	patch_q = mix(patch_q, vec3(boil), 0.439);
	colour = mix(colour, glow_color, step(0.214, sphere(patch_q)));

	// the rings: a band under the top (v 0.825, 0.1 tall), divided by a still noise
	vec3 ring_q = vec3(uv.x - 0.41, uv.y * 9.54 - 7.87, 0.0);
	float islands = clamp((fbm3(vec3(b.x * 2.49, b.y, b.z) * 5.0, 0.5) - 0.28) / 0.72, 0.0, 1.0);
	ring_q *= 0.617 + 0.383 / max(islands, 0.001);  // Blender's Divide mix, 38 %
	float rings = clamp(floor(sphere(ring_q) * 12.58) / 11.58, 0.0, 1.0);  // 12 flat steps
	colour = mix(colour, colour * ring_brightness, rings);

	// four tiers by height: the height pulled toward a still noise, cut into 4 steps
	float tier_line = mix(b.z - 0.27, fbm3(b * 5.0, 0.5), 0.442);
	float tier = clamp(floor(clamp(tier_line, 0.0, 1.0) * 3.68) / 2.68, 0.0, 1.0);  // 0, 0.37, 0.75, 1
	colour *= mix(bottom_brightness, top_brightness, tier);  // dark bottom .. light top

	// lace: the cells' borders on the UV (1.83 x 5: 9 cells across), nudged toward a noise
	vec3 lace_p = vec3(uv * 1.83 + vec2(0.0, 1.735), 0.0);
	lace_p = mix(lace_p, vec3(fbm3(b * 5.0, 0.5)), 0.15);
	float lace = step(lace_edge, cell_edges(lace_p * 5.0));

	// drips: slow bands across the sheet, pulled toward a still noise so they wander, cut
	// into three levels; only there does the lace show
	float column_x = mix(b.x + 0.33, fbm3(vec3(b.x, b.y, b.z + 3.135) * 5.0, 0.5), 0.375) * 0.87;
	float column = 0.5 + 0.5 * sin(column_x * 20.0 - PI * 0.5);  // a Wave Texture, not bent
	column = clamp((column - 0.14) / 0.86, 0.0, 1.0);
	float drip = column >= 0.782 ? 1.0 : (column >= 0.527 ? drip_side : 0.0);  // three levels
	colour = mix(colour, vec3(1.0), lace * drip);  // white where lace and column meet

	// grade: hue + 0.06, saturation x 1.7, value x 1.9
	vec3 hsv = rgb_to_hsv(colour);
	hsv = vec3(fract(hsv.x + grade.x), clamp(hsv.y * grade.y, 0.0, 1.0), hsv.z * grade.z);
	ALBEDO = hsv_to_rgb(hsv);
}
