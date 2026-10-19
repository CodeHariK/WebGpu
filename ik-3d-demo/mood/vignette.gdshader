// Darkens the screen edges (an oval falloff), tinted slightly purple.
shader_type canvas_item;

uniform float strength : hint_range(0.0, 1.0) = 0.5;
uniform vec3 tint : source_color = vec3(0.08, 0.0, 0.1);

void fragment() {
	vec2 centered = SCREEN_UV - 0.5;
	float edge = smoothstep(0.25, 0.75, length(centered * vec2(1.25, 1.0)));
	COLOR = vec4(tint, edge * strength);
}
