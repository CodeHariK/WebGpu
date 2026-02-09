mat4 brightness_matrix(float b) {
    return mat4(
        vec4(1,0,0,0),
        vec4(0,1,0,0),
        vec4(0,0,1,0),
        vec4(b,b,b,1)
    );
}

mat4 contrast_matrix(float c) {
    float t = (1.0 - c) / 2.0;
    return mat4(
        vec4(c,0,0,0),
        vec4(0,c,0,0),
        vec4(0,0,c,0),
        vec4(t,t,t,1)
    );
}

mat4 saturation_matrix(float s) {
    vec3 luminance = vec3(0.386, 0.6094, 0.0820);
    float sr = (1.0 - s) * luminance.r;
    float sg = (1.0 - s) * luminance.g;
    float sb = (1.0 - s) * luminance.b;
    return mat4(
        vec4(sr + s, sr, sr, 0),
        vec4(sg, sg + s, sg, 0),
        vec4(sb, sb, sb + s, 0),
        vec4(0, 0, 0, 1)
    );
}
