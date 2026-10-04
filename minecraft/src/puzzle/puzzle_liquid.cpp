#include "puzzle_liquid.h"

#include <godot_cpp/classes/shader.hpp>

namespace godot {

namespace PuzzleLiquid {

// Two interfering sine waves in world space: soft bands + sparkles on the crests.
static const char *WATER_CODE = R"(
shader_type spatial;
render_mode diffuse_toon, specular_toon;
uniform vec3 deep_color = vec3(0.12, 0.45, 0.82);
uniform vec3 shallow_color = vec3(0.38, 0.82, 0.96);
varying vec3 world_pos;
void vertex() {
	world_pos = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;
}
void fragment() {
	float w = sin(world_pos.x * 2.1 + TIME * 1.4) * sin(world_pos.z * 1.7 - TIME * 1.1);
	vec3 c = mix(deep_color, shallow_color, 0.5 + 0.5 * w);
	c += smoothstep(0.82, 1.0, w) * 0.35; // sparkle on crests
	ALBEDO = c;
	ROUGHNESS = 0.12;
	SPECULAR = 0.6;
}
)";

// Slow churning crust: dark cracks over a glowing orange core that pulses.
static const char *LAVA_CODE = R"(
shader_type spatial;
render_mode diffuse_toon;
uniform vec3 crust_color = vec3(0.45, 0.08, 0.03);
uniform vec3 hot_color = vec3(1.0, 0.55, 0.12);
varying vec3 world_pos;
void vertex() {
	world_pos = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;
}
void fragment() {
	float a = sin(world_pos.x * 1.6 + TIME * 0.6) + sin(world_pos.z * 1.9 - TIME * 0.5);
	float b = sin((world_pos.x + world_pos.z) * 2.7 + TIME * 0.9);
	float heat = clamp(0.5 + 0.25 * a + 0.25 * b, 0.0, 1.0);
	vec3 c = mix(crust_color, hot_color, smoothstep(0.35, 0.8, heat));
	ALBEDO = c;
	EMISSION = c * (0.8 + 0.4 * sin(TIME * 2.0 + world_pos.x));
	ROUGHNESS = 0.8;
}
)";

static Ref<ShaderMaterial> _make(const char *p_code) {
	Ref<Shader> shader;
	shader.instantiate();
	shader->set_code(p_code);
	Ref<ShaderMaterial> m;
	m.instantiate();
	m->set_shader(shader);
	return m;
}

Ref<ShaderMaterial> water_material() { return _make(WATER_CODE); }

Ref<ShaderMaterial> lava_material() { return _make(LAVA_CODE); }

} // namespace PuzzleLiquid

} // namespace godot
