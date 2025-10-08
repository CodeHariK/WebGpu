vec3 palette(float t, vec3 a, vec3 b, vec3 c, vec3 d) {
	return a + b * cos(2.0 * PI * (c * t + d));
}

mat2 rotate(float angle) {
    float sa = sin(angle);
    float ca = cos(angle);
    return mat2(vec2(ca, sa),
                vec2(-sa, ca));
}

vec3 invert_color(vec3 color, float intensity) {
	return mix(color.rgb, 1.0 - color.rgb, intensity);
}

vec2 to_polar(vec2 uv) {
    vec2 centered_uv = uv - 0.5;
    return vec2(length(centered_uv), atan(centered_uv.y, centered_uv.x));
}
