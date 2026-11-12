// Near grass: single blades (GrassMeshes.blade, 5 triangles), opaque, everything done here in
// the vertex shader from the blade's root position (no per-blade data from the CPU):
//   ground     the height map gives the root's height; the mask its density and dryness
//   random     a hash of the root's world position: each blade's height, turn, shade, so the
//              tiles (all sharing one pattern of roots) don't look repeated
//   density    a blade whose random number is over the mask's density folds to nothing
//   distance   blades shrink to nothing between near_end - fade and near_end: no pop, no alpha
//   wind       a noise texture sliding over the world bends blades in gusts, plus a quick
//              flutter of each blade; bending grows with the square of the height (stiff root)
//   push       blades lean away from the pusher (the player) and flatten under it
//   colour     root to tip gradient, green to dry by the mask, each blade's own shade, a dark
//              root (fake ambient occlusion), light through the blade from behind (BACKLIGHT)
//   normal     tilted up toward the sky: the field lights like a soft surface, not as
//              thousands of flickering cards
shader_type spatial;
render_mode cull_disabled, world_vertex_coords, specular_disabled;

uniform sampler2D height_map : filter_linear, repeat_disable;
uniform sampler2D mask_map : filter_linear, repeat_disable;
uniform sampler2D wind_noise : filter_linear, repeat_enable;
uniform float terrain_size = 512.0;
uniform float blade_height = 0.45; // metres, average
uniform float blade_width = 0.055; // metres at the root
uniform float near_end = 26.0; // metres from the camera where the last blades are gone
uniform float fade = 5.0; // metres they take to shrink away
uniform vec2 wind_direction = vec2(1.0, 0.35);
uniform float wind_strength = 0.6; // how far a gust leans a blade (x its height)
uniform float wind_speed = 0.05; // how fast the gusts cross (noise texture widths a second)
uniform vec4 pusher = vec4(0.0, -1000.0, 0.0, 1.2); // xyz, radius in metres
uniform vec3 green_root : source_color = vec3(0.16, 0.24, 0.07);
uniform vec3 green_tip : source_color = vec3(0.6, 0.66, 0.26);
uniform vec3 dry_root : source_color = vec3(0.33, 0.17, 0.06);
uniform vec3 dry_tip : source_color = vec3(0.92, 0.58, 0.24);
uniform float color_variation = 1.0; // 0: every blade alike .. 2: twice the spread of shade and dryness
uniform float dryness = 0.0; // -1 all green .. 1 all dry, added to the mask's

varying vec3 blade_colour;

float hash(vec2 p) {
	return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453);
}

vec2 map_uv(vec2 xz) {
	return (xz + terrain_size * 0.5 + 0.5) / (terrain_size + 1.0);
}

void vertex() {
	vec3 root = MODEL_MATRIX[3].xyz; // the instance's position: its root, on y 0
	vec3 local = VERTEX - root; // the blade as built: x across, y up, 1 x 1
	float r1 = hash(root.xz);
	float r2 = hash(root.zx + 17.31);
	vec4 mask = textureLod(mask_map, map_uv(root.xz), 0.0);
	float ground = textureLod(height_map, map_uv(root.xz), 0.0).r;

	// keep or fold away: density, then distance
	float dist = distance(root.xz, CAMERA_POSITION_WORLD.xz);
	float keep = step(r1, mask.r) * (1.0 - smoothstep(near_end - fade, near_end, dist));

	// size and turn
	float height = blade_height * mix(0.5, 1.5, r2 * r2) * keep;
	float angle = r1 * 6.2832;
	vec3 across = vec3(cos(angle), 0.0, sin(angle));
	vec3 blade = across * local.x * blade_width * keep + vec3(0.0, local.y * height, 0.0);

	// bend: wind gusts + flutter + the pusher, more toward the tip
	vec2 gust_uv = root.xz * 0.015 + normalize(wind_direction) * TIME * wind_speed;
	float gust = textureLod(wind_noise, gust_uv, 0.0).r;
	vec2 lean = normalize(wind_direction) * gust * wind_strength;
	lean += vec2(sin(TIME * 2.7 + r1 * 40.0), cos(TIME * 2.3 + r2 * 40.0)) * 0.05;
	vec2 away = root.xz - pusher.xz;
	float pushed = (1.0 - smoothstep(pusher.w * 0.4, pusher.w, length(away))) * step(abs(pusher.y - ground), 2.0);
	lean += normalize(away + 0.0001) * pushed * 1.6;
	float bend = local.y * local.y;
	blade.xz += lean * height * bend;
	blade.y *= 1.0 - 0.45 * clamp(length(lean), 0.0, 1.0) * local.y; // a leaning blade is lower

	VERTEX = vec3(root.x, ground, root.z) + blade;

	// a blade's normal: mostly up, a little toward its face (round-ish, never edge-on black)
	vec3 face = vec3(-across.z, 0.0, across.x);
	NORMAL = normalize(vec3(0.0, 1.0, 0.0) * 0.8 + face * local.x * 0.4 + vec3(lean.x, 0.0, lean.y) * 0.3);

	// colour: root to tip, green to dry, each blade its own shade, dark at the root
	float dry = clamp(mask.g + dryness + (r2 - 0.5) * 0.3 * color_variation, 0.0, 1.0);
	vec3 base = mix(green_root, dry_root, dry);
	vec3 tip = mix(green_tip, dry_tip, dry);
	blade_colour = mix(base, tip, local.y) * mix(1.0 - 0.2 * color_variation, 1.0 + 0.15 * color_variation, r1) * mix(0.7, 1.0, local.y);
}

void fragment() {
	ALBEDO = blade_colour;
	ROUGHNESS = 1.0;
	BACKLIGHT = blade_colour * 0.35; // sun through the blade
}
