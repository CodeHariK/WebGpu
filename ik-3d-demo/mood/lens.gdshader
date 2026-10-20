// Lens fringing: the red and blue channels pulled apart toward the screen edges, plus a little
// film grain; and heat shimmer: the picture wobbles in a band around the horizon, where hot air
// over far ground bends the light (desert). A full-screen pass — MoodCamera hides it when both
// are 0.
shader_type canvas_item;

uniform sampler2D screen : hint_screen_texture, filter_linear_mipmap;
uniform float strength : hint_range(0.0, 1.0) = 0.0;
uniform float heat : hint_range(0.0, 1.0) = 0.0;

float hash(vec2 p) {
	return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453);
}

void fragment() {
	vec2 uv = SCREEN_UV;
	float band = smoothstep(0.2, 0.45, uv.y) * smoothstep(0.85, 0.55, uv.y); // around the horizon
	vec2 wobble = vec2(sin(uv.y * 140.0 + TIME * 7.0) + sin(uv.y * 63.0 - TIME * 4.3), sin(uv.x * 90.0 + TIME * 5.0));
	uv += wobble * 0.0012 * heat * band;
	vec2 from_center = uv - 0.5;
	vec2 shift = from_center * length(from_center) * strength * 0.025;
	float r = texture(screen, uv + shift).r;
	float g = texture(screen, uv).g;
	float b = texture(screen, uv - shift).b;
	float grain = (hash(SCREEN_UV * 731.0 + TIME) - 0.5) * strength * 0.08;
	COLOR = vec4(vec3(r, g, b) + grain, 1.0);
}
