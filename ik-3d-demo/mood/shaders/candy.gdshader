// Glossy candy: hard-candy and jelly. A crisp toon specular highlight and a clear-coat layer (the
// "lacquered" shine), a strong rim, and a soft inner glow where the surface faces the camera —
// as if light were passing through it. Sways in the wind too (lollipops on their sticks).
shader_type spatial;
render_mode diffuse_lambert_wrap, specular_toon;

#include "res://mood/shaders/wind.gdshaderinc"

uniform vec4 albedo : source_color = vec4(1.0, 0.5, 0.7, 1.0);
uniform float inner_glow = 0.22;

void vertex() {
	vec3 world = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;
	VERTEX += inverse(mat3(MODEL_MATRIX)) * wind_offset(world, TIME);
}

void fragment() {
	ALBEDO = albedo.rgb;
	ROUGHNESS = 0.22;
	SPECULAR = 0.8;
	CLEARCOAT = 1.0;
	CLEARCOAT_ROUGHNESS = 0.08;
	RIM = 0.7;
	RIM_TINT = 0.6;
	float facing = clamp(dot(NORMAL, VIEW), 0.0, 1.0);
	EMISSION = albedo.rgb * inner_glow * facing * facing; // light "inside" the candy
}
