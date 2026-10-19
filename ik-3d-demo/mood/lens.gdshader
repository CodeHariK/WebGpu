// Lens fringing: the red and blue channels pulled apart toward the screen edges, plus a little
// film grain. A full-screen pass — MoodCamera hides it when strength is 0.
shader_type canvas_item;

uniform sampler2D screen : hint_screen_texture, filter_linear_mipmap;
uniform float strength : hint_range(0.0, 1.0) = 0.0;

float hash(vec2 p) {
	return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453);
}

void fragment() {
	vec2 from_center = SCREEN_UV - 0.5;
	vec2 shift = from_center * length(from_center) * strength * 0.025;
	float r = texture(screen, SCREEN_UV + shift).r;
	float g = texture(screen, SCREEN_UV).g;
	float b = texture(screen, SCREEN_UV - shift).b;
	float grain = (hash(SCREEN_UV * 731.0 + TIME) - 0.5) * strength * 0.08;
	COLOR = vec4(vec3(r, g, b) + grain, 1.0);
}
