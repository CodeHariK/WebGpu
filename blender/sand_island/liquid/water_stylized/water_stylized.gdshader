// Water, stylized: the simple waterfall's look (waterfall_simple/) on the game's MC water mesh.
// One shader for the whole mesh, like the mobile water: the falls get the sheet's layers, the flat
// water the pool's, where a fall lands the froth's churn. What the MC mesh gives, and what it stands
// in for (the waterfall's coordinates were Blender's Generated over one sheet / one pool):
//   falls   across = UV.x (metres along the cliff edge), height = UV2.y over (UV2.y + UV.y): 1 at
//           the lip, 0 where it lands, whatever the fall's height, so the lip glow sits on every
//           lip and the tiers run from light top to dark foot; the cells are sized in metres
//           (one sheet: 7.96 m across, 13.49 m tall) and fall with time as on the sheet
//   flat    the pool's bands, tiers and webs by depth (CUSTOM0.x) instead of by distance from the
//           falls: deep water dark, shallow light; its patterns sized in metres (one pool: 27.5 x
//           20 m); the glints drift along the flow (COLOR.rg) in streams
//   foam    where a fall lands (COLOR.a, and the last metre of each fall: UV2.y) the froth's white
//           over blue, churning; along the shore (depth under shore_width) the lace web in white
// Lit flat, not by the normal: the sun (or moon) times its shadow, so a day shot keeps the Blender
// scene's unshaded colours, cliffs shade the falls, and the night is dark like the other waters.
// No sky (ambient) light: it washed the colours out to cyan.
shader_type spatial;
render_mode cull_disabled, specular_disabled, ambient_light_disabled;

#include "res://shader_lib/liquid_noise.gdshaderinc"

// the falls (waterfall_simple.gdshader), linear like Blender's
uniform vec3 deep_color = vec3(0.007, 0.060, 0.072);
uniform vec3 deep_color_2 = vec3(0.031, 0.111, 0.126);
uniform vec3 light_color = vec3(0.087, 0.317, 0.320);
uniform vec3 aqua_color = vec3(0.214, 0.6, 0.539);
uniform vec3 fall_grade = vec3(0.06, 1.7, 1.9); // hue shift, saturation x, value x
// the flat water (waterfall_pool.gdshader)
uniform vec3 teal_color = vec3(0.114, 0.319, 0.305);
uniform vec3 back_color = vec3(0.034, 0.151, 0.138);
uniform vec3 glint_color = vec3(0.334, 0.779, 0.732);
uniform vec3 flat_grade = vec3(0.07, 1.5, 2.0);
uniform float depth_range : hint_range(0.1, 4.0) = 1.2; // metres from shallow to deep
uniform float shore_width = 0.15; // metres of depth that get the shore lace
uniform float flow_speed = 0.9; // metres a second the glints drift in a stream
// the froth (waterfall_froth.gdshader)
uniform vec3 foam_color = vec3(1.5, 1.45, 1.5); // over 1: white after the tonemap (2.7 bloomed)
uniform vec3 under_color = vec3(0.038, 0.471, 0.66);
uniform float landing_height = 0.8; // metres above a fall's landing that froth
// the light
uniform float shade : hint_range(0.0, 1.0) = 0.6; // light in a shadow

varying float depth;
varying vec3 world;

// ---------------------------------------------------------------- noise (as in waterfall_simple/)

vec3 hash33(vec3 p) {
	p = fract(p * vec3(0.1031, 0.1030, 0.0973));
	p += dot(p, p.yxz + 33.33);
	return fract((p.xxy + p.yxx) * p.zyx);
}

// Voronoi F1: the distance to the nearest point.
float cells(vec3 p) {
	vec3 cell = floor(p);
	vec3 local_p = p - cell;
	float nearest = 8.0;
	for (int k = -1; k <= 1; k++) {
		for (int j = -1; j <= 1; j++) {
			for (int i = -1; i <= 1; i++) {
				vec3 offset = vec3(float(i), float(j), float(k));
				nearest = min(nearest, length(offset + hash33(cell + offset) - local_p));
			}
		}
	}
	return nearest;
}

// Voronoi F1 minus Smooth F1: 0 inside the cells, rising along their borders (the lace).
float cell_edges(vec3 p) {
	vec3 cell = floor(p);
	vec3 local_p = p - cell;
	float nearest = 8.0;
	float smooth_nearest = 8.0;
	for (int k = -1; k <= 1; k++) {
		for (int j = -1; j <= 1; j++) {
			for (int i = -1; i <= 1; i++) {
				vec3 offset = vec3(float(i), float(j), float(k));
				float d = length(offset + hash33(cell + offset) - local_p);
				nearest = min(nearest, d);
				float h = smoothstep(0.0, 1.0, 0.5 + (smooth_nearest - d));
				smooth_nearest = mix(smooth_nearest, d, h) - 0.5 * h * (1.0 - h);
			}
		}
	}
	return nearest - smooth_nearest;
}

// Gradient (Perlin) noise, about -1..1, and 3 sizes of it (fBM), 0..1 around 0.5.
float gnoise3(vec3 p) {
	vec3 i = floor(p);
	vec3 f = p - i;
	vec3 u = f * f * f * (f * (f * 6.0 - 15.0) + 10.0);
	float n000 = dot(hash33(i) * 2.0 - 1.0, f);
	float n100 = dot(hash33(i + vec3(1.0, 0.0, 0.0)) * 2.0 - 1.0, f - vec3(1.0, 0.0, 0.0));
	float n010 = dot(hash33(i + vec3(0.0, 1.0, 0.0)) * 2.0 - 1.0, f - vec3(0.0, 1.0, 0.0));
	float n110 = dot(hash33(i + vec3(1.0, 1.0, 0.0)) * 2.0 - 1.0, f - vec3(1.0, 1.0, 0.0));
	float n001 = dot(hash33(i + vec3(0.0, 0.0, 1.0)) * 2.0 - 1.0, f - vec3(0.0, 0.0, 1.0));
	float n101 = dot(hash33(i + vec3(1.0, 0.0, 1.0)) * 2.0 - 1.0, f - vec3(1.0, 0.0, 1.0));
	float n011 = dot(hash33(i + vec3(0.0, 1.0, 1.0)) * 2.0 - 1.0, f - vec3(0.0, 1.0, 1.0));
	float n111 = dot(hash33(i + vec3(1.0, 1.0, 1.0)) * 2.0 - 1.0, f - vec3(1.0, 1.0, 1.0));
	return mix(mix(mix(n000, n100, u.x), mix(n010, n110, u.x), u.y), mix(mix(n001, n101, u.x), mix(n011, n111, u.x), u.y), u.z);
}

float fbm3(vec3 p) {
	return 0.5 + 0.5 * (gnoise3(p) + gnoise3(p * 2.0) * 0.5 + gnoise3(p * 4.0) * 0.25) / 1.75;
}

// Blender's Wave Texture (bands across x, sine), bent by 2D noise (liquid_noise's).
float wave(vec2 p, float distortion) {
	float bend = noise(p) * 0.57 + noise(p * 2.0 + 3.1) * 0.29 + noise(p * 4.0 + 7.7) * 0.14;
	return 0.5 + 0.5 * sin(p.x * 20.0 + distortion * (bend * 2.0 - 1.0) - PI * 0.5);
}

float sphere(vec3 p) {
	float r = max(1.0 - length(p), 0.0);
	return r * r;
}

vec3 graded(vec3 c, vec3 grade) {
	vec4 k = vec4(0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0);
	vec4 p = mix(vec4(c.bg, k.wz), vec4(c.gb, k.xy), step(c.b, c.g));
	vec4 q = mix(vec4(p.xyw, c.r), vec4(c.r, p.yzx), step(p.x, c.r));
	float d = q.x - min(q.w, q.y);
	vec3 hsv = vec3(abs(q.z + (q.w - q.y) / (6.0 * d + 1e-10)), d / (q.x + 1e-10), q.x);
	hsv = vec3(fract(hsv.x + grade.x), clamp(hsv.y * grade.y, 0.0, 1.0), hsv.z * grade.z);
	vec3 r = abs(fract(hsv.xxx + vec3(1.0, 2.0 / 3.0, 1.0 / 3.0)) * 6.0 - 3.0);
	return hsv.z * mix(vec3(1.0), clamp(r - 1.0, 0.0, 1.0), hsv.y);
}

// ---------------------------------------------------------------- the falls: the sheet's layers

vec3 fall_look(vec2 p_uv, vec2 p_uv2) {
	float across = p_uv.x / 7.96; // in sheets
	float down = max(p_uv.y, 0.0); // metres from the lip
	float above = max(p_uv2.y, 0.0); // metres over the landing
	float height = clamp(above / max(above + down, 0.01), 0.0, 1.0); // 1 at the lip .. 0 at the foot
	float metric = above / 13.49; // the height in sheets, for the cells (they keep their size)
	vec3 b = vec3(across, 0.5, height);
	// layers 1 - 3: deep blues, light teal on top, aqua circles, cells falling
	float d1 = cells(vec3(b.x * 1.29, b.y, metric + 0.296 * TIME) * 7.0);
	vec3 colour = mix(deep_color, deep_color_2, step(0.467, mix(0.86 - b.z, d1, 0.242))) * 1.65;
	float d2 = cells((vec3(b.x * 1.19, b.y, metric + 0.129 * TIME) + vec3(8.5, 0.0, 2.28)) * 7.2);
	colour = mix(light_color, colour, step(0.382, mix(0.92 - b.z, d2, 0.183)));
	float d3 = cells((vec3(b.x, b.y * 3.22, metric + 0.245 * TIME) + vec3(0.18, 0.0, 0.0)) * 5.4);
	float band = clamp((sphere(vec3(0.0, b.z * 5.16 - 4.34, 0.0) * mix(1.0, noise(vec2(across, metric) * 4.1) * 2.0, 0.445)) - 0.14) / 0.86, 0.0, 1.0);
	colour = mix(colour, aqua_color, step(0.255, step(d3, 0.487) * band));
	// streaks: thin bright lines, shown in patches sliding down
	float stripes = wave(vec2(across, metric) * 2.38, 3.1);
	float edge = fwidth(stripes);
	float lines = smoothstep(0.98 - edge, 0.98 + edge, stripes);
	float sideways = wave(vec2(-metric - 1.195 * TIME, across) * 0.36, 4.18);
	float mask = step(0.173, clamp((sideways - 0.86) / 0.14, 0.0, 1.0)
			* clamp((fbm3(vec3(across + 8.5, metric * 4.0, 0.27 * TIME) * 4.0) - 0.04) / 0.96, 0.0, 1.0));
	colour *= mix(1.0, 2.4, lines * mask);
	// dark lines
	float grain = wave(vec2((across + 0.94) * 1.4, metric * 1.4), 0.3);
	float grain_edge = fwidth(grain);
	colour *= mix(1.0, 0.5, 1.0 - smoothstep(0.005 - grain_edge, 0.005 + grain_edge, grain));
	// the lip: a boiling near-white band along every lip
	float boil = fbm3(vec3(across * 5.9, 0.5, metric + 1.134 + 0.468 * TIME) * 2.7);
	colour = mix(colour, vec3(1.2, 1.175, 1.14), step(0.214, sphere(mix(vec3(0.0, 1.6 - 1.23 * b.z, 0.0), vec3(boil), 0.439))));
	// tiers: light top, dark foot
	float tier = clamp(floor(clamp(mix(b.z - 0.27, fbm3(vec3(across, 0.5, metric) * 5.0), 0.442), 0.0, 1.0) * 3.68) / 2.68, 0.0, 1.0);
	colour *= mix(0.43, 1.45, tier);
	// drips: the lace in wandering columns
	vec3 lace_p = mix(vec3(vec2(across, metric) * 1.83, 0.0), vec3(fbm3(vec3(across, 0.5, metric) * 5.0)), 0.15);
	float lace = step(0.179, cell_edges(lace_p * 5.0));
	float column = 0.5 + 0.5 * sin(mix(across + 0.33, fbm3(vec3(across, 0.5, metric + 3.135) * 5.0), 0.375) * 0.87 * 20.0 - PI * 0.5);
	column = clamp((column - 0.14) / 0.86, 0.0, 1.0);
	colour = mix(colour, vec3(1.0), lace * (column >= 0.782 ? 1.0 : (column >= 0.527 ? 0.088 : 0.0)));
	return graded(colour, fall_grade);
}

// ---------------------------------------------------------------- the flat water: the pool's

vec3 flat_look(vec2 p_metres, float p_depth, vec2 p_flow, float p_flows) {
	vec3 b = vec3(p_metres.x / 27.5, p_metres.y / 20.0, 0.5); // in pools
	float deep = smoothstep(0.0, depth_range, p_depth); // 0 at the shore .. 1 deep
	// bands: shallow light, deep dark (the pool's front / back lines, by depth)
	float d1 = cells((vec3(b.x * 2.0, b.y, b.z) + vec3(8.5, 0.0, 2.28 + 0.129 * TIME)) * 8.5);
	vec3 colour = teal_color * mix(1.0, 0.5, step(0.427, mix(deep * 0.7, d1, 0.25)));
	float d2 = cells((vec3(b.x * 1.9, b.y, b.z) + vec3(8.5, 0.0, 3.003 + 0.269 * TIME)) * 7.9);
	colour = mix(colour, back_color, 1.0 - step(0.427, mix(1.05 - deep, d2, 0.25)));
	// glints, drifting with the flow in a stream
	vec3 drift = vec3(-p_flow * flow_speed * p_flows * TIME / vec2(27.5, 20.0), 0.0);
	float wide = fbm3((vec3(b.x, b.y * 3.44, b.z) + drift + vec3(0.0, 0.0, 0.81 + 0.224 * TIME)) * 5.76);
	float thin = fbm3((vec3(b.x * 4.95, b.y * 3.44, b.z) + drift + vec3(0.0, 0.0, 1.729 - 0.441 * TIME)) * 5.76);
	colour = mix(colour, glint_color, step(0.0545, clamp((thin - 0.59) / 0.51, 0.0, 1.0) * 0.28));
	colour = mix(colour, glint_color, step(0.0545, clamp((wide - 0.47) / 0.63, 0.0, 1.0) * 0.26));
	// tiers: shallow brighter, deep darker
	float tier = clamp(floor(clamp(mix(deep, fbm3(vec3(b.x, b.y, b.z + 1.0) * 5.5), 0.342), 0.0, 1.0) * 6.0) / 5.0, 0.0, 1.0);
	colour *= mix(1.7, 0.35, tier);
	return graded(colour * 0.66, flat_grade);
}

// ---------------------------------------------------------------- foam

// The froth's churn: white over blue split by a noise changing in place, 0 blue .. 1 white.
float churn(vec3 p) {
	return step(0.5, fbm3(p * vec3(1.0, 1.0, 1.0) + vec3(0.0, 0.0, 0.37 * TIME)));
}

void vertex() {
	depth = CUSTOM0.x;
	world = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;
}

void fragment() {
	float on_fall = smoothstep(0.0, 0.85, UV2.x); // 0 on the flat, 1 down a fall
	vec2 flow = COLOR.rg * 2.0 - 1.0;
	float flows = clamp(COLOR.b, 0.0, 1.0);
	vec3 colour = vec3(0.0);
	if (on_fall > 0.0) {
		colour += fall_look(UV, UV2) * on_fall;
	}
	if (on_fall < 1.0) {
		vec3 flat_colour = flat_look(world.xz, depth, flow, flows);
		// the shore: the lace web in white where it is shallow (not on a stream: all shallow)
		float shore = (1.0 - smoothstep(0.0, shore_width, depth)) * (1.0 - flows);
		// the cells' borders, widest at the shore and gone by shore_width (a web, never solid)
		float shore_lace = step(mix(0.4, 0.12, shore), cell_edges(vec3(world.xz * 0.9, 0.5) * 2.0)) * step(0.01, shore);
		flat_colour = mix(flat_colour, foam_color, shore_lace);
		colour += flat_colour * (1.0 - on_fall);
	}
	// froth where a fall lands: on the flat by COLOR.a, on the fall in its last landing_height
	float landing = smoothstep(0.5, 1.0, COLOR.a) * (1.0 - on_fall);
	float foot = (1.0 - smoothstep(0.0, landing_height, UV2.y + noise(vec2(UV.x * 3.0, TIME)) * 0.3)) * on_fall * step(0.0, UV2.y);
	float froth = max(landing, foot);
	if (froth > 0.0) {
		vec3 froth_colour = mix(under_color, foam_color, churn(vec3(world.x, world.y * 2.0, world.z) * 1.2));
		colour = mix(colour, froth_colour, step(0.3, froth));
	}
	ALBEDO = colour;
}

// Flat light: the sun's colour and energy, darker in a shadow; not by the normal (no 3D shading).
// LIGHT_COLOR comes multiplied by PI: / PI gives a sun of energy 1 the unshaded colours.
void light() {
	DIFFUSE_LIGHT += mix(shade, 1.0, ATTENUATION) * LIGHT_COLOR / PI;
}
