// Waterfall strips for MCGrid's water layer (src/marching_cubes/grid/mc_grid_water_falls.cpp).
// One strip per fall: a rounded lip from the surface over the edge, straight down, a flared foot.
//   UV.x  metres along the fall         UV.y  metres along the strip from the lip's start
//   UV2.x metres above the landing (0 at the bottom)
//   UV2.y how far over the lip (0 at its start on the surface, 1 once it falls straight down)
// Built up one idea per step:
shader_type spatial;
// depth_draw_always: the falls write depth, and the grid draws them before the water surfaces
// (render priority -1), so a surface behind a fall is hidden and one in front blends over it — the
// surfaces themselves don't write depth, so without this a fall drawn after them shows through them.
render_mode blend_mix, depth_draw_always, cull_disabled;

// Step 1: a base colour: lighter than the pool (falling water is full of air), nearly opaque, so
// the sheet reads as pouring water, not glass.
uniform vec3 water_color : source_color = vec3(0.45, 0.80, 0.92);
uniform vec3 foam_color : source_color = vec3(0.96, 0.99, 1.0);
uniform float alpha = 0.93;
// Step 2: streaks: thin bright threads running down. Each thread is noise in x (so they don't line
// up) stretched long in y, and the whole pattern slides down with TIME.
uniform float threads_per_metre = 9.0;
uniform float fall_speed = 1.6;
// Step 3: a white lip where the water tips over the top edge, and foam where it lands.
uniform float lip_height = 0.12;
uniform float foam_height = 0.35;
// Step 4: the rounded lip carries on from the surface: it starts with the surface's colour and
// clearness and turns into the falling water's streaks as it bends over, with a bright band where
// it turns down (falls with no lip keep step 3's white top edge instead).
uniform vec3 surface_color : source_color = vec3(0.40, 0.88, 0.92);
uniform float surface_alpha = 0.75;

float hash(vec2 p) {
	return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

float noise(vec2 p) {
	vec2 i = floor(p);
	vec2 f = fract(p);
	vec2 u = f * f * (3.0 - 2.0 * f);
	return mix(mix(hash(i), hash(i + vec2(1.0, 0.0)), u.x), mix(hash(i + vec2(0.0, 1.0)), hash(i + vec2(1.0, 1.0)), u.x), u.y);
}

void fragment() {
	// step 2: threads: sample noise squashed vertically (x fine, y coarse) and scroll it down
	vec2 p = vec2(UV.x * threads_per_metre, UV.y * 1.2 - TIME * fall_speed);
	float threads = noise(p) * 0.6 + noise(p * vec2(2.1, 1.7) + 4.3) * 0.4;
	float bright = smoothstep(0.55, 0.85, threads);
	vec3 color = mix(water_color, foam_color, bright * 0.7);
	// darker gaps between threads give the sheet depth
	color *= 0.85 + 0.15 * threads;

	// step 3: the white top edge (falls with no lip) and the foam at the bottom, broken up by noise
	float plain_top = step(0.999, UV2.y) * (1.0 - smoothstep(0.0, lip_height, UV.y + (threads - 0.5) * 0.05));
	float foam = 1.0 - smoothstep(0.0, foam_height, UV2.x - threads * 0.15);

	// step 4: over the lip: from the surface's look into the streaks, bright where it turns down
	float over = smoothstep(0.0, 0.85, UV2.y);
	color = mix(surface_color, color, over);
	float turn = smoothstep(0.35, 0.75, UV2.y) * (1.0 - smoothstep(0.85, 1.0, UV2.y)) * (0.6 + 0.4 * threads);
	color = mix(color, foam_color, clamp(plain_top * 0.8 + turn * 0.7 + foam, 0.0, 1.0));

	ALBEDO = color;
	EMISSION = color * 0.3; // stays bright in shade, like the pool
	ALPHA = mix(surface_alpha, alpha, over);
	ROUGHNESS = 0.35;
}
