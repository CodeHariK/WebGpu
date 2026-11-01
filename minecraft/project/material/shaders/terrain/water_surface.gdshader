// Water surface for MCGrid's water layer (src/marching_cubes/grid/mc_grid_water.cpp).
// One flat quad per wet corner column. The mesh's vertex COLOR carries the flow:
//   r, g  flow direction (x, z) as 0..1 (0.5 = none)    b  1 when this water flows
//   a     1 where a waterfall lands (extra foam)
// UV2.x is 1 on a stream (a thin sheet over open ground): it is shallow everywhere, so no shore foam.
// Built up one idea per step:
shader_type spatial;
render_mode blend_mix, depth_draw_never, cull_disabled, specular_schlick_ggx;

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
// and where a waterfall lands (vertex colour a).
uniform vec3 foam_color : source_color = vec3(0.95, 0.99, 1.0);
uniform float foam_width = 0.18; // metres of depth that still get shore foam
// Step 4: ripples: noise sliding along the flow (rivers) or drifting slowly (ponds, sea); they tilt
// the normal, so the sun glints on them.
uniform float ripple_scale = 1.3;
uniform float flow_speed = 0.9;
uniform float ripple_tilt = 0.35;
// Step 5: looking across the water it gets lighter (fresnel).
uniform float fresnel_power = 4.0;
// Step 6: cartoon water keeps its colour in shade and under a grey sky: part of it glows (emission),
// and the ripple crests sparkle.
uniform float glow = 0.35;
uniform float sparkle = 0.8;

varying vec3 world;
varying vec4 flow;
varying float stream;

void vertex() {
	world = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;
	flow = COLOR;
	stream = UV2.x;
}

// Value noise: a random value per grid point, smoothly blended in between.
float hash(vec2 p) {
	return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
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

// Ripples carried along a flow that changes from place to place. Sliding the pattern for ever would
// stretch it wherever the direction turns, so two copies slide for one cycle each, half a cycle
// apart, and each fades out as it restarts (the usual flow-map trick).
uniform float flow_cycle = 1.6; // seconds before a copy restarts
float flowing_ripples(vec2 p, vec2 velocity, float t) {
	float phase0 = fract(t / flow_cycle);
	float phase1 = fract(t / flow_cycle + 0.5);
	float weight0 = 1.0 - abs(1.0 - 2.0 * phase0); // 0 when copy 0 restarts, 1 half way
	float r0 = ripples(p - velocity * phase0 * flow_cycle);
	float r1 = ripples(p - velocity * phase1 * flow_cycle + vec2(3.7, 1.3));
	return mix(r1, r0, weight0);
}

void fragment() {
	// step 1: depth of water behind this pixel (view space: the ground's distance minus ours)
	float raw = texture(depth_texture, SCREEN_UV).r;
	vec4 behind = INV_PROJECTION_MATRIX * vec4(SCREEN_UV * 2.0 - 1.0, raw, 1.0);
	behind.xyz /= behind.w;
	float depth = max(VERTEX.z - behind.z, 0.0);
	float deep = smoothstep(0.0, deep_at, depth);
	vec3 color = mix(shallow_color, deep_color, deep);

	// step 4: ripples slide along the flow (b blends from still drift to the flow near a river's
	// edge), or drift slowly where the water is still
	vec2 direction = flow.rg * 2.0 - 1.0;
	vec2 drift = mix(vec2(0.08, 0.05), direction * flow_speed, clamp(flow.b, 0.0, 1.0));
	vec2 p = world.xz * ripple_scale;
	vec2 velocity = drift * ripple_scale;
	float r = flowing_ripples(p, velocity, TIME);
	float e = 0.05; // slope of the ripples -> tilt the normal (world space, then to view space)
	vec2 slope = vec2(flowing_ripples(p + vec2(e, 0.0), velocity, TIME) - r,
			flowing_ripples(p + vec2(0.0, e), velocity, TIME) - r) / e;
	vec3 tilt = (VIEW_MATRIX * vec4(-slope.x, 0.0, -slope.y, 0.0)).xyz * ripple_tilt * 0.1;
	NORMAL = normalize(NORMAL + tilt);
	color += (r - 0.5) * 0.08; // light / dark bands

	// step 3: foam at the banks (shallow) and where a fall lands, broken up by noise
	float wobble = noise(world.xz * 4.0 + TIME * 0.6);
	float shore = (1.0 - smoothstep(0.0, foam_width * (0.6 + wobble * 0.8), depth)) * (1.0 - stream);
	float churn = noise(p * 2.0 + vec2(TIME * 1.5, -TIME * 1.1));
	// (flow.a is 1 at the foot of the fall and fades across the column, so the foam hugs the fall)
	float landing = smoothstep(0.55, 1.0, flow.a) * smoothstep(0.45, 0.75, churn) * 0.85;
	// thin streaks of foam along flowing water
	float streaks = flow.b * smoothstep(0.84, 0.95, r) * 0.4;
	float foam = clamp(shore + landing + streaks, 0.0, 1.0);

	// step 5: lighter toward the horizon
	float fresnel = pow(1.0 - clamp(dot(NORMAL, VIEW), 0.0, 1.0), fresnel_power);
	color = mix(color, shallow_color * 1.2, fresnel * 0.5);

	// step 6: glints on the highest ripple crests, twinkling
	float glint = smoothstep(0.86, 0.95, r) * smoothstep(0.4, 0.9, noise(world.xz * 9.0 + TIME * 2.0)) * sparkle;
	foam = clamp(foam + glint, 0.0, 1.0);

	ALBEDO = mix(color, foam_color, foam);
	EMISSION = ALBEDO * glow;
	ALPHA = mix(mix(alpha_shallow, alpha_deep, deep), 1.0, foam);
	ROUGHNESS = 0.15;
	SPECULAR = 0.4;
}
