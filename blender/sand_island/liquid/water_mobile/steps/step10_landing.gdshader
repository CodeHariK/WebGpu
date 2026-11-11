// STEP 10 — where a fall lands: churning white blobs. The finished water.
// COLOR.a is 1 on the pond where a fall lands (the game works it out per 1 m column, so it is
// blocky). Times a noise sliding fast across the surface, cut with a toon band: white blobs that
// come and go, only where the fall hits. This is water_mobile.gdshader. Per pixel: 6 noises on
// the flat water (4 for the lines, 1 wobble, 1 churn), 4 down a fall, one depth read; no textures,
// no UV.
shader_type spatial;
render_mode cull_disabled, specular_disabled;

#include "res://shader_lib/liquid_noise.gdshaderinc"

uniform vec3 top_color : source_color = vec3(0.13, 0.5, 0.95);  // the flat water
uniform vec3 fall_color : source_color = vec3(0.2, 0.62, 1.0);  // a fall, lighter
uniform float fall_start : hint_range(0.0, 1.0) = 0.6;  // normal.y below this: a fall
uniform float shade : hint_range(0.0, 1.0) = 0.6;  // light on the side away from the sun
uniform float sharpness = 4.0;  // how quickly the three sides hand over
uniform vec3 band_color : source_color = vec3(0.55, 0.85, 1.0);  // the middle toon band
uniform vec3 foam_color : source_color = vec3(0.96, 0.99, 1.0);  // white
uniform float streaks_per_metre = 4.0;  // across a fall
uniform float fall_speed = 2.5;  // metres a second the streaks fall
uniform float lip_width : hint_range(0.0, 0.5) = 0.25;  // how much of the bend is white
uniform float line_scale = 1.2;  // blobs a metre (the lines go round them)
uniform float line_pixels = 2.0;  // how wide the lines are, in pixels
uniform float flow_speed = 0.9;  // metres a second the lines run in a stream
uniform float flow_cycle = 1.6;  // seconds one copy slides before it restarts
uniform float edge_width = 0.12;  // metres of water above the ground that foam
uniform sampler2D depth_texture : hint_depth_texture, filter_nearest, repeat_disable;  // the solid world's depth

varying vec3 world_pos;
varying vec3 world_normal;

void vertex() {
	world_pos = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;
	world_normal = normalize((MODEL_MATRIX * vec4(NORMAL, 0.0)).xyz);
}

// A hard toon edge: 0 below edge, 1 above (a little blend, so it isn't jagged).
float band(float v, float edge) {
	return smoothstep(edge - 0.02, edge + 0.02, v);
}

// Streaks on one side view: p.x across (metres), p.y up. Squeezed across, stretched up, sliding
// down with time: 0..1.
float streaks(vec2 p) {
	vec2 q = vec2(p.x * streaks_per_metre, (p.y + TIME * fall_speed) * 0.3);
	return noise(q) * 0.65 + noise(q * vec2(2.3, 1.7) + 5.1) * 0.35;
}

// Blobs on the top view, run along the flow: two copies, each sliding for one cycle and
// restarting, half a cycle apart, cross-faded.
float top_noise(vec2 p, vec2 velocity) {
	float phase0 = fract(TIME / flow_cycle);  // 0 -> 1 every cycle
	float phase1 = fract(TIME / flow_cycle + 0.5);  // the same, half a cycle later
	float weight0 = 1.0 - abs(1.0 - 2.0 * phase0);  // 0 as copy 0 jumps back, 1 half way
	float n0 = noise((p - velocity * phase0 * flow_cycle) * line_scale);
	float n1 = noise((p - velocity * phase1 * flow_cycle) * line_scale + vec2(3.7, 1.3));  // other blobs
	return mix(n1, n0, weight0);
}

// A line where the noise crosses 0.5, line_pixels wide on screen.
float contour(float n) {
	float pixels = abs(n - 0.5) / max(fwidth(n), 0.0001);  // pixels from the line
	return 1.0 - smoothstep(line_pixels - 1.0, line_pixels, pixels);
}

void fragment() {
	vec3 n = normalize(world_normal);
	float fall = 1.0 - smoothstep(fall_start - 0.05, fall_start + 0.05, abs(n.y));
	vec3 colour = mix(top_color, fall_color, fall);

	vec3 w = pow(abs(n), vec3(sharpness));
	w /= w.x + w.y + w.z;

	if (fall > 0.0) {
		// the x and z side views only (the top one is the flat water's), weighted
		float s = (streaks(world_pos.zy) * w.x + streaks(world_pos.xy) * w.z) / max(w.x + w.z, 0.001);
		colour = mix(colour, band_color, band(s, 0.42) * fall);  // light band
		colour = mix(colour, foam_color, band(s, 0.58) * fall);  // white threads
	}

	float lip = band(abs(n.y), fall_start - lip_width) * (1.0 - band(abs(n.y), fall_start));  // a slice of the bend
	colour = mix(colour, foam_color, lip);

	if (fall < 1.0) {  // not where it is all fall
		vec2 direction = COLOR.rg * 2.0 - 1.0;  // the flow, -1..1
		vec2 velocity = mix(vec2(0.08, 0.05), direction * flow_speed, clamp(COLOR.b, 0.0, 1.0));  // a pond drifts
		float lines = max(contour(top_noise(world_pos.xz, velocity)),
				contour(top_noise(world_pos.xz * 1.9 + 11.0, velocity * 1.9)));  // two sizes
		colour = mix(colour, band_color, lines * (1.0 - fall));
	}

	float depth = texture(depth_texture, SCREEN_UV).r;  // how far the solid world is, this pixel
	vec4 view = INV_PROJECTION_MATRIX * vec4(SCREEN_UV * 2.0 - 1.0, depth, 1.0);  // camera space
	vec3 ground = (INV_VIEW_MATRIX * vec4(view.xyz / view.w, 1.0)).xyz;  // the world
	float above = world_pos.y - ground.y;  // metres of water over the ground
	float wobble = noise(world_pos.xz * 3.0 + TIME * 0.4);  // a slow wobble along the edge
	float width = edge_width * mix(1.0, 0.35, clamp(COLOR.b, 0.0, 1.0));  // a stream is all shallow
	float edge = (1.0 - band(above, width * (0.5 + wobble))) * (1.0 - fall);  // the flat only
	colour = mix(colour, foam_color, edge);

	float churn = noise(world_pos.xz * 2.2 + vec2(1.0, -0.7) * TIME * 1.5);  // NEW: fast-moving blobs
	colour = mix(colour, foam_color, band(COLOR.a * churn, 0.45) * (1.0 - fall));  // NEW: where a fall lands

	ALBEDO = colour;
}

void light() {
	float lit = smoothstep(-0.05, 0.05, dot(NORMAL, LIGHT)) * ATTENUATION;
	DIFFUSE_LIGHT += mix(shade, 1.0, lit) * LIGHT_COLOR / PI;
}
