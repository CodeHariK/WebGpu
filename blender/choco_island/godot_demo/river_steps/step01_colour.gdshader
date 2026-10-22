// Step 1 — one colour. The syrup is a single chocolate brown with a low roughness, so Godot's
// normal lighting already gives it a soft shine. Everything after this adds one idea.
shader_type spatial;
render_mode cull_disabled;

uniform vec3 syrup : source_color = vec3(0.44, 0.22, 0.1);

void fragment() {
	ALBEDO = syrup;
	ROUGHNESS = 0.25;
	SPECULAR = 0.3;
}
