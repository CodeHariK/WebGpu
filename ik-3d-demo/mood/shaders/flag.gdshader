// A waving flag on a subdivided quad (UV.x 0 at the pole, 1 at the free end): a travelling wave
// along it that grows toward the free end, stronger and faster with the wind; seen from both sides.
shader_type spatial;
render_mode cull_disabled, diffuse_lambert_wrap, specular_disabled;

uniform vec4 albedo : source_color = vec4(0.15, 0.3, 0.75, 1.0);
uniform float wind = 0.4;
uniform float phase = 0.0;

void vertex() {
	float along = UV.x;
	float speed = 4.0 + wind * 6.0;
	VERTEX.z += sin(TIME * speed - along * 9.0 + phase) * (0.06 + 0.12 * wind) * along;
	VERTEX.y += sin(TIME * speed * 0.7 - along * 6.0 + phase) * 0.03 * along;
}

void fragment() {
	ALBEDO = albedo.rgb * (FRONT_FACING ? 1.0 : 0.8);
	ROUGHNESS = 1.0;
}
