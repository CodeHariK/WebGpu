// The matte prop surface (like MoodProps' StandardMaterial3D: soft Lambert-wrap shading, no
// specular) plus the theme effects a standard material can't do:
//   wind sway     (wind.gdshaderinc) for trees and leaves
//   pulse         glowing parts breathe — brightness waves rolling across the world (magical)
//   flicker       glowing parts buzz and drop out now and then, like failing neon (cyberpunk)
shader_type spatial;
render_mode diffuse_lambert_wrap, specular_disabled;

#include "res://mood/shaders/wind.gdshaderinc"

uniform vec4 albedo : source_color = vec4(1.0);
uniform vec4 emission : source_color = vec4(0.0, 0.0, 0.0, 1.0);
uniform float emission_energy = 0.0;
uniform float pulse = 0.0; // 0 = steady … 1 = breathing glow
uniform float flicker = 0.0; // 0 = steady … 1 = buzzing neon

varying vec3 world_position;

void vertex() {
	vec3 world = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;
	world_position = world;
	VERTEX += inverse(mat3(MODEL_MATRIX)) * wind_offset(world, TIME);
}

float hash(float n) {
	return fract(sin(n) * 43758.5453);
}

void fragment() {
	ALBEDO = albedo.rgb;
	ROUGHNESS = 1.0;
	float glow = emission_energy;
	glow *= mix(1.0, 0.45 + 0.55 * (0.5 + 0.5 * sin(TIME * 1.6 + world_position.x * 0.45 + world_position.z * 0.33)), pulse);
	float seed = floor(world_position.x * 0.5) * 17.0 + floor(world_position.z * 0.5) * 31.0 + floor(world_position.y);
	float frame = floor(TIME * 14.0);
	float dropout = step(0.94, hash(seed + frame)) * flicker; // now and then a strip blinks off
	glow *= (1.0 - dropout) * (1.0 - 0.15 * flicker * hash(seed + frame * 0.37));
	EMISSION = emission.rgb * glow;
}
