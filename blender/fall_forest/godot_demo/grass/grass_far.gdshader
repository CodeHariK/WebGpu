// Far grass: tufts (GrassMeshes.tuft, 3 triangles), opaque, shaded by their vertex colours.
// Cheaper than the near blades in every way: a tenth of the triangles a square metre, no
// pusher, one sway for the whole tuft, the same height map, mask and wind (so near and far
// grass match where they meet):
//   rings      a tuft grows in between near_start - fade and near_start (as the near blades
//              shrink) and shrinks away between far_end - fade and far_end; past that, the
//              ground's own colour is the grass
//   colour     the mask's dryness picks green or dry, the vertex colour darkens the root
shader_type spatial;
render_mode cull_disabled, world_vertex_coords, specular_disabled;

uniform sampler2D height_map : filter_linear, repeat_disable;
uniform sampler2D mask_map : filter_linear, repeat_disable;
uniform sampler2D wind_noise : filter_linear, repeat_enable;
uniform float terrain_size = 512.0;
uniform float tuft_height = 0.5; // metres, average
uniform float near_start = 26.0; // metres: the near grass's near_end
uniform float far_end = 80.0;
uniform float fade = 5.0;
uniform vec2 wind_direction = vec2(1.0, 0.35);
uniform float wind_strength = 0.6;
uniform float wind_speed = 0.05;
uniform vec3 green : source_color = vec3(0.48, 0.55, 0.2);
uniform vec3 dry : source_color = vec3(0.8, 0.48, 0.2);
uniform float color_variation = 1.0; // as the near grass's
uniform float dryness = 0.0;

varying vec3 tuft_colour;

float hash(vec2 p) {
	return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453);
}

vec2 map_uv(vec2 xz) {
	return (xz + terrain_size * 0.5 + 0.5) / (terrain_size + 1.0);
}

void vertex() {
	vec3 root = MODEL_MATRIX[3].xyz;
	vec3 local = VERTEX - root;
	float r1 = hash(root.xz);
	float r2 = hash(root.zx + 17.31);
	vec4 mask = textureLod(mask_map, map_uv(root.xz), 0.0);
	float ground = textureLod(height_map, map_uv(root.xz), 0.0).r;

	float dist = distance(root.xz, CAMERA_POSITION_WORLD.xz);
	float grow = smoothstep(near_start - fade, near_start, dist) * (1.0 - smoothstep(far_end - fade, far_end, dist));
	float size = step(r1, mask.r) * grow * mix(0.7, 1.3, r2);

	float angle = r1 * 6.2832; // turn the tuft about its root
	vec2 turned = vec2(local.x * cos(angle) - local.z * sin(angle), local.x * sin(angle) + local.z * cos(angle));
	vec3 tuft = vec3(turned.x, local.y * tuft_height, turned.y) * size;

	vec2 gust_uv = root.xz * 0.015 + normalize(wind_direction) * TIME * wind_speed;
	float gust = textureLod(wind_noise, gust_uv, 0.0).r;
	tuft.xz += normalize(wind_direction) * gust * wind_strength * tuft_height * size * local.y * local.y;

	VERTEX = vec3(root.x, ground, root.z) + tuft;
	NORMAL = vec3(0.0, 1.0, 0.0);
	float dried = clamp(mask.g + dryness + (r1 - 0.5) * 0.3 * color_variation, 0.0, 1.0);
	tuft_colour = mix(green, dry, dried) * COLOR.rgb * mix(1.0 - 0.15 * color_variation, 1.0 + 0.1 * color_variation, r2);
}

void fragment() {
	ALBEDO = tuft_colour;
	ROUGHNESS = 1.0;
	BACKLIGHT = tuft_colour * 0.3;
}
