vec3 palette(float t, vec3 a, vec3 b, vec3 c, vec3 d) {
	return a + b * cos(2.0 * PI * (c * t + d));
}

vec2 rotate(vec2 uv, float angle) {
	return uv * mat2(
			vec2(cos(angle), -sin(angle)),
			vec2(sin(angle), cos(angle))
		);
}

vec3 invert_color(vec3 color, float intensity) {
	return mix(color.rgb, 1.0 - color.rgb, intensity);
}

vec2 to_polar(vec2 uv) {
    vec2 centered_uv = uv - 0.5;
    return vec2(length(centered_uv), atan(centered_uv.y, centered_uv.x));
}

float draw_circle(vec2 uv, float size, float edge) {
    float dist = length(uv);
    return clamp((1.0 - dist / size) / edge, 0.0, 1.0);
}
