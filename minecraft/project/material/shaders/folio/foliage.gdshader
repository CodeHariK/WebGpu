shader_type spatial;

// =============================================================================
// Folio "Foliage" leaf material  (folio World/Foliage.js)
//
// A leaf-cluster billboard cloud: the mesh is ~80 small cross-quads scattered on
// a sphere (built CPU-side by FolioFoliage), each quad alpha-cutting a leaf shape
// out of a shared leaf mask. Colour is a 2-tone gradient by sun facing; the leaf
// UV is rotated a little by the wind field so the cluster shimmers. Lit through
// the shared folio pipeline. Double-sided.
//
// Deferred vs folio: the "see-through near the vehicle" alpha fade (needs the
// vehicle screen position) and the shadow-offset receive tweak.
// =============================================================================

render_mode cull_disabled, ambient_light_disabled;

#include "res://material/shaders/folio/folio_material.gdshaderinc"

// Wind field (FolioWind) + shared perlin (FolioNoises); light dir is in the include.
global uniform vec3 folio_season_tint; // YearCycles seasonal vegetation tint
global uniform vec2 folio_wind_direction;
global uniform float folio_wind_position_frequency;
global uniform float folio_wind_strength;
global uniform float folio_wind_time;
global uniform sampler2D folio_noise_perlin : repeat_enable, filter_linear;

uniform sampler2D foliage_texture : filter_nearest;
uniform vec4 color_a : source_color = vec4(0.706, 0.71, 0.212, 1.0); // #b4b536
uniform vec4 color_b : source_color = vec4(0.847, 0.812, 0.231, 1.0); // #d8cf3b
uniform float leaf_threshold = 0.3;

varying vec2 v_local_xz;
varying vec2 v_uv;

vec2 fol_wind_offset(vec2 pos) {
	vec2 rp = pos * folio_wind_position_frequency;
	vec2 uv1 = rp * 0.2 + folio_wind_direction * folio_wind_time;
	float n1 = textureLod(folio_noise_perlin, uv1, 0.0).r - 0.5;
	vec2 uv2 = rp * 0.1 + folio_wind_direction * folio_wind_time * 0.2;
	float n2 = textureLod(folio_noise_perlin, uv2, 0.0).r - 0.5;
	return folio_wind_direction * (n1 + n2) * folio_wind_strength;
}

vec2 rot_uv(vec2 uvv, float a, vec2 c) {
	uvv -= c;
	float s = sin(a);
	float co = cos(a);
	uvv = vec2(uvv.x * co - uvv.y * s, uvv.x * s + uvv.y * co);
	return uvv + c;
}

void vertex() {
	v_world_pos = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;
	v_world_normal = normalize(mat3(MODEL_MATRIX) * NORMAL);
	v_local_xz = VERTEX.xz;
	v_uv = UV;
}

void fragment() {
	// Wind-rotated leaf cutout.
	float ang = length(fol_wind_offset(v_local_xz)) * 2.2;
	vec2 ruv = rot_uv(v_uv, ang, vec2(0.5));
	float a = texture(foliage_texture, ruv).r - leaf_threshold;
	if (a < 0.0) {
		discard;
	}

	// 2-tone colour by sun facing (folio: mix by smoothstep(dot(N, lightDir))).
	float m = smoothstep(0.0, 1.0, dot(normalize(v_world_normal), folio_light_direction));
	vec3 base = mix(color_a.rgb, color_b.rgb, m) * folio_season_tint;
	ALBEDO = folio_shade(base, SCREEN_UV, -VERTEX.z);
}

void light() {
	float m = smoothstep(0.0, 1.0, dot(normalize(v_world_normal), folio_light_direction));
	vec3 base = mix(color_a.rgb, color_b.rgb, m) * folio_season_tint;
	DIFFUSE_LIGHT = folio_light_shadow(ALBEDO, base, ATTENUATION);
}
