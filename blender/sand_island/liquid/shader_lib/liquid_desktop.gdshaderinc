// The desktop liquid shaders' shared tools (water_desktop, lava_desktop, poison_desktop): built for
// looks first, no mobile budget (dozens of noise lookups a pixel, the screen and depth textures,
// lighting). If one of them is liked, it gets simplified into the mobile set (*_mobile/).
// The mesh data they read is listed in liquid_common.gdshaderinc (COLOR flow, UV, UV2, depth).
// Functions can't read built-ins (UV, COLOR, TIME, the matrices), so fragment() hands them in.
#ifndef LIQUID_DESKTOP
#define LIQUID_DESKTOP

#include "res://shader_lib/liquid_noise.gdshaderinc"

// The opaque scene behind the liquid, as the camera saw it before the liquid was drawn: its colour
// (for refraction: looking through the liquid) and its depth (how far the floor is behind it).
// Reading the screen texture puts the material in the transparent pass, after the opaque scene.
uniform sampler2D liquid_screen : hint_screen_texture, filter_linear_mipmap;
uniform sampler2D liquid_depth : hint_depth_texture, filter_linear_mipmap;

// Which part of the mesh a pixel is on, and where the liquid runs (from UV2 and COLOR).
struct LiquidFlow {
	float on_flat; // 1 on the flat liquid, 0 down a fall (blends over the lip)
	float on_stream; // 1 on a stream (a thin sheet), 0 on held water
	vec2 direction; // the flow direction (-1..1), 0 in a pond
	float flows; // 1 in a river, 0 in a pond
	float landing; // 1 where a fall lands
};

LiquidFlow liquid_flow(vec2 p_uv2, vec4 p_flow) {
	LiquidFlow f;
	f.on_flat = 1.0 - smoothstep(0.0, 0.85, p_uv2.x);
	f.on_stream = clamp(-p_uv2.y, 0.0, 1.0);
	f.direction = p_flow.rg * 2.0 - 1.0;
	f.flows = clamp(p_flow.b, 0.0, 1.0);
	f.landing = smoothstep(0.5, 1.0, p_flow.a);
	return f;
}

// fbm ("fractal" noise): octaves of noise, each twice as fine and half as strong, each turned a
// little so their grids don't line up. Soft big shapes with detail on top, 0..1.
float fbm(vec2 p, int octaves) {
	float sum = 0.0;
	float amplitude = 0.5;
	float total = 0.0;
	mat2 turn = mat2(vec2(1.6, 1.2), vec2(-1.2, 1.6)); // x2 and turned ~37 degrees
	for (int i = 0; i < octaves; i++) {
		sum += noise(p) * amplitude;
		total += amplitude;
		p = turn * p + vec2(1.7, 9.2);
		amplitude *= 0.5;
	}
	return sum / total;
}

// The flow-map trick's two copies (see liquid_common.gdshaderinc, flowing_ripples): how far each
// has slid along the flow and how much of copy 0 to show.
struct FlowCopies {
	vec2 offset0;
	vec2 offset1;
	float weight0; // 0 at copy 0's restart .. 1 halfway
	float phase0; // 0..1 through copy 0's cycle
	float phase1;
};

FlowCopies flow_copies(vec2 p_velocity, float p_time, float p_cycle) {
	FlowCopies c;
	c.phase0 = fract(p_time / p_cycle);
	c.phase1 = fract(p_time / p_cycle + 0.5);
	c.weight0 = 1.0 - abs(1.0 - 2.0 * c.phase0);
	c.offset0 = p_velocity * c.phase0 * p_cycle;
	c.offset1 = p_velocity * c.phase1 * p_cycle - vec2(3.7, 1.3); // offset: not the same shapes
	return c;
}

// A height field of waves at p (metres) carried along the flow: fbm in the flow-map trick's two
// copies, plus a broad swell drifting with the wind (not the flow), so ponds aren't glassy.
float wave_height(vec2 p, FlowCopies c, float p_scale, int p_octaves, float p_time) {
	float h0 = fbm((p - c.offset0) * p_scale, p_octaves);
	float h1 = fbm((p - c.offset1) * p_scale, p_octaves);
	float swell = noise(p * 0.45 + vec2(p_time * 0.11, p_time * 0.07));
	return mix(h1, h0, c.weight0) * 0.8 + swell * 0.2;
}

// The waves' slope (rise per metre in x and z), from three heights a step apart (finite
// differences): the wave normal is normalize(-slope.x, 1, -slope.y).
vec2 wave_slope(vec2 p, FlowCopies c, float p_scale, int p_octaves, float p_time) {
	float e = 0.03; // metres
	float h = wave_height(p, c, p_scale, p_octaves, p_time);
	float hx = wave_height(p + vec2(e, 0.0), c, p_scale, p_octaves, p_time);
	float hz = wave_height(p + vec2(0.0, e), c, p_scale, p_octaves, p_time);
	return vec2(hx - h, hz - h) / e;
}

// The opaque scene's point under a screen position, in view space (camera space: x right, y up,
// looking down -z). The depth texture holds the raw depth; the inverse projection turns it back.
vec3 scene_view_position(vec2 p_screen_uv, mat4 p_inv_projection) {
	float raw = texture(liquid_depth, p_screen_uv).r;
	vec4 v = p_inv_projection * vec4(p_screen_uv * 2.0 - 1.0, raw, 1.0);
	return v.xyz / v.w;
}

// Refraction: look a little to the side (p_offset, in screen units), but only where what is seen
// there is behind the liquid's surface (p_surface_z, view space): a rock in front of the water
// mustn't smear into it. Falls back to looking straight through.
vec2 refracted_uv(vec2 p_screen_uv, vec2 p_offset, float p_surface_z, mat4 p_inv_projection) {
	vec2 uv = clamp(p_screen_uv + p_offset, vec2(0.001), vec2(0.999));
	return scene_view_position(uv, p_inv_projection).z < p_surface_z ? uv : p_screen_uv;
}

// Cells (Voronoi), with centres that wander in small circles over time (p_time 0: still). Returns
// x: distance to the nearest centre, y: to the second nearest, zw: the nearest cell's square.
vec4 cells(vec2 p, float p_time) {
	vec2 i = floor(p);
	vec2 f = fract(p);
	vec4 best = vec4(8.0, 8.0, 0.0, 0.0);
	for (int y = -1; y <= 1; y++) {
		for (int x = -1; x <= 1; x++) {
			vec2 square = vec2(float(x), float(y));
			vec2 r = hash2(i + square);
			vec2 centre = 0.5 + 0.38 * sin(p_time + 6.2831 * r); // wanders round its spot
			float d = length(square + centre - f);
			if (d < best.x) {
				best.y = best.x;
				best.x = d;
				best.zw = i + square;
			} else if (d < best.y) {
				best.y = d;
			}
		}
	}
	return best;
}

// Caustics: the bright net of light a wavy surface focuses onto the floor. Two layers of wandering
// cells; their borders (y - x near 0) are the bright lines, multiplied so they cross like the real
// thing. 0..1.
float caustics(vec2 p, float p_time) {
	vec4 a = cells(p, p_time * 0.9);
	vec4 b = cells(p * 1.37 + vec2(4.1, 2.3), -p_time * 0.7);
	float la = 1.0 - smoothstep(0.0, 0.12, a.y - a.x);
	float lb = 1.0 - smoothstep(0.0, 0.12, b.y - b.x);
	return clamp(la * 0.6 + lb * 0.6 + la * lb * 1.5, 0.0, 1.0);
}

// Thin-film colour: the rainbow of an oil film or a soap bubble. Light reflected from the film's
// top and bottom adds up or cancels depending on the film's thickness, a different thickness for
// each colour; p_thickness 0..1 runs once round the rainbow.
vec3 thin_film(float p_thickness) {
	return 0.5 + 0.5 * cos(6.2831 * (p_thickness + vec3(0.0, 0.33, 0.67)));
}

// Heat colour: how a hot thing glows as it heats up, 0 cold black-red .. 1 white-hot (a cartoon
// of blackbody light).
vec3 heat_colour(float p_heat) {
	vec3 c = mix(vec3(0.04, 0.0, 0.0), vec3(0.55, 0.03, 0.0), smoothstep(0.0, 0.3, p_heat));
	c = mix(c, vec3(1.0, 0.32, 0.02), smoothstep(0.25, 0.6, p_heat));
	c = mix(c, vec3(1.0, 0.78, 0.22), smoothstep(0.55, 0.85, p_heat));
	return mix(c, vec3(1.0, 0.97, 0.85), smoothstep(0.85, 1.0, p_heat));
}

// Fall threads: noise squeezed across a fall and stretched down it, sliding down faster the further
// it has fallen (slid in sqrt(metres), see the mobile step 4). Three sizes, 0..1.
float fall_threads(vec2 p_uv, float p_per_metre, float p_speed, float p_time) {
	float fallen = max(p_uv.y, 0.0) + 0.25;
	vec2 q = vec2(p_uv.x * p_per_metre, sqrt(fallen) * 2.0 - p_time * p_speed);
	return noise(q) * 0.5 + noise(q * vec2(2.1, 1.7) + 4.3) * 0.3 + noise(q * vec2(4.3, 2.9) + 1.1) * 0.2;
}

#endif
