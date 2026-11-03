// Water for MCGrid's water layer (src/marching_cubes/grid/mc_grid_water.cpp): ONE material for the
// pools, the rivers and the waterfalls, drawn as ONE continuous mesh: the surface itself bends over a
// fall's edge and the fall hangs from its edge vertices (mc_grid_water_falls.cpp). Its UV is one
// flow coordinate: the world x, z on the flat water, carried on over the lip and down the fall as
// if the fall were laid flat beyond the edge. Everything here runs on UV, so the same ripples and
// streaks flow from the pool over the edge and down without a break.
//
// Inputs (per vertex):
//   CUSTOM0  x  falls: metres along the joined falls (the streaks run across it)
//   COLOR    the flow: rg direction (x, z) as 0..1 (0.5 = none), b 1 where it flows,
//            a 1 where a waterfall lands (extra foam)
//   UV2      x  how far over a fall's lip: 0 on the surface (and the strip's first row) .. 1 once
//               it falls straight down
//            y  falls: metres above the landing (>= 0); surfaces (and a strip's first row): -1 on a
//               stream (a thin sheet over open ground: no shore foam), else 0
//            (packed in UV2: reading a CUSTOM0 channel hangs the renderer on macOS / MoltenVK)
//   UV       the flow coordinate: metres, world x, z on the flat water, unfolded down the falls
// Built up one idea per step: steps 1-6 are the surface, 7-9 the falling water, 10 joins them.
shader_type spatial;
// depth_draw_always: the mesh lists the falls' triangles first; they write depth, so a surface
// behind a fall is hidden and one in front blends over it. cull_disabled: the strips are seen from
// both sides.
render_mode blend_mix, depth_draw_always, cull_disabled, specular_schlick_ggx;

// --- the surface ---
// Step 1: colour by depth: light turquoise where it's shallow, blue where it's deep. The depth is
// how far the ground behind the water is from the surface, read from the depth texture.
uniform vec3 shallow_color : source_color = vec3(0.40, 0.88, 0.92);
uniform vec3 deep_color : source_color = vec3(0.10, 0.50, 0.86);
uniform float deep_at = 1.6; // metres of water that look fully deep
uniform sampler2D depth_texture : hint_depth_texture, filter_linear_mipmap;
// Step 2: mostly opaque (cartoon water), a little clearer where it's shallow.
uniform float alpha_shallow = 0.7;
uniform float alpha_deep = 0.95;
// Step 3: foam: a white line where the water meets a bank (very shallow), wobbling with noise,
// and where a waterfall lands (COLOR.a).
uniform vec3 foam_color : source_color = vec3(0.95, 0.99, 1.0);
uniform float foam_width = 0.18; // metres of depth that still get shore foam
// Step 4: ripples: noise sliding along the flow (rivers) or drifting slowly (ponds, sea); they tilt
// the normal, so the sun glints on them.
uniform float ripple_scale = 1.3;
uniform float flow_speed = 0.9;
uniform float ripple_tilt = 0.35;
uniform float flow_cycle = 1.6; // seconds before a flow-map copy restarts
// Step 5: looking across the water it gets lighter (fresnel).
uniform float fresnel_power = 4.0;
// Step 6: cartoon water keeps its colour in shade: part of it glows, and ripple crests sparkle.
uniform float glow = 0.35;
uniform float sparkle = 0.8;

// --- the falling water ---
// Step 7: a lighter base colour (falling water is full of air), nearly opaque.
uniform vec3 fall_color : source_color = vec3(0.45, 0.80, 0.92);
uniform float fall_alpha = 0.93;
// Step 8: streaks in the fall's own frame: across = metres along the joined falls (CUSTOM0.x),
// down = metres above the landing; stretched long downwards and sliding down. (Projecting the
// world-scale UV onto a per-vertex direction speckles wherever the direction turns.)
uniform float threads_per_metre = 9.0;
uniform float fall_speed = 1.8; // m/s the streaks slide down
// Step 9: a bright band where the water turns over the edge, foam where it lands.
uniform float foam_height = 0.35;

varying vec3 world;
varying vec4 flow;
varying float over;
varying float above;
varying float stream;
varying float fall_across;

void vertex() {
	world = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;
	flow = COLOR;
	over = UV2.x;
	above = max(UV2.y, 0.0);
	stream = clamp(-UV2.y, 0.0, 1.0);
	fall_across = CUSTOM0.x;
}

// Value noise: a random value per grid point, smoothly blended in between. The hash has no sin():
// sin of a large number loses precision on some GPUs (Metal), and with UVs of tens of metres that
// turns the streaks into speckle (Dave Hoskins' hash without sine).
float hash(vec2 p) {
	vec3 p3 = fract(vec3(p.xyx) * 0.1031);
	p3 += dot(p3, p3.yzx + 33.33);
	return fract((p3.x + p3.y) * p3.z);
}

float noise(vec2 p) {
	vec2 i = floor(p);
	vec2 f = fract(p);
	vec2 u = f * f * (3.0 - 2.0 * f);
	return mix(mix(hash(i), hash(i + vec2(1.0, 0.0)), u.x), mix(hash(i + vec2(0.0, 1.0)), hash(i + vec2(1.0, 1.0)), u.x), u.y);
}

// Two octaves of noise, 0..1.
float ripples(vec2 p) {
	return noise(p) * 0.65 + noise(p * 2.3 + 7.1) * 0.35;
}

// Ripples carried along a flow that changes from place to place: two copies slide for one cycle
// each, half a cycle apart, each fading out as it restarts (the usual flow-map trick).
float flowing_ripples(vec2 p, vec2 velocity, float t) {
	float phase0 = fract(t / flow_cycle);
	float phase1 = fract(t / flow_cycle + 0.5);
	float weight0 = 1.0 - abs(1.0 - 2.0 * phase0);
	float r0 = ripples(p - velocity * phase0 * flow_cycle);
	float r1 = ripples(p - velocity * phase1 * flow_cycle + vec2(3.7, 1.3));
	return mix(r1, r0, weight0);
}

void fragment() {
	// step 10 (used throughout): how much this pixel is falling water, 0 on the surface
	float falling = smoothstep(0.0, 0.85, over);

	// --- the surface (steps 1-6) ---
	// step 1: depth of water behind this pixel (view space: the ground's distance minus ours)
	float raw = texture(depth_texture, SCREEN_UV).r;
	vec4 behind = INV_PROJECTION_MATRIX * vec4(SCREEN_UV * 2.0 - 1.0, raw, 1.0);
	behind.xyz /= behind.w;
	float depth = max(VERTEX.z - behind.z, 0.0);
	float deep = smoothstep(0.0, deep_at, depth);
	vec3 surface = mix(shallow_color, deep_color, deep);
	// step 4: ripples along the flow
	vec2 direction = flow.rg * 2.0 - 1.0;
	vec2 drift = mix(vec2(0.08, 0.05), direction * flow_speed, clamp(flow.b, 0.0, 1.0));
	vec2 p = UV * ripple_scale; // the flow coordinate (world x, z on the flat water)
	vec2 velocity = drift * ripple_scale;
	float r = flowing_ripples(p, velocity, TIME);
	float e = 0.05;
	vec2 slope = vec2(flowing_ripples(p + vec2(e, 0.0), velocity, TIME) - r,
			flowing_ripples(p + vec2(0.0, e), velocity, TIME) - r) / e;
	surface += (r - 0.5) * 0.08;
	// step 3: foam at the banks and where a fall lands
	float wobble = noise(UV * 4.0 + TIME * 0.6);
	float shore = (1.0 - smoothstep(0.0, foam_width * (0.6 + wobble * 0.8), depth)) * (1.0 - stream);
	float churn = noise(p * 2.0 + vec2(TIME * 1.5, -TIME * 1.1));
	float landing = smoothstep(0.55, 1.0, flow.a) * smoothstep(0.45, 0.75, churn) * 0.85;
	float streaks = flow.b * smoothstep(0.84, 0.95, r) * 0.4;
	// step 6: glints on the highest ripple crests
	float glint = smoothstep(0.86, 0.95, r) * smoothstep(0.4, 0.9, noise(UV * 9.0 + TIME * 2.0)) * sparkle;
	float surface_foam = clamp(shore + landing + streaks + glint, 0.0, 1.0);
	float surface_alpha = mix(mix(alpha_shallow, alpha_deep, deep), 1.0, surface_foam);

	// --- the falling water (steps 7-9) ---
	// step 8: streaks across the fall, stretched long downwards and sliding down
	vec2 q = vec2(fall_across * threads_per_metre, (-above - TIME * fall_speed) * 1.2);
	float threads = noise(q) * 0.6 + noise(q * vec2(2.1, 1.7) + 4.3) * 0.4;
	float bright = smoothstep(0.55, 0.85, threads);
	vec3 fall = mix(fall_color, foam_color, bright * 0.7);
	fall *= 0.85 + 0.15 * threads;
	// step 9: a bright band where it turns over the edge, foam where it lands
	float turn = smoothstep(0.35, 0.75, over) * (1.0 - smoothstep(0.85, 1.0, over)) * (0.6 + 0.4 * threads);
	float fall_foam = 1.0 - smoothstep(0.0, foam_height, above - threads * 0.15);
	fall = mix(fall, foam_color, clamp(turn * 0.7 + fall_foam * step(0.5, over), 0.0, 1.0));

	// --- step 10: one water: the surface's look turns into the falling water's over the lip ---
	vec3 color = mix(surface, fall, falling);
	float foam = surface_foam * (1.0 - falling);
	// the ripples tilt the surface's normal only (world slope -> view space)
	vec3 tilt = (VIEW_MATRIX * vec4(-slope.x, 0.0, -slope.y, 0.0)).xyz * ripple_tilt * 0.1 * (1.0 - falling);
	NORMAL = normalize(NORMAL + tilt);
	// step 5: lighter toward the horizon (the surface)
	float fresnel = pow(1.0 - clamp(dot(NORMAL, VIEW), 0.0, 1.0), fresnel_power) * (1.0 - falling);
	color = mix(color, shallow_color * 1.2, fresnel * 0.5);

	ALBEDO = mix(color, foam_color, foam);
	EMISSION = ALBEDO * mix(glow, 0.3, falling);
	ALPHA = mix(surface_alpha, fall_alpha, falling);
	ROUGHNESS = mix(0.15, 0.35, falling);
	SPECULAR = 0.4;
}
