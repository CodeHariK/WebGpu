// Island layers (grass sheet, icing bands): the colour comes from the vertex colour, so every
// layer can share this one material. Soft patches of darker colour, and the same soft two-band
// toon light and cool shadow tint as sand.gdshader, so layers sit in the same picture.
shader_type spatial;
render_mode specular_disabled;

uniform float patch_strength = 0.12;
uniform vec3 shadow_tint : source_color = vec3(0.7, 0.68, 0.9);
uniform float shadow_light : hint_range(0.0, 1.0) = 0.35;
uniform float band_lit = 0.25;
uniform float band_softness = 0.12;

varying vec3 world_pos;

float hash(vec3 p) {
	p = fract(p * 0.3183099 + 0.1);
	p *= 17.0;
	return fract(p.x * p.y * p.z * (p.x + p.y + p.z));
}

float noise(vec3 x) {
	vec3 i = floor(x);
	vec3 f = fract(x);
	f = f * f * (3.0 - 2.0 * f);
	return mix(mix(mix(hash(i), hash(i + vec3(1, 0, 0)), f.x), mix(hash(i + vec3(0, 1, 0)), hash(i + vec3(1, 1, 0)), f.x), f.y),
			   mix(mix(hash(i + vec3(0, 0, 1)), hash(i + vec3(1, 0, 1)), f.x), mix(hash(i + vec3(0, 1, 1)), hash(i + vec3(1, 1, 1)), f.x), f.y), f.z);
}

void vertex() {
	world_pos = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;
}

void fragment() {
	float patches = smoothstep(0.45, 0.6, noise(world_pos * 0.5));
	vec3 colour = pow(COLOR.rgb, vec3(2.2));  // vertex colours are authored in sRGB; lighting needs linear
	ALBEDO = colour * (1.0 - patch_strength * patches);
	ROUGHNESS = 1.0;
}

void light() {
	float ndl = dot(NORMAL, LIGHT) * ATTENUATION;
	float lit = smoothstep(band_lit - band_softness, band_lit + band_softness, ndl);
	DIFFUSE_LIGHT += LIGHT_COLOR / PI * mix(shadow_tint * shadow_light, vec3(1.0), lit);
}
