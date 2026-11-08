// Green poison, mobile: steps 1 - 5 (liquid_common.gdshaderinc) and its own step 7: bubbles that
// grow and pop, drifting with the flow, and a faint toxic glow. Matte, opaque.
// MCGrid picks this shader when the map's liquid is green poison. Its working copy, and a desktop
// look, are in the Water Shader Steps project (blender/sand_island/water_tutorial/).
shader_type spatial;
render_mode cull_disabled; // falls are seen from both sides

#include "res://material/shaders/terrain/liquid/liquid_common.gdshaderinc"

// Steps 1 - 5: poison's colours and speed.
uniform vec3 deep_color : source_color = vec3(0.2, 0.55, 0.08); // step 1 / 2
uniform vec3 shallow_color : source_color = vec3(0.75, 0.95, 0.3); // step 2: pale, sickly
uniform float flow_speed = 0.35; // step 3: metres a second: thick
uniform vec3 streak_color : source_color = vec3(0.85, 1.0, 0.45); // step 4
uniform vec3 foam_color : source_color = vec3(0.88, 1.0, 0.6); // step 5: pale froth

// Step 7: bubbles (how many squares a metre, each may hold one bubble; the share of squares that
// do; how long one copy drifts) and the glow (faint: it looks toxic, not hot).
uniform float bubbles_per_metre = 2.5;
uniform float bubble_amount : hint_range(0.0, 1.0) = 0.35;
uniform float bubble_cycle = 3.0; // seconds
uniform float poison_glow = 0.3;

// Step 7: one bubble per square of the plane (some squares have none). p_phase (0..1) is how far
// through its cycle this copy is; each bubble is born at a random moment in it, grows (fast at
// first, like a sphere coming up) for most of its life, then pops: a thin ring spreading out and
// fading. All of it is over before the copy restarts (phase 1 -> 0), so the restart never shows.
// Returns x: the bubble (0 outside, brighter at its rim, like a sphere's edge), y: the pop ring.
vec2 bubble(vec2 p, float p_phase) {
	vec2 square = floor(p);
	vec2 f = fract(p); // where in the square, 0..1
	float has = step(hash(square + 5.0), bubble_amount); // this square: a bubble or not
	float birth = 0.05 + hash(square + 9.0) * 0.6; // born between 0.05 and 0.65 of the cycle
	float age = (p_phase - birth) / 0.3; // 0 -> 1 over its life (0.3 of the cycle)
	float alive = step(0.0, age) * step(age, 1.0);
	vec2 centre = 0.35 + hash2(square + 2.0) * 0.3; // near the square's middle, so it stays inside
	float d = length(f - centre);
	// growing (age 0 .. 0.85): radius up to 0.3 of the square, sqrt: quick at first
	float radius = 0.3 * sqrt(clamp(age / 0.85, 0.0, 1.0));
	float disc = 1.0 - smoothstep(radius - 0.03, radius, d);
	float rim = smoothstep(radius * 0.5, radius, d);
	float body = disc * (0.4 + 0.6 * rim) * step(age, 0.85);
	// popped (age 0.85 .. 1): a ring from the bubble's edge out a little, fading
	float popped = clamp((age - 0.85) / 0.15, 0.0, 1.0);
	float ring = (1.0 - smoothstep(0.0, 0.025, abs(d - (0.3 + popped * 0.05)))) * (1.0 - popped) * step(0.85, age);
	return vec2(body, ring) * has * alive;
}

// Step 7: the bubbles carried along the flow: two copies half a cycle apart (as the lava's crust);
// each bubble only lives inside its copy's cycle, so nothing needs fading. Together: max.
vec2 flowing_bubbles(vec2 p, vec2 velocity, float t) {
	float phase0 = fract(t / bubble_cycle);
	float phase1 = fract(t / bubble_cycle + 0.5);
	vec2 b0 = bubble(p - velocity * phase0 * bubble_cycle, phase0);
	vec2 b1 = bubble(p - velocity * phase1 * bubble_cycle + vec2(4.1, 7.7), phase1);
	return max(b0, b1);
}

void vertex() {
	depth = CUSTOM0.x; // step 2: this vertex's depth in metres
}

void fragment() {
	LiquidLook look = LiquidLook(deep_color, shallow_color, flow_speed, streak_color, foam_color);
	LiquidBase base = liquid_base(look, UV, UV2, COLOR, depth, TIME);
	vec3 colour = base.colour;

	// step 7: bubbles on the flat poison (not down the falls or on foam), in its pale foam colour,
	// drifting with the flow (the same velocity as the ripples, step 3)
	vec2 b = flowing_bubbles(UV * bubbles_per_metre, base.velocity * bubbles_per_metre, TIME);
	b *= base.on_flat * (1.0 - base.foam);
	colour = mix(colour, foam_color, b.x * 0.6); // the bubble
	colour = mix(colour, foam_color, b.y); // the pop

	ALBEDO = colour;
	EMISSION = colour * poison_glow; // a faint toxic glow, bubbles a little brighter
	ROUGHNESS = 1.0; // matte
	SPECULAR = 0.0;
}
