// Slime: a thick, glowing green goo of lumpy pillows split by bright veins, with bubbles that grow
// and pop, all creeping along the flow. The look comes from two textures baked in Blender
// (tools/bake_slime.py, its node materials in slime_bake.blend), not from noise worked out per
// pixel, so it is cheap: 2 lookups in vertex(), 4 in fragment(). Opaque, one draw.
//
// The textures (both seamless; one tile = tile_metres of slime):
//   slime_pattern.png  R height (pillows, low in the veins)  G veins  B bubbles (1 at a centre ..
//                      0 at the rim)  A each bubble's phase (when it is born)
//   slime_slopes.png   RG height's slope (for the normal)  BA the way out of a bubble (its dome)
//
// What it does, in order:
//   1  flow      the pattern creeps along the flow (COLOR), with the flow-map trick: two copies
//                each slide for one cycle and restart, half a cycle apart, cross-faded
//   2  bulge     vertex(): the pillows push the surface up (the mesh must be dense: ~0.1 m), and
//                breathe slowly
//   3  surface   the pillows' slope tilts the normal (lit like lumps); tops lighter, valleys dark
//   4  veins     the valleys glow, a pulse running across them
//   5  bubbles   per copy (not cross-faded: they pop in): born at their phase, growing, then
//                popping; a dome normal, a dark rim, a bright core
//   6  light     wet and glossy; a faint glow everywhere, bright in the veins and bubble cores
//                (EMISSION: it still glows at night)
//
// The mesh data it reads (the same as the minecraft game's MC water mesh, see ../README.md):
//   COLOR  the flow: rg the direction (x, z) as 0..1, b 1 where it flows (0: a pond, it drifts)
//   UV     metres: world x, z on the flat; on a fall x across, y down from the lip
//   UV2.x  0 on the flat, 1+ down a fall (a fall slides the pattern straight down)
shader_type spatial;
render_mode cull_disabled; // falls are seen from both sides

// The baked textures (slime.tres sets them).
uniform sampler2D pattern : hint_default_black, filter_linear_mipmap, repeat_enable;
uniform sampler2D slopes : hint_default_white, filter_linear_mipmap, repeat_enable;
uniform float tile_metres = 4.5; // one texture tile covers this much slime
// slime_slopes.png keeps the height slope as sign * sqrt(|slope| / range): bake_slime.py's
// SLOPE_RANGE (height 0..1 per tile)
const float SLOPE_RANGE = 40.0;

// 1 flow
uniform float flow_speed = 0.35; // metres a second, in a river
uniform vec2 pond_drift = vec2(0.05, 0.03); // metres a second, where it doesn't flow
uniform float fall_speed = 2.5; // x flow_speed, down a fall
uniform float flow_cycle = 3.0; // seconds one copy slides before it restarts (and a bubble lives)
// 2 bulge
uniform float bulge = 0.07; // metres from a valley to a pillow's top
uniform float breathe_speed = 0.7;
// 3 surface
uniform vec3 body_dark : source_color = vec3(0.16, 0.42, 0.02);
uniform vec3 body_light : source_color = vec3(0.58, 0.93, 0.1);
uniform float bumpiness = 3.0; // x the bulge's real slope (the lumps catch more light)
// 4 veins
uniform vec3 vein_color : source_color = vec3(0.86, 1.0, 0.3);
uniform float vein_glow = 1.6;
uniform float pulse_speed = 1.8;
// 5 bubbles
uniform vec3 bubble_color : source_color = vec3(0.8, 1.0, 0.25);
uniform float bubble_glow = 2.5;
// 6 light
uniform float body_glow = 0.12;

varying vec2 velocity; // tiles a second, along the flow (worked out per vertex)
varying float on_flat; // 1 on the flat slime, 0 down a fall

// 1: the two copies of the flow-map trick: where each has slid to, how far through its cycle it
// is, and copy 0's weight (0 when it restarts, 1 halfway: copy 1 restarts then).
struct Copies {
	vec2 offset0;
	vec2 offset1;
	float phase0;
	float phase1;
	float weight0;
};

Copies copies(vec2 p_velocity, float p_time, float p_cycle) {
	Copies c;
	c.phase0 = fract(p_time / p_cycle);
	c.phase1 = fract(p_time / p_cycle + 0.5);
	c.weight0 = 1.0 - abs(1.0 - 2.0 * c.phase0);
	c.offset0 = p_velocity * c.phase0 * p_cycle;
	c.offset1 = p_velocity * c.phase1 * p_cycle + vec2(0.37, 0.61); // not the same pillows
	return c;
}

// 1: the flow from the mesh: along COLOR on the flat (drifting where it doesn't flow), straight
// down a fall (UV.y runs down it), in tiles a second.
vec2 slime_velocity(vec4 p_flow, float p_on_flat) {
	vec2 direction = p_flow.rg * 2.0 - 1.0;
	vec2 flat_velocity = mix(pond_drift, direction * flow_speed, clamp(p_flow.b, 0.0, 1.0));
	vec2 fall_velocity = vec2(0.0, flow_speed * fall_speed);
	return mix(fall_velocity, flat_velocity, p_on_flat) / tile_metres;
}

// 3: the stored height slope (0..1 each, round 0.5) back to height (0..1) per tile.
vec2 height_slope(vec2 p_stored) {
	vec2 e = p_stored * 2.0 - 1.0;
	return e * abs(e) * SLOPE_RANGE;
}

// 5: one copy's bubble at this pixel. p_dome: B (1 at the centre .. 0 at the rim, falling evenly
// with distance), p_phase: A, p_out: the way out from the centre, p_copy_phase: the copy's cycle.
// It is born at its phase, grows (fast at first) to its full size, and pops near the cycle's end,
// before the copy restarts. Returns x: inside (0..1), y: rim, z: core, w: unused; and the dome's
// tilt through p_tilt.
vec4 bubble(float p_dome, float p_phase, vec2 p_out, float p_copy_phase, out vec2 p_tilt) {
	float birth = 0.02 + p_phase * 0.25; // all born in the first quarter of the cycle ..
	float age = (p_copy_phase - birth) / 0.7; // .. live 0.7 of it (0 born .. 1 popped), gone by its end
	float alive = step(0.0, age) * step(age, 1.0);
	float grow = sqrt(clamp(age / 0.8, 0.0, 1.0)); // 0 .. 1 by age 0.8
	float edge = 1.0 - grow; // inside where the dome is above this
	float inside = smoothstep(edge, edge + 0.05, p_dome) * (1.0 - smoothstep(0.85, 1.0, age)) * alive;
	// how far out, 0 at the centre .. 1 at the bubble's rim now (the dome falls evenly)
	float r = clamp((1.0 - p_dome) / max(grow, 1e-3), 0.0, 1.0);
	float rim = smoothstep(0.6, 0.95, r) * inside;
	float core = (1.0 - smoothstep(0.0, 0.6, r)) * inside;
	// a sphere's slope: r / sqrt(1 - r^2), outwards (capped: the rim isn't vertical)
	p_tilt = p_out * (r / sqrt(max(1.0 - r * r, 0.08))) * inside;
	return vec4(inside, rim, core, 0.0);
}

void vertex() {
	// 1 the flow (handed to fragment(): worked out once per vertex)
	on_flat = 1.0 - smoothstep(0.0, 0.85, UV2.x);
	velocity = slime_velocity(COLOR, on_flat);

	// 2 bulge: the pillows' height (the same two copies fragment() uses), pushed out along the
	// normal, around its middle (0.6), breathing slowly
	Copies c = copies(velocity, TIME, flow_cycle);
	vec2 p = UV / tile_metres;
	float h0 = textureLod(pattern, p - c.offset0, 0.0).r;
	float h1 = textureLod(pattern, p - c.offset1, 0.0).r;
	float h = mix(h1, h0, c.weight0);
	float breathe = 0.8 + 0.2 * sin(TIME * breathe_speed + (UV.x + UV.y) * 0.6);
	VERTEX += NORMAL * (h - 0.6) * bulge * breathe;
}

void fragment() {
	vec2 p = UV / tile_metres;

	// 1 the two copies of the pattern
	Copies c = copies(velocity, TIME, flow_cycle);
	vec4 a0 = texture(pattern, p - c.offset0);
	vec4 a1 = texture(pattern, p - c.offset1);
	vec4 s0 = texture(slopes, p - c.offset0);
	vec4 s1 = texture(slopes, p - c.offset1);
	float h = mix(a1.r, a0.r, c.weight0);
	float veins = mix(a1.g, a0.g, c.weight0);

	// 3 surface: the height slope (per tile) -> metres per metre, as bulged, made bumpier
	vec2 slope = mix(height_slope(s1.rg), height_slope(s0.rg), c.weight0) * bulge / tile_metres * bumpiness;

	// 5 bubbles: each copy's own, living through its cycle (popped before it restarts); the copy
	// with more bubble at this pixel wins (no cross-fade: no half-bubbles)
	vec2 tilt0;
	vec2 tilt1;
	vec4 bub0 = bubble(a0.b, a0.a, s0.ba * 2.0 - 1.0, c.phase0, tilt0);
	vec4 bub1 = bubble(a1.b, a1.a, s1.ba * 2.0 - 1.0, c.phase1, tilt1);
	bool first = bub0.x >= bub1.x;
	vec4 bub = (first ? bub0 : bub1) * on_flat;
	vec2 bubble_tilt = (first ? tilt0 : tilt1) * on_flat;

	// 3 the normal: the mesh's, tilted by the pillows (and bubbles) on the flat. UV runs along
	// world x, z there, so the slope along UV is the slope along x, z.
	vec3 mesh_normal = normalize((INV_VIEW_MATRIX * vec4(NORMAL, 0.0)).xyz);
	vec2 tilt = mix(slope, bubble_tilt * 0.6, bub.x) * on_flat;
	vec3 normal = normalize(mesh_normal + vec3(-tilt.x, 0.0, -tilt.y));
	NORMAL = normalize((VIEW_MATRIX * vec4(normal, 0.0)).xyz);

	// 3 colour: valleys dark, pillow tops light
	vec3 colour = mix(body_dark, body_light, smoothstep(0.1, 0.95, h));

	// 4 veins: a pulse runs across them (a slow wave in world space, along the flow)
	float wave = dot(UV, velocity * tile_metres) * 1.5 + dot(UV, vec2(0.45, 0.3));
	float pulse = 0.7 + 0.3 * sin(TIME * pulse_speed - wave * 2.0);
	colour = mix(colour, vein_color, veins * 0.85);

	// 5 bubbles: a lighter skin, a darker rim, a bright core
	colour = mix(colour, bubble_color, bub.x * 0.8);
	colour *= 1.0 - bub.y * 0.55;

	// 6 light: wet and glossy (bubbles glossier); glow in the veins, bubble cores, a little all over
	ALBEDO = colour;
	ROUGHNESS = mix(0.28, 0.12, bub.x);
	SPECULAR = 0.5;
	EMISSION = colour * body_glow + vein_color * veins * veins * vein_glow * pulse
			+ bubble_color * bub.z * bubble_glow;
}
