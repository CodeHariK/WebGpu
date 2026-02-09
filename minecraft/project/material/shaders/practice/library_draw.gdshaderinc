float draw_circle(vec2 uv, float size, float edge) {
    float dist = length(uv);
    return clamp((1.0 - dist / size) / edge, 0.0, 1.0);
}

vec2 fractal(vec2 uv, float octaves, float zoom) {
    return cos(uv * zoom) * octaves;
}
