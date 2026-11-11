// Water, mobile: a cartoon look (after Minions Art's stylised waterfall) from where each pixel is
// and which way it faces in the world, not from UV:
//   world normal   up = the flat water, sideways = a fall (steps 1, 2); the lip where one bends
//                  into the other gets a white band (step 6)
//   triplanar      patterns are laid on the world from three sides (x, y, z) and blended by the
//                  normal (step 4), so no UVs and no seams: streaks fall down the sides (step 5),
//                  wavy lines run on top (steps 7, 8)
//   toon           few flat colours with crisp edges between them: banded light (step 3), the
//                  streaks and lines cut out of noise by thresholds
//   edge foam      the depth texture tells how far the ground is below the surface: white where it
//                  is shallow, round anything standing in the water (step 9)
// From the game's mesh it reads only COLOR: rg the flow direction and b "flows" (the top lines
// run with it, step 8), a where a fall lands (foam, step 10).
// Built step by step in water_mobile/steps/ (the last step is this file).
shader_type spatial;
render_mode cull_disabled, specular_disabled; // falls are seen from both sides; no shiny highlight

#include "res://shader_lib/liquid_noise.gdshaderinc"

uniform vec3 top_color : source_color = vec3(0.13, 0.5, 0.95); // step 2: the flat water
uniform vec3 fall_color : source_color = vec3(0.2, 0.62, 1.0); // step 2: a fall, lighter
uniform float fall_start : hint_range(0.0, 1.0) = 0.6; // step 2: normal.y below this: a fall
uniform float shade : hint_range(0.0, 1.0) = 0.6; // step 3: light on the side away from the sun
uniform float sharpness = 4.0; // step 4: how quickly the three sides hand over
uniform vec3 band_color : source_color = vec3(0.55, 0.85, 1.0); // step 5: the middle toon band
uniform vec3 foam_color : source_color = vec3(0.96, 0.99, 1.0); // step 5: white
uniform float streaks_per_metre = 4.0; // step 5: across a fall
uniform float fall_speed = 2.5; // step 5: metres a second the streaks fall
uniform float lip_width : hint_range(0.0, 0.5) = 0.25; // step 6: how much of the bend is white
uniform float line_scale = 1.2; // step 7: blobs a metre (the lines go round them)
uniform float line_pixels = 2.0; // step 7: how wide the lines are, in pixels
uniform float flow_speed = 0.9; // step 8: metres a second the lines run in a stream
uniform float flow_cycle = 1.6; // step 8: seconds one copy slides before it restarts
uniform float edge_width = 0.12; // step 9: metres of water above the ground that foam
uniform sampler2D depth_texture : hint_depth_texture, filter_nearest, repeat_disable; // step 9

varying vec3 world_pos; // step 1: metres, in the world
varying vec3 world_normal; // step 1: which way the surface faces, in the world

void vertex() {
	world_pos = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;
	world_normal = normalize((MODEL_MATRIX * vec4(NORMAL, 0.0)).xyz);
}

// Step 5: a hard toon edge: 0 below edge, 1 above (a little blend, so it isn't jagged).
float band(float v, float edge) {
	return smoothstep(edge - 0.02, edge + 0.02, v);
}

// Step 5: streaks on one side plane: p.x across (metres), p.y up. Squeezed across, stretched up,
// sliding down with time: 0..1.
float streaks(vec2 p) {
	vec2 q = vec2(p.x * streaks_per_metre, (p.y + TIME * fall_speed) * 0.3);
	return noise(q) * 0.65 + noise(q * vec2(2.3, 1.7) + 5.1) * 0.35;
}

// Steps 7, 8: blobs on the top plane, run along the flow with the flow-map trick: two copies each
// slide for one cycle and restart, half a cycle apart, cross-faded (never stretching at a bend).
float top_noise(vec2 p, vec2 velocity) {
	float phase0 = fract(TIME / flow_cycle);
	float phase1 = fract(TIME / flow_cycle + 0.5);
	float weight0 = 1.0 - abs(1.0 - 2.0 * phase0);
	float n0 = noise((p - velocity * phase0 * flow_cycle) * line_scale);
	float n1 = noise((p - velocity * phase1 * flow_cycle) * line_scale + vec2(3.7, 1.3));
	return mix(n1, n0, weight0);
}

// Step 7: a line where the noise crosses 0.5, line_pixels wide on screen (fwidth: how much the
// noise changes from one pixel to the next).
float contour(float n) {
	float pixels = abs(n - 0.5) / max(fwidth(n), 0.0001);
	return 1.0 - smoothstep(line_pixels - 1.0, line_pixels, pixels);
}

void fragment() {
	vec3 n = normalize(world_normal);
	// step 2: 0 on the flat, 1 down a fall (abs: a fall's back faces point down a little)
	float fall = 1.0 - smoothstep(fall_start - 0.05, fall_start + 0.05, abs(n.y));
	vec3 colour = mix(top_color, fall_color, fall);

	// step 4: how much each side's projection counts: the normal's size along x, y, z, sharpened
	vec3 w = pow(abs(n), vec3(sharpness));
	w /= w.x + w.y + w.z;

	// step 5: streaks down the falls, from the x and z sides, in two toon bands
	if (fall > 0.0) {
		float s = (streaks(world_pos.zy) * w.x + streaks(world_pos.xy) * w.z) / max(w.x + w.z, 0.001);
		colour = mix(colour, band_color, band(s, 0.42) * fall);
		colour = mix(colour, foam_color, band(s, 0.58) * fall);
	}

	// step 6: white where the water bends over the lip
	float lip = band(abs(n.y), fall_start - lip_width) * (1.0 - band(abs(n.y), fall_start));
	colour = mix(colour, foam_color, lip);

	// steps 7, 8: wavy lines on the top, two sizes, running with the flow (a pond just drifts)
	if (fall < 1.0) {
		vec2 direction = COLOR.rg * 2.0 - 1.0;
		vec2 velocity = mix(vec2(0.08, 0.05), direction * flow_speed, clamp(COLOR.b, 0.0, 1.0));
		float lines = max(contour(top_noise(world_pos.xz, velocity)),
				contour(top_noise(world_pos.xz * 1.9 + 11.0, velocity * 1.9)));
		colour = mix(colour, band_color, lines * (1.0 - fall));
	}

	// step 9: edge foam. The depth texture holds, per screen pixel, how far the solid world is;
	// back to a world position, it is the ground under (or behind) this pixel of water.
	float depth = texture(depth_texture, SCREEN_UV).r;
	vec4 view = INV_PROJECTION_MATRIX * vec4(SCREEN_UV * 2.0 - 1.0, depth, 1.0);
	vec3 ground = (INV_VIEW_MATRIX * vec4(view.xyz / view.w, 1.0)).xyz;
	float above = world_pos.y - ground.y; // metres of water over the ground
	float wobble = noise(world_pos.xz * 3.0 + TIME * 0.4);
	float width = edge_width * mix(1.0, 0.35, clamp(COLOR.b, 0.0, 1.0)); // a stream is all shallow
	float edge = (1.0 - band(above, width * (0.5 + wobble))) * (1.0 - fall);
	colour = mix(colour, foam_color, edge);

	// step 10: churning white blobs where a fall lands
	float churn = noise(world_pos.xz * 2.2 + vec2(1.0, -0.7) * TIME * 1.5);
	colour = mix(colour, foam_color, band(COLOR.a * churn, 0.45) * (1.0 - fall));

	ALBEDO = colour;
}

// Step 3: toon light: lit or in shade, a crisp edge between (no smooth falloff).
void light() {
	float lit = smoothstep(-0.05, 0.05, dot(NORMAL, LIGHT)) * ATTENUATION;
	DIFFUSE_LIGHT += mix(shade, 1.0, lit) * LIGHT_COLOR / PI;
}
