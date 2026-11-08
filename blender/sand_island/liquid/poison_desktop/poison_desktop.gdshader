// Green poison, desktop: no budget. What it does, in the order fragment() does it:
//   1  swirl      the poison's body: fbm warped by fbm (it swirls, two toxic greens), carried by
//                 the flow, slowly
//   2  murk       thick: only where it is shallow do you see the floor through it (the depth
//                 texture says how much poison the light passes through), bent a little
//   3  film       an oily rainbow film on top (thin-film colour), in drifting patches, strongest
//                 at grazing angles
//   4  bubbles    domes rising and popping, drifting with the flow: a sphere's normal (so the
//                 sun glints on them), a rainbow skin, a ring when they pop
//   5  falls      thick, gooey: slow threads and fat drips sliding down
//   6  froth      pale froth at the shore and where falls land
//   7  light      glossy surface (the engine adds reflections); the body lit, plus a pulsing
//                 toxic glow (EMISSION), brighter in the bubbles and froth
// Desktop look (green poison), no budget: kept here only (the game keeps the simple mobile shaders). It reads
// the MC water mesh's data (COLOR flow, UV, UV2, depth: shader_lib/liquid_common.gdshaderinc);
// on another mesh it compiles but shows the defaults.
shader_type spatial;
render_mode cull_disabled, depth_draw_always; // falls seen from both sides; transparent pass, still writes depth

#include "res://shader_lib/liquid_desktop.gdshaderinc"

// 1 swirl
uniform vec3 toxic_light : source_color = vec3(0.55, 1.0, 0.12);
uniform vec3 toxic_dark : source_color = vec3(0.08, 0.32, 0.03);
uniform float flow_speed = 0.35;
uniform float flow_cycle = 3.0;
uniform float swirl_scale = 0.7;
uniform float warp = 1.4;
// 2 murk
uniform vec3 absorption = vec3(5.0, 1.6, 6.0); // per metre: green gets through best
uniform float min_murk : hint_range(0.0, 1.0) = 0.45; // even the thinnest poison hides this much
uniform float refraction = 0.02;
// 3 film
uniform float film_strength : hint_range(0.0, 1.0) = 0.85;
uniform float film_scale = 0.9;
// 4 bubbles
uniform float bubbles_per_metre = 2.2;
uniform float bubble_amount : hint_range(0.0, 1.0) = 0.4;
uniform float bubble_cycle = 3.5; // seconds
uniform float bubble_size : hint_range(0.1, 0.45) = 0.33; // of a square
// 5 falls
uniform float streaks_per_metre = 6.0;
uniform float fall_speed = 1.4; // x flow_speed, one metre down: slow, gooey
uniform float landing_height = 0.4;
// 6 froth
uniform vec3 froth_color : source_color = vec3(0.85, 1.0, 0.55);
uniform float shore_froth_depth = 0.2; // metres of real depth
// 7 light
uniform float glow = 0.9;
uniform float pulse_speed = 1.3;

varying vec3 world_position;

void vertex() {
	world_position = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;
}

// 4: one bubble per square (some squares have none), born at a random moment of its copy's cycle,
// growing (fast at first), then popping into a ring (see poison_mobile.gdshader's bubble()).
// Returns x: inside the bubble (0..1, soft edge), yz: the dome's slope (for its normal), w: the pop
// ring.
vec4 bubble(vec2 p, float p_phase) {
	vec2 square = floor(p);
	vec2 f = fract(p);
	float has = step(hash(square + 5.0), bubble_amount);
	float birth = 0.05 + hash(square + 9.0) * 0.6;
	float age = (p_phase - birth) / 0.3;
	float alive = step(0.0, age) * step(age, 1.0) * has;
	vec2 centre = 0.5 + (hash2(square + 2.0) - 0.5) * (1.0 - 2.0 * bubble_size) * 0.9;
	vec2 to_pixel = f - centre;
	float d = length(to_pixel);
	float radius = bubble_size * sqrt(clamp(age / 0.85, 0.0, 1.0)) * (0.6 + 0.4 * hash(square + 1.0));
	float inside = (1.0 - smoothstep(radius - 0.02, radius, d)) * step(age, 0.85);
	// a dome: height sqrt(r^2 - d^2), so its slope is -to_pixel / height (steep at the rim)
	float height = sqrt(max(radius * radius - d * d, 1e-4));
	vec2 slope = -to_pixel / height * inside;
	float popped = clamp((age - 0.85) / 0.15, 0.0, 1.0);
	float ring_radius = bubble_size * (0.9 + popped * 0.35);
	float ring = (1.0 - smoothstep(0.0, 0.03, abs(d - ring_radius))) * (1.0 - popped) * step(0.85, age);
	return vec4(inside, slope, ring) * alive;
}

// 4: two copies drifting with the flow, half a cycle apart; whichever bubble is there wins.
vec4 flowing_bubbles(vec2 p, vec2 p_velocity, float p_time) {
	FlowCopies c = flow_copies(p_velocity, p_time, bubble_cycle);
	vec4 a = bubble(p - c.offset0, c.phase0);
	vec4 b = bubble(p - c.offset1, c.phase1);
	return a.x + a.w >= b.x + b.w ? a : b;
}

void fragment() {
	LiquidFlow flow = liquid_flow(UV2, COLOR);
	float falling = 1.0 - flow.on_flat;
	vec2 velocity = mix(vec2(0.05, 0.03), flow.direction * flow_speed, flow.flows);
	vec3 mesh_normal = normalize((INV_VIEW_MATRIX * vec4(NORMAL, 0.0)).xyz);

	// 1 swirl: domain warping in the flow-map trick's two copies
	FlowCopies copies = flow_copies(velocity, TIME, flow_cycle);
	vec2 p0 = (UV - copies.offset0) * swirl_scale;
	vec2 p1 = (UV - copies.offset1) * swirl_scale;
	float s0 = fbm(p0 + vec2(fbm(p0, 3), fbm(p0 + vec2(5.2, 1.3), 3)) * warp, 4);
	float s1 = fbm(p1 + vec2(fbm(p1, 3), fbm(p1 + vec2(5.2, 1.3), 3)) * warp, 4);
	float swirl = mix(s1, s0, copies.weight0);
	vec3 body = mix(toxic_dark, toxic_light, smoothstep(0.3, 0.75, swirl));

	// 5 falls: slow threads and fat drips (cells stretched down the fall) sliding down
	float threads = fall_threads(UV, streaks_per_metre, flow_speed * fall_speed, TIME);
	vec2 dp = vec2(UV.x * 2.5, sqrt(max(UV.y, 0.0) + 0.25) * 2.0 - TIME * flow_speed * fall_speed);
	vec4 drip_cells = cells(dp * vec2(1.0, 2.5), 0.0);
	float drips = (1.0 - smoothstep(0.15, 0.35, drip_cells.x)) * step(hash(drip_cells.zw), 0.35);
	vec3 fall_body = mix(toxic_dark, toxic_light, clamp(threads * 0.8 + drips * 0.6, 0.0, 1.0));
	body = mix(body, fall_body, falling);

	// 4 bubbles (flat poison only): their dome slope tilts the normal
	vec4 bub = flowing_bubbles(UV * bubbles_per_metre, velocity * bubbles_per_metre, TIME) * flow.on_flat;
	// a gentle surface wobble, plus the bubbles, plus the fall's drips
	float wobble_x = fbm(UV * 1.5 + vec2(TIME * 0.1, 0.0), 2) - 0.5;
	float wobble_z = fbm(UV * 1.5 + vec2(3.1, TIME * 0.1), 2) - 0.5;
	vec2 tilt = vec2(wobble_x, wobble_z) * 0.15 + bub.yz * 0.35;
	vec3 normal = normalize(mesh_normal + vec3(-tilt.x, 0.0, -tilt.y) + vec3(0.0, 0.0, drips * 0.2 * falling));
	NORMAL = normalize((VIEW_MATRIX * vec4(normal, 0.0)).xyz);
	float facing = clamp(dot(NORMAL, VIEW), 0.0, 1.0); // 1 looking straight at it, 0 grazing

	// 2 murk: the floor through the poison, only where it is shallow
	vec3 floor_straight = scene_view_position(SCREEN_UV, INV_PROJECTION_MATRIX);
	vec2 shift = NORMAL.xy * refraction * smoothstep(0.0, 0.3, length(floor_straight - VERTEX));
	vec2 uv = refracted_uv(SCREEN_UV, shift, VERTEX.z, INV_PROJECTION_MATRIX);
	vec3 floor_view = scene_view_position(uv, INV_PROJECTION_MATRIX);
	float path = length(floor_view - VERTEX) * flow.on_flat + falling * 4.0; // falls: opaque
	vec3 floor_world = (INV_VIEW_MATRIX * vec4(floor_view, 1.0)).xyz;
	float poison_depth = max(world_position.y - floor_world.y, 0.0);
	vec3 transmitted = exp(-absorption * path) * (1.0 - min_murk);
	float opacity = 1.0 - dot(transmitted, vec3(0.333));
	vec3 behind = texture(liquid_screen, uv).rgb * transmitted;

	// 3 film: rainbow patches, their colour shifting with the angle you look at them
	float thickness = fbm(UV * film_scale + vec2(TIME * 0.05, -TIME * 0.04), 3);
	vec3 film = thin_film(thickness * 2.2 + (1.0 - facing) * 0.6);
	float patches = smoothstep(0.38, 0.62, fbm(UV * film_scale * 0.6 - TIME * 0.03, 3));
	float film_amount = film_strength * patches * (0.35 + 0.65 * (1.0 - facing)) * flow.on_flat;

	// 6 froth at the shore and where falls land, and the foot of each fall
	float near_shore = 1.0 - smoothstep(0.0, shore_froth_depth, poison_depth);
	float froth_noise = smoothstep(0.4, 0.65, fbm(world_position.xz * 3.0 + TIME * 0.15, 3));
	float shore = near_shore * froth_noise * (1.0 - flow.on_stream) * flow.on_flat;
	float churn = fbm(UV * 2.0 + vec2(1.0, -0.7) * TIME * 0.8, 3);
	float landing = flow.landing * smoothstep(0.4, 0.7, churn) * flow.on_flat;
	float foot = (1.0 - smoothstep(0.0, landing_height, UV2.y - threads * 0.25)) * step(0.5, UV2.x) * falling;
	float froth = clamp(shore + landing + foot + bub.w, 0.0, 1.0);

	// 7 light: the body is lit (ALBEDO, as thick as it is opaque); the floor seen through it glows
	// through (EMISSION: it was lit already); the film and bubbles sit on top; a pulsing glow
	vec3 colour = mix(body, body * film * 1.6, film_amount);
	vec3 bubble_skin = mix(froth_color, thin_film(hash(floor(UV * bubbles_per_metre)) + (1.0 - facing)), 0.5);
	colour = mix(colour, bubble_skin, bub.x * 0.55);
	colour = mix(colour, froth_color, froth);
	float pulse = 0.75 + 0.25 * sin(TIME * pulse_speed + swirl * 8.0);
	ALBEDO = colour * opacity;
	EMISSION = behind * (1.0 - froth) + colour * glow * pulse * (0.35 + 0.65 * smoothstep(0.4, 0.8, swirl)) * 0.5;
	ROUGHNESS = mix(0.12, 0.7, froth);
	SPECULAR = 0.5;
}
