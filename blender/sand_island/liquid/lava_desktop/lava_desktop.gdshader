// Lava, desktop: no budget. What it does, in the order fragment() does it:
//   1  heat       a heat field: fbm warped by fbm ("domain warping": the noise's own shapes bend
//                 it, so it swirls like a thick fluid), carried by the flow; hotter where it runs
//                 and deep, cooler at the edges, slowly pulsing
//   2  colour     heat -> the colour a hot thing glows (dark red .. orange .. yellow .. white)
//   3  crust      cooled plates (cells) drifting with the flow, popping in / melting away one by
//                 one (the mobile lava's trick); raised, with bevelled edges for the light
//   4  embers     the plates' edges still glow, and thin cracks inside the plates glow too
//   5  falls      molten and hot, threads sliding down, bits of crust tumbling with them,
//                 a white-hot splash where it lands
//   6  light      molten lava: HDR EMISSION (brighter than white: the scene's glow turns it into
//                 bloom), a little glossy; the crust: rough dark rock, lit by the sun
// Desktop look (lava), no budget: kept here only (the game keeps the simple mobile shaders). It reads
// the MC water mesh's data (COLOR flow, UV, UV2, depth: shader_lib/liquid_common.gdshaderinc);
// on another mesh it compiles but shows the defaults.
shader_type spatial;
render_mode cull_disabled; // falls are seen from both sides

#include "res://shader_lib/liquid_desktop.gdshaderinc"

// 1 heat
uniform float flow_speed = 0.25; // metres a second along a river
uniform float flow_cycle = 4.0; // seconds per flow-map copy
uniform float heat_scale = 0.55; // swirls a metre
uniform float warp = 1.6; // how much the noise bends itself
uniform float edge_cooling = 0.5; // metres of depth over which the edge cools
uniform float pulse_speed = 0.8;
// 2 colour / 6 light
uniform float glow = 2.5; // the molten lava's light (HDR: > 1 blooms)
// 3 crust
uniform vec3 rock_dark : source_color = vec3(0.05, 0.035, 0.03);
uniform vec3 rock_light : source_color = vec3(0.2, 0.13, 0.1);
uniform float crust_scale = 1.1; // plates a metre
uniform float crust_cover : hint_range(0.0, 1.0) = 0.85;
uniform float crack_width : hint_range(0.01, 0.4) = 0.07;
uniform float bevel = 0.18; // how far in from a plate's edge it rises to full height
uniform float crust_cycle = 6.0; // seconds
// 4 embers
uniform float ember_glow = 2.5;
uniform float fissure_scale = 3.5; // fine cracks a metre, inside the plates
// 5 falls
uniform float streaks_per_metre = 7.0;
uniform float fall_speed = 2.5; // x flow_speed, one metre down
uniform float landing_height = 0.5; // metres above the landing that splash

varying float depth;

void vertex() {
	depth = CUSTOM0.x; // metres
}

// 3: the crust's height at p: cells; a plate rises from its cracks over `bevel` to full height.
// Each plate is crust or not (p_cover), and shows only for part of its copy's life (p_life):
// see lava_mobile.gdshader's crust(). Returns x: height 0..1, y: distance into the plate
// (for the embers), z: the plate's random shade.
vec3 crust_at(vec2 p, float p_cover, float p_life) {
	vec4 c = cells(p, 0.0);
	float edge = c.y - c.x; // 0 on a crack
	float crusted = step(hash(c.zw + 17.0), p_cover);
	float life = hash(c.zw + 41.0);
	float shows = smoothstep(life - 0.06, life + 0.06, p_life);
	float height = smoothstep(crack_width, crack_width + bevel, edge) * crusted * shows;
	return vec3(height, edge, hash(c.zw + 3.0));
}

// 3: the crust carried along the flow: two copies (as the mobile lava), the higher of the two wins.
vec3 flowing_crust(vec2 p, FlowCopies c, float p_cover) {
	vec3 a = crust_at(p - c.offset0, p_cover, c.weight0);
	vec3 b = crust_at(p - c.offset1, p_cover, 1.0 - c.weight0);
	return a.x >= b.x ? a : b;
}

void fragment() {
	LiquidFlow flow = liquid_flow(UV2, COLOR);
	float falling = 1.0 - flow.on_flat;
	vec2 velocity = mix(vec2(0.05, 0.03), flow.direction * flow_speed, flow.flows); // ponds creep
	vec3 mesh_normal = normalize((INV_VIEW_MATRIX * vec4(NORMAL, 0.0)).xyz);
	float deepness = smoothstep(0.0, edge_cooling, depth);

	// 1 heat: domain warping in the flow-map trick's two copies
	FlowCopies copies = flow_copies(velocity, TIME, flow_cycle);
	vec2 p0 = (UV - copies.offset0) * heat_scale;
	vec2 p1 = (UV - copies.offset1) * heat_scale;
	vec2 bend0 = vec2(fbm(p0, 3), fbm(p0 + vec2(5.2, 1.3), 3));
	vec2 bend1 = vec2(fbm(p1, 3), fbm(p1 + vec2(5.2, 1.3), 3));
	float swirl = mix(fbm(p1 + bend1 * warp, 4), fbm(p0 + bend0 * warp, 4), copies.weight0);
	float pulse = 0.5 + 0.5 * sin(TIME * pulse_speed + swirl * 9.0); // waves of heat through it
	float heat = smoothstep(0.3, 0.75, swirl) * 0.65 + pulse * 0.15 + flow.flows * 0.15;
	heat *= mix(0.45, 1.0, deepness); // the edges are cooler

	// 5 falls: hotter, threads sliding down, a white-hot splash at the foot
	float threads = fall_threads(UV, streaks_per_metre, flow_speed * fall_speed, TIME);
	float fall_heat = 0.42 + threads * 0.35; // hot, not white: the foot's splash is the white-hot part
	float foot = 1.0 - smoothstep(0.0, landing_height, UV2.y - threads * 0.3);
	fall_heat = mix(fall_heat, 1.0, foot * step(0.5, UV2.x));
	heat = mix(heat, fall_heat, falling);
	// where a fall lands on the flat: churning, white-hot
	float churn = fbm(UV * 2.0 + vec2(1.0, -0.7) * TIME * 0.6, 3);
	heat = max(heat, flow.landing * smoothstep(0.35, 0.7, churn) * flow.on_flat);

	// 2 colour
	vec3 molten = heat_colour(clamp(heat, 0.0, 1.0));

	// 3 crust: fewer plates where it is deep or runs; none on a landing; plates tumbling down the
	// falls too (cells stretched down the fall)
	float cover = crust_cover * mix(1.0, 0.6, deepness) * (1.0 - 0.45 * flow.flows) * (1.0 - flow.landing);
	FlowCopies crust_copies = flow_copies(velocity, TIME, crust_cycle);
	vec2 cp = UV * crust_scale;
	vec3 crust = flowing_crust(cp, crust_copies, cover);
	// the crust's bevel tilts the normal: height a step to each side (finite differences)
	float e = 0.02;
	float hx = flowing_crust(cp + vec2(e, 0.0), crust_copies, cover).x;
	float hz = flowing_crust(cp + vec2(0.0, e), crust_copies, cover).x;
	vec2 crust_slope = vec2(hx - crust.x, hz - crust.x) / e * 0.08;
	// down the falls: dark bits sliding with the threads
	vec2 fp = vec2(UV.x * 1.5, sqrt(max(UV.y, 0.0) + 0.25) * 2.0 - TIME * flow_speed * fall_speed) * 1.6;
	vec4 bits = cells(fp, 0.0);
	float fall_crust = smoothstep(0.25, 0.35, bits.y - bits.x) * step(hash(bits.zw), 0.25) * (1.0 - foot);
	float crusted = mix(crust.x, fall_crust, falling);

	// 4 embers: a plate's edge still glows; thin fissures glow inside the plates
	float ember = exp(-max(crust.y - crack_width, 0.0) * 18.0) * step(0.01, crust.x);
	vec4 fissure_cells = cells(UV * fissure_scale + crust.z * 13.0, 0.0);
	float fissure = 1.0 - smoothstep(0.0, 0.05, fissure_cells.y - fissure_cells.x);
	fissure *= smoothstep(0.4, 1.0, crust.x) * step(0.55, crust.z); // some plates only
	float embers = clamp(ember * 0.8 + fissure * 0.6, 0.0, 1.0) * flow.on_flat;

	// 6 light
	vec3 rock = mix(rock_dark, rock_light, fbm(UV * 6.0 + crust.z * 7.0, 3) * 0.8 + crust.z * 0.2);
	vec3 molten_light = molten * glow * (0.3 + heat * heat);
	vec3 ember_light = heat_colour(0.55 + embers * 0.3) * ember_glow * embers;
	vec3 slope_tilt = vec3(-crust_slope.x, 0.0, -crust_slope.y) * flow.on_flat; // molten lava: smooth
	vec3 normal = normalize(mesh_normal + slope_tilt * crusted);
	NORMAL = normalize((VIEW_MATRIX * vec4(normal, 0.0)).xyz);

	ALBEDO = mix(molten * 0.25, rock, crusted);
	EMISSION = mix(molten_light, ember_light, crusted);
	ROUGHNESS = mix(0.35, 0.92, crusted);
	SPECULAR = mix(0.45, 0.2, crusted);
}
