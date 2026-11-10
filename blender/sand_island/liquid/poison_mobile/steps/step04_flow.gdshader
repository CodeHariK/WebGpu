// STEP 4 — look at the flow: which way does the poison run? (C in the viewer: one channel at a time)
// The vertex colour (COLOR) carries the flow, worked out by the game's water system, one number
// per channel, each 0..1:
//   r  how much it runs along x: 0.5 = not at all, 1 = toward +x, 0 = toward -x
//   g  the same along z (1 = toward +z)
//   b  1 where it flows (a stream), 0 where it stands (a pond)
//   a  1 where a fall lands (step 9 puts churning foam there)
// The direction is stored as 0..1 because a colour can't be negative: * 2 - 1 gets back -1..1.
// show_channel picks the view (debug, unshaded): "all" shows r as red and g as green, so a stream
// changes colour where it bends and a pond (b = 0) is dark; one channel alone is grey (0 black,
// 0.5 mid grey, 1 white); "dots" sets dots moving the way it runs (tinted as "all"; still in a
// pond). The dots slide with step 7's trick (two copies, each restarting, cross-faded): sliding
// them for ever would bunch them up where the direction changes, the problem step 6 shows.
shader_type spatial;
render_mode cull_disabled, unshaded;

uniform int show_channel : hint_enum("all", "r: along x", "g: along z", "b: flows", "a: a fall lands", "dots: which way") = 0;  // NEW

varying vec2 world_xz;  // NEW: for the dots: metres on the ground plane

void vertex() {
	world_xz = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xz;  // NEW: the vertex in world space
}

void fragment() {
	vec2 direction = COLOR.rg * 2.0 - 1.0;  // NEW: back to -1..1
	float flows = clamp(COLOR.b, 0.0, 1.0);  // NEW: 1 in a stream, 0 in a pond
	vec3 view = vec3(direction * 0.5 + 0.5, 0.2) * (0.25 + 0.75 * flows);  // NEW: all: red = x, green = z
	if (show_channel == 1) {
		view = vec3(COLOR.r);  // NEW: grey: 0.5 still, brighter toward +x, darker toward -x
	} else if (show_channel == 2) {
		view = vec3(COLOR.g);  // NEW: the same along z
	} else if (show_channel == 3) {
		view = vec3(flows);  // NEW: white in a stream, black in a pond
	} else if (show_channel == 4) {
		view = vec3(COLOR.a);  // NEW: white where a fall lands
	} else if (show_channel == 5) {
		// NEW: dots two a metre, slid along the flow for one second, then back (two copies, half a
		// second apart, cross-faded: step 7 explains it); a pond's dots stand still
		vec2 velocity = direction * 0.8 * flows; // metres a second
		float phase0 = fract(TIME);
		float phase1 = fract(TIME + 0.5);
		float weight0 = 1.0 - abs(1.0 - 2.0 * phase0);
		vec2 p0 = fract((world_xz - velocity * phase0) * 2.0) - 0.5;
		vec2 p1 = fract((world_xz - velocity * phase1) * 2.0 + 0.5) - 0.5;
		float dot0 = 1.0 - smoothstep(0.12, 0.18, length(p0));
		float dot1 = 1.0 - smoothstep(0.12, 0.18, length(p1));
		view = mix(view, vec3(1.0, 1.0, 0.85), mix(dot1, dot0, weight0));
	}
	ALBEDO = view;
}
