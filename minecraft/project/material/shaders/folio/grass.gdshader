shader_type spatial;

// =============================================================================
// Folio "Grass"  (folio World/Grass.js)
//
// A GPU blade field: one mesh of ~subdiv² camera-facing triangles (3 verts each).
// Each blade's ground XZ is stored in VERTEX.xz (y=0, shared by its 3 verts) and
// its height randomness in UV.x; the loop index (tip/left/right) comes from
// VERTEX_ID % 3. The vertex shader builds the blade: a toroidal camera-follow
// wrap, a triangle shape scaled by terrain grass coverage + height, a billboard
// rotation toward the camera, wind sway on the tip, and a "hide" push for cells
// with no grass. Colour comes from the shared terrain colour, lit via the folio
// pipeline (bounce/water off, flat up normal).
// =============================================================================

render_mode cull_disabled, ambient_light_disabled;

#include "res://material/shaders/folio/folio_material.gdshaderinc"

// Wind field (published by FolioWind) + shared perlin (FolioNoises).
global uniform vec3 folio_season_tint; // YearCycles seasonal vegetation tint
global uniform vec2 folio_wind_direction;
global uniform float folio_wind_position_frequency;
global uniform float folio_wind_strength;
global uniform float folio_wind_time;
global uniform sampler2D folio_noise_perlin : repeat_enable, filter_linear;

// Grass tunables (folio Grass uniforms).
uniform vec2 grass_center = vec2(0.0);
uniform float grass_size = 40.0;
uniform float blade_width = 0.1;
uniform float blade_height = 0.6;
uniform float blade_height_randomness = 0.6;

varying vec2 v_blade_xz;

const float GRASS_PI = 3.14159265358979;

// folio Wind.offsetNode: 2-octave perlin sway, along the wind direction.
vec2 wind_offset(vec2 pos) {
	vec2 rp = pos * folio_wind_position_frequency;
	vec2 uv1 = rp * 0.2 + folio_wind_direction * folio_wind_time;
	float n1 = textureLod(folio_noise_perlin, uv1, 0.0).r - 0.5;
	vec2 uv2 = rp * 0.1 + folio_wind_direction * folio_wind_time * 0.2;
	float n2 = textureLod(folio_noise_perlin, uv2, 0.0).r - 0.5;
	return folio_wind_direction * (n1 + n2) * folio_wind_strength;
}

// Rotate v around center c by angle a (folio rotateUV).
vec2 rot_around(vec2 v, float a, vec2 c) {
	v -= c;
	float s = sin(a);
	float co = cos(a);
	v = vec2(v.x * co - v.y * s, v.x * s + v.y * co);
	return v + c;
}

void vertex() {
	int loop = int(mod(float(VERTEX_ID), 3.0)); // 0 tip, 1 left, 2 right
	float tipness = (loop == 0) ? 1.0 : 0.0;

	// Camera-follow toroidal wrap so the field stays around grass_center.
	vec2 blade = VERTEX.xz;
	float half_size = grass_size * 0.5;
	vec2 loop_pos = blade - grass_center;
	loop_pos.x = mod(loop_pos.x + half_size, grass_size) - half_size;
	loop_pos.y = mod(loop_pos.y + half_size, grass_size) - half_size;
	vec3 position3 = vec3(loop_pos.x, 0.0, loop_pos.y) + vec3(grass_center.x, 0.0, grass_center.y);

	vec3 world_pos = (MODEL_MATRIX * vec4(position3, 1.0)).xyz;
	v_blade_xz = world_pos.xz;

	// Terrain grass coverage at the blade.
	vec2 tuv = world_pos.xz / folio_terrain_subdivision / 1.5 + 0.5;
	float g = textureLod(folio_terrain_data, tuv, 0.0).g;

	// Height: base * randomness * perlin variation * grass coverage.
	float height_variation = textureLod(folio_noise_perlin, world_pos.xz * 0.0321, 0.0).r + 0.5;
	float rnd = UV.x; // heightRandomness
	float height = blade_height
			* (blade_height_randomness * rnd + (1.0 - blade_height_randomness))
			* height_variation * g;

	// Blade triangle shape: tip (0,1), left (1,0), right (-1,0).
	vec2 bs = (loop == 0) ? vec2(0.0, 1.0) : ((loop == 1) ? vec2(1.0, 0.0) : vec2(-1.0, 0.0));
	vec3 shape = vec3(bs.x * blade_width * g, bs.y * height, 0.0);
	vec3 vpos = position3 + shape;

	// Billboard toward the camera (rotate around the blade base in XZ).
	vec3 cam = INV_VIEW_MATRIX[3].xyz;
	float ang = atan(world_pos.z - cam.z, world_pos.x - cam.x) - GRASS_PI * 0.5;
	vpos.xz = rot_around(vpos.xz, ang, world_pos.xz);

	// Wind: sway the tip only.
	vec2 w = wind_offset(world_pos.xz) * tipness * height * 2.0;
	vpos += vec3(w.x, 0.0, w.y);

	// Hide blades where there is little grass (folio: g <= 0.5 -> push far up).
	float hidden = step(g - 0.4, 0.1);
	vpos.y += hidden * 100.0;

	VERTEX = vpos;
	v_world_pos = vpos;
	v_world_normal = vec3(0.0, 1.0, 0.0);
}

void fragment() {
	// Base colour = shared terrain colour at the blade, through the folio pipeline.
	vec4 data = folio_terrain_sample(v_blade_xz);
	ALBEDO = folio_shade(folio_terrain_color(data) * folio_season_tint, SCREEN_UV, -VERTEX.z);
}

void light() {
	DIFFUSE_LIGHT = folio_light_shadow(ALBEDO, folio_terrain_color(folio_terrain_sample(v_blade_xz)) * folio_season_tint, ATTENUATION);
}
