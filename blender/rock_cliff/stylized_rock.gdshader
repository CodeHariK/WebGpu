// Stylized rock — no textures, everything from world position and normal.
//
// Colour (fragment):
//   rock_light / rock_dark   two rock tones, blended by large world-space noise (each rock differs)
//   strata                   thin horizontal bands, wobbled by noise
//   height                   darker at the foot, lighter at the top (cheap fake AO)
//   moss                     patches of moss on upward-facing facets
//   undersides               overhangs fall to rock_under
// Light (light()):
//   toon bands: dark / mid / lit with soft edges, shadows tinted (shadow_tint) instead of grey,
//   plus a thin rim on silhouettes. No specular.
// Cost: a handful of noise lookups (hash value noise, no textures) — fine on mobile.
shader_type spatial;
render_mode specular_disabled;  // light() below does the diffuse

group_uniforms colour;
uniform vec3 rock_light : source_color = vec3(0.86, 0.74, 0.6);
uniform vec3 rock_dark : source_color = vec3(0.66, 0.53, 0.45);
uniform vec3 rock_under : source_color = vec3(0.42, 0.36, 0.42);
uniform vec3 moss_color : source_color = vec3(0.45, 0.62, 0.24);
uniform float tone_scale = 0.35;          // size of the light/dark blotches (1/metres)
uniform float strata_spacing = 0.6;      // metres between bands
uniform float strata_strength = 0.05;
uniform vec2 height_range = vec2(-0.5, 6.0);  // world Y (Godot up) of the foot and the top
uniform float foot_darkening = 0.3;
uniform float moss_amount : hint_range(0.0, 1.0) = 0.55;
uniform float moss_scale = 1.4;

group_uniforms lighting;
uniform vec3 shadow_tint : source_color = vec3(0.55, 0.5, 0.78);
uniform float shadow_light : hint_range(0.0, 1.0) = 0.3;  // tinted light kept on the shadow side (on top of ambient)
uniform float band_mid = 0.15;            // N·L where shadow turns to mid
uniform float band_lit = 0.45;            // N·L where mid turns to lit
uniform float band_softness = 0.04;
uniform float rim_strength = 0.25;
uniform float rim_width = 0.25;

varying vec3 world_pos;
varying vec3 world_normal;

float hash(vec3 p) {
	p = fract(p * 0.3183099 + 0.1);
	p *= 17.0;
	return fract(p.x * p.y * p.z * (p.x + p.y + p.z));
}

float value_noise(vec3 x) {
	vec3 i = floor(x);
	vec3 f = fract(x);
	f = f * f * (3.0 - 2.0 * f);
	return mix(mix(mix(hash(i + vec3(0, 0, 0)), hash(i + vec3(1, 0, 0)), f.x),
				   mix(hash(i + vec3(0, 1, 0)), hash(i + vec3(1, 1, 0)), f.x), f.y),
			   mix(mix(hash(i + vec3(0, 0, 1)), hash(i + vec3(1, 0, 1)), f.x),
				   mix(hash(i + vec3(0, 1, 1)), hash(i + vec3(1, 1, 1)), f.x), f.y), f.z);
}

void vertex() {
	world_pos = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;
	world_normal = normalize((MODEL_MATRIX * vec4(NORMAL, 0.0)).xyz);
}

void fragment() {
	float up = world_normal.y;

	float tone = value_noise(world_pos * tone_scale) * 0.7 + value_noise(world_pos * tone_scale * 3.0) * 0.3;
	vec3 colour = mix(rock_dark, rock_light, smoothstep(0.3, 0.7, tone));

	float wobble = value_noise(world_pos * vec3(0.6, 0.2, 0.6)) * 2.0;
	float strata = sin((world_pos.y / strata_spacing + wobble) * 6.2831853);
	colour *= 1.0 - strata_strength * smoothstep(0.6, 1.0, strata);

	float height = clamp((world_pos.y - height_range.x) / (height_range.y - height_range.x), 0.0, 1.0);
	colour *= mix(1.0 - foot_darkening, 1.05, height);

	float moss_noise = value_noise(world_pos * moss_scale);
	float moss = smoothstep(0.55, 0.8, up) * smoothstep(1.0 - moss_amount, 1.0 - moss_amount + 0.15, moss_noise);
	colour = mix(colour, moss_color, moss);

	colour = mix(colour, rock_under, smoothstep(-0.2, -0.6, up));

	ALBEDO = colour;
	ROUGHNESS = 1.0;
	SPECULAR = 0.0;
}

void light() {
	float ndl = dot(NORMAL, LIGHT) * ATTENUATION;
	float mid = smoothstep(band_mid - band_softness, band_mid + band_softness, ndl);
	float lit = smoothstep(band_lit - band_softness, band_lit + band_softness, ndl);
	float amount = mix(0.0, 0.6, mid) + 0.4 * lit;
	vec3 tint = mix(shadow_tint * shadow_light, vec3(1.0), amount);
	float rim = smoothstep(1.0 - rim_width, 1.0, 1.0 - max(dot(NORMAL, VIEW), 0.0)) * lit;
	DIFFUSE_LIGHT += LIGHT_COLOR / PI * (tint + rim * rim_strength);
}
