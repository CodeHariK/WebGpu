// The mobile liquid shaders' shared steps (lava_mobile/, poison_mobile/; the water has its own): one
// liquid per map, one material, one draw (MCGrid's `liquid` picks the shader; no branching).
// Built one idea per step; each step adds one thing on top of the last:
//   step 1  a colour per liquid (each shader's own uniforms)
//   step 2  depth: shallow colour at the edges, the liquid's own colour where it is deep
//   step 3  ripples: a soft pattern sliding along the flow (rivers), drifting slowly (ponds)
//   step 4  falls: light streaks sliding down, faster the further they have fallen
//   step 5  foam: along the shore, churning where a fall lands, at the bottom of each fall
//   step 6  lava.gdshader only: crust plates and glow; step 7 poison.gdshader only: bubbles, glow
//
// What the mesh gives the shader per vertex (src/marching_cubes/water/build/mc_water_build.cpp):
//   COLOR      the flow (water step 2c): rg the direction (x, z) as 0..1 (0.5 = none), b 1 where
//              it flows, a 1 where a fall lands (foam). The Water tab's "Water view: flow" draws it.
//   UV         metres: world x, z on the flat liquid; on a fall its own frame: x across the fall
//              (along the cliff edge), y down it (0 at the lip)
//   UV2        x: 0 on the flat liquid, 0..1 over a fall's lip, 1 and more down the fall;
//              y: metres above the landing on a fall, -1 on a stream
//   CUSTOM0.x  the depth in metres (surface to ground); a fall carries the depth at its top
// COLOR, UV and UV2 can be read in fragment() directly: the GPU blends them between the vertices.
// Functions can't read those built-ins, so fragment() hands them in.
#ifndef LIQUID_COMMON
#define LIQUID_COMMON

#include "res://shader_lib/liquid_noise.gdshaderinc"

// Step 2: how deep the liquid has to be to reach its own colour.
uniform float depth_range : hint_range(0.1, 4.0) = 1.2; // metres from shallow to the full colour
// Step 3: ripples: how big, how strong (how much they lighten / darken the colour).
uniform float ripple_scale = 1.3; // about this many ripples a metre
uniform float ripple_strength : hint_range(0.0, 0.5) = 0.12;
uniform float flow_cycle = 1.6; // seconds one copy of the pattern slides before it restarts
// Step 4: falls: how many streaks a metre across, how much faster than its flow the liquid falls.
uniform float streaks_per_metre = 9.0;
uniform float fall_speed = 2.0; // x the liquid's speed (step 3), one metre down the fall
// Step 5: foam: how shallow the shore foam starts, how far up a fall its foot foams.
uniform float shore_width = 0.15; // metres of depth that still get shore foam
uniform float landing_height = 0.35; // metres above a fall's landing that foam

// A varying is worked out per vertex and handed to every pixel between the vertices, blended.
// Each shader's vertex() sets it: depth = CUSTOM0.x (step 2).
varying float depth;

// Step 3: two sizes of blobs together look like ripples: 0..1.
float ripples(vec2 p) {
	return noise(p) * 0.65 + noise(p * 2.3 + 7.1) * 0.35;
}

// Step 3: the flow-map trick. Sliding the pattern by velocity * time would stretch it more and more
// where the direction changes (a bend in a river). Instead two copies each slide for one cycle and
// then restart, half a cycle apart; each fades out just as it restarts, so the jump never shows.
float flowing_ripples(vec2 p, vec2 velocity, float t) {
	float phase0 = fract(t / flow_cycle); // 0 -> 1 every cycle
	float phase1 = fract(t / flow_cycle + 0.5); // the same, half a cycle later
	float weight0 = 1.0 - abs(1.0 - 2.0 * phase0); // 0 at copy 0's restart, 1 halfway
	float r0 = ripples(p - velocity * phase0 * flow_cycle);
	float r1 = ripples(p - velocity * phase1 * flow_cycle + vec2(3.7, 1.3)); // offset: not the same blobs
	return mix(r1, r0, weight0);
}

// One liquid's look for steps 1 - 5 (each shader fills it from its own uniforms).
struct LiquidLook {
	vec3 deep; // step 1 / 2: its own colour, where it is deep
	vec3 shallow; // step 2: at the edges
	float speed; // step 3: metres a second it runs
	vec3 streak; // step 4
	vec3 foam; // step 5
};

// Steps 2 - 5's result, and what the liquid's own steps (6, 7) build on.
struct LiquidBase {
	vec3 colour;
	float deepness; // step 2: 0 at the edge .. 1 at depth_range
	vec2 velocity; // step 3: metres a second, along the flow
	float flows; // step 3: 1 in a river, 0 in a pond
	float on_flat; // 1 on the flat liquid, 0 down a fall
	float foam; // step 5: 0..1
};

// Steps 2 - 5 for one pixel. p_uv, p_uv2, p_flow: UV, UV2, COLOR; p_depth: the depth varying.
LiquidBase liquid_base(LiquidLook look, vec2 p_uv, vec2 p_uv2, vec4 p_flow, float p_depth, float p_time) {
	LiquidBase base;
	// step 2: 0 at the edge, rising smoothly to 1 at depth_range metres deep
	base.deepness = smoothstep(0.0, depth_range, p_depth);
	vec3 colour = mix(look.shallow, look.deep, base.deepness);

	// step 3: ripples sliding along the flow
	vec2 direction = p_flow.rg * 2.0 - 1.0; // the flow direction, back to -1..1
	base.flows = clamp(p_flow.b, 0.0, 1.0); // 1 in a river, 0 in a pond
	base.velocity = mix(vec2(0.08, 0.05), direction * look.speed, base.flows); // a pond just drifts
	float r = flowing_ripples(p_uv * ripple_scale, base.velocity * ripple_scale, p_time);
	base.on_flat = 1.0 - smoothstep(0.0, 0.85, p_uv2.x); // not down the falls (step 4 is theirs)
	colour *= 1.0 + (r - 0.5) * 2.0 * ripple_strength * base.on_flat; // lighter / darker

	// step 4: streaks down the falls. Noise squeezed across (many streaks a metre) and stretched
	// down (long threads), sliding down. It slides in sqrt(metres fallen), not metres: a streak at
	// d metres then moves at speed * fall_speed * sqrt(d), so it speeds up as it falls.
	float fallen = max(p_uv.y, 0.0) + 0.25; // + a little: not standing still at the lip
	vec2 q = vec2(p_uv.x * streaks_per_metre, sqrt(fallen) * 2.0 - p_time * look.speed * fall_speed);
	float threads = noise(q) * 0.6 + noise(q * vec2(2.1, 1.7) + 4.3) * 0.4; // two sizes, 0..1
	float bright = smoothstep(0.55, 0.85, threads); // only the brightest threads: crisp streaks
	colour = mix(colour, look.streak, bright * 0.8 * (1.0 - base.on_flat)); // only down the falls

	// step 5: foam, three kinds added together (each 0..1, then capped at 1)
	// a. the shore: shallower than shore_width (wobbling with slow noise); not on a stream
	//    (UV2.y = -1): a stream is a thin sheet, shallow everywhere, so it would be all foam
	float on_stream = clamp(-p_uv2.y, 0.0, 1.0);
	float wobble = noise(p_uv * 4.0 + p_time * 0.6 * look.speed);
	float shore = 1.0 - smoothstep(0.0, shore_width * (0.6 + wobble * 0.8), p_depth);
	shore *= (1.0 - on_stream) * base.on_flat;
	// b. where a fall lands (COLOR.a, from the flow): patches of noise that churn (move fast)
	float churn = noise(p_uv * 2.6 + vec2(1.0, -0.7) * p_time * look.speed * 1.7);
	float landing = smoothstep(0.5, 1.0, p_flow.a) * smoothstep(0.45, 0.75, churn) * base.on_flat;
	// c. the bottom of each fall: UV2.y is metres above the landing; the top edge follows the streaks
	float fall_foot = (1.0 - smoothstep(0.0, landing_height, p_uv2.y - threads * 0.15)) * (1.0 - base.on_flat);
	base.foam = clamp(shore + landing + fall_foot, 0.0, 1.0);
	base.colour = mix(colour, look.foam, base.foam * 0.9);
	return base;
}

#endif
