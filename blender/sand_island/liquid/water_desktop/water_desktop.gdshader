// Water, desktop: no budget. What it does, in the order fragment() does it:
//   1  waves      flowing fbm turned into a normal (bumps for the light), carried by the flow
//   2  behind     the floor under each pixel from the depth texture: how far the light travels
//                 through the water, and how deep the water really is there
//   3  refraction look at the floor a little to the side, bent by the waves
//   4  absorption Beer-Lambert: red light dies first, then green, so deeper water turns blue,
//                 plus light scattered back by the water itself (its own colour when deep, lit)
//   5  caustics   the bright net of light the waves focus onto the floor, fading with depth
//   6  falls      thin and full of air: see-through streaks, grooves for the light, white foot
//   7  foam       bands rolling in to the shore, churning where falls land, lines along rivers
//   8  light      the light seen through the water is EMISSION (it was lit already, behind); the
//                 water's own colour, the falls and the foam are ALBEDO (lit by the sun / moon, so
//                 dark at night); the surface is glossy (low roughness), so the engine adds the
//                 sky's reflection and sun glints, stronger at grazing angles (fresnel)
// Desktop look (water), no budget: kept here only (the game keeps the simple mobile shaders). It reads
// the MC water mesh's data (COLOR flow, UV, UV2, depth: shader_lib/liquid_common.gdshaderinc);
// on another mesh it compiles but shows the defaults.
shader_type spatial;
render_mode cull_disabled, depth_draw_always; // falls seen from both sides; transparent pass, still writes depth

#include "res://shader_lib/liquid_desktop.gdshaderinc"

// 1 waves
uniform float flow_speed = 0.9; // metres a second along a river
uniform float flow_cycle = 1.6; // seconds per flow-map copy
uniform float wave_scale = 1.4; // bumps a metre
uniform float wave_strength = 0.22; // how much they tilt the surface
// 3 refraction
uniform float refraction = 0.035; // screen widths the floor shifts at full tilt
// 4 absorption
uniform vec3 absorption = vec3(2.0, 0.9, 0.45); // per metre of water: red, green, blue (cartoon: strong)
uniform vec3 scatter_color : source_color = vec3(0.02, 0.22, 0.4); // deep water's own glow
uniform float scatter_density = 1.4; // per metre
// 5 caustics
uniform float caustic_scale = 1.3;
uniform float caustic_strength = 0.9;
uniform float caustic_speed = 1.2;
// 6 falls
uniform vec3 fall_color : source_color = vec3(0.7, 0.9, 1.0); // full of air: pale
uniform float streaks_per_metre = 11.0;
uniform float fall_speed = 2.2; // x flow_speed, one metre down
uniform float fall_clarity : hint_range(0.0, 1.0) = 0.4; // how much of what's behind shows through
uniform float landing_height = 0.45; // metres above the landing that foam
// 7 foam
uniform vec3 foam_color : source_color = vec3(0.95, 0.99, 1.0);
uniform float shore_foam_depth = 0.35; // metres of real depth that still get shore foam
uniform float foam_bands = 4.0; // bands a metre of depth
uniform float foam_band_speed = 0.35;
// 8 light
uniform float gloss : hint_range(0.0, 1.0) = 0.94; // 1 - roughness of the water

varying vec3 world_position;

void vertex() {
	world_position = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;
}

void fragment() {
	LiquidFlow flow = liquid_flow(UV2, COLOR);
	float falling = 1.0 - flow.on_flat;
	vec2 velocity = mix(vec2(0.1, 0.06), flow.direction * flow_speed, flow.flows); // ponds drift
	vec3 mesh_normal = normalize((INV_VIEW_MATRIX * vec4(NORMAL, 0.0)).xyz); // world space

	// 1 waves: the slope of the flowing height field tilts the mesh's normal (flat water: up)
	FlowCopies copies = flow_copies(velocity, TIME, flow_cycle);
	vec2 slope = wave_slope(UV, copies, wave_scale, 4, TIME) * wave_strength;
	vec3 flat_normal = normalize(mesh_normal + vec3(-slope.x, 0.0, -slope.y));

	// 6 falls (their normal first): grooves along the threads, across the fall
	float threads = fall_threads(UV, streaks_per_metre, flow_speed * fall_speed, TIME);
	float threads_x = fall_threads(UV + vec2(0.01, 0.0), streaks_per_metre, flow_speed * fall_speed, TIME);
	vec3 across = normalize(cross(mesh_normal, vec3(0.0, 1.0, 0.0)) + vec3(1e-4));
	vec3 fall_normal = normalize(mesh_normal + across * (threads_x - threads) * 8.0);
	vec3 normal = normalize(mix(fall_normal, flat_normal, flow.on_flat));
	NORMAL = normalize((VIEW_MATRIX * vec4(normal, 0.0)).xyz);

	// 2 behind: the floor straight behind this pixel, and how far through water that is
	vec3 floor_straight = scene_view_position(SCREEN_UV, INV_PROJECTION_MATRIX);
	float path_straight = length(floor_straight - VERTEX);
	// 3 refraction: shift by the tilt (view space), less where the water is thin (edges stay put)
	vec2 shift = NORMAL.xy * refraction * smoothstep(0.0, 0.6, path_straight);
	vec2 uv = refracted_uv(SCREEN_UV, shift, VERTEX.z, INV_PROJECTION_MATRIX);
	vec3 floor_view = scene_view_position(uv, INV_PROJECTION_MATRIX);
	float path = length(floor_view - VERTEX); // metres the light travels through the water
	vec3 floor_world = (INV_VIEW_MATRIX * vec4(floor_view, 1.0)).xyz;
	float water_depth = max(world_position.y - floor_world.y, 0.0); // straight down to the floor

	// 5 caustics on the floor, fading as it gets deep and gone under foam
	vec3 behind = texture(liquid_screen, uv).rgb;
	float caustic = caustics(floor_world.xz * caustic_scale, TIME * caustic_speed);
	behind *= 1.0 + caustic * caustic_strength * exp(-water_depth * 0.9) * flow.on_flat;

	// 4 absorption: what's left of the floor's light (seen through: already lit), and the water's
	// own colour, light scattered back by the water itself: a colour the scene's light falls on
	// (lit, so it goes dark at night like everything else)
	vec3 transmitted = exp(-absorption * path);
	float scattering = 1.0 - exp(-scatter_density * path);
	vec3 through_water = behind * transmitted;
	vec3 water_colour = scatter_color * scattering;

	// 6 falls: a thin sheet: some of the cliff behind it shows through (seen through), the rest is
	// pale aerated water and its streaks (lit)
	vec3 behind_fall = texture(liquid_screen, SCREEN_UV + NORMAL.xy * 0.02).rgb;
	float bright = smoothstep(0.45, 0.85, threads);
	float fall_see_through = fall_clarity * (1.0 - bright * 0.7);
	vec3 through_fall = behind_fall * fall_color * fall_see_through;
	vec3 fall_colour = mix(fall_color * (1.0 - fall_clarity), foam_color, bright * 0.7);
	float lip = smoothstep(0.2, 0.6, UV2.x) * (1.0 - smoothstep(0.7, 1.0, UV2.x)); // bright over the edge
	float fall_foot = 1.0 - smoothstep(0.0, landing_height, UV2.y - threads * 0.25);
	float fall_foam = clamp(lip * 0.5 * (0.5 + threads) + fall_foot * step(0.5, UV2.x), 0.0, 1.0);

	// 7 foam on the flat water
	// a. bands rolling in: lines at depths fract(depth * bands + t) = 0, which move toward
	//    shallower water as t grows (toward the shore), broken up by noise, plus the contact line
	float band = fract(water_depth * foam_bands + TIME * foam_band_speed);
	float line = smoothstep(0.0, 0.08, band) * (1.0 - smoothstep(0.08, 0.3, band));
	float breakup = smoothstep(0.35, 0.65, fbm(world_position.xz * 3.0 + TIME * 0.2, 3));
	float near_shore = 1.0 - smoothstep(0.0, shore_foam_depth, water_depth);
	float contact = 1.0 - smoothstep(0.0, 0.05, water_depth);
	float shore = clamp(line * breakup * near_shore + contact, 0.0, 1.0) * (1.0 - flow.on_stream);
	// b. where a fall lands: churning fbm, fast
	float churn = fbm(UV * 2.2 + vec2(1.0, -0.7) * TIME * flow_speed * 1.8, 3);
	float landing = flow.landing * smoothstep(0.4, 0.7, churn);
	// c. lines along rivers: noise stretched along the flow, carried by it
	vec2 along = flow.direction / max(length(flow.direction), 1e-3);
	vec2 river_p = vec2(dot(UV, along) * 0.6, dot(UV, vec2(-along.y, along.x)) * 6.0);
	float river = flow.flows * smoothstep(0.72, 0.85, fbm(river_p - vec2(TIME * flow_speed * 0.6, 0.0), 3));
	float foam = clamp(shore + landing + river * 0.6, 0.0, 1.0) * flow.on_flat + fall_foam * falling;

	// 8 light: what is seen through the water / fall was lit already, behind: it is EMISSION (added
	// as it is). The water's own colour, the fall's pale water and the foam are lit by the scene
	// (ALBEDO), so at night only what the moon or a lamp lights shows.
	vec3 through = mix(through_water, through_fall, falling);
	vec3 own_colour = mix(water_colour, fall_colour, falling);
	ALBEDO = mix(own_colour, foam_color, foam);
	EMISSION = through * (1.0 - foam);
	ROUGHNESS = mix(1.0 - gloss, 0.85, max(foam, falling * 0.4));
	SPECULAR = mix(0.6, 0.2, foam);
}
