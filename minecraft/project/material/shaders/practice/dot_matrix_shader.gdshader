shader_type canvas_item;

#include "library.gdshaderinc"

uniform vec3 luminance : source_color = vec3(0.3086, 0.6094, 0.0820);
uniform float brightness: hint_range(0.1, 20.0, 0.01) = 2.0;
uniform float contrast: hint_range(1.0, 4.0, 0.01) = 2.0;
uniform float scale: hint_range(0.0, 2.0, 0.01) = 1.0;
uniform float angle: hint_range(0.0, 6.28, 0.01) = 3.0;

float dot_matrix(vec2 uv) {
    vec2 rot_uv = rotate(angle) * uv * scale;
    return sin(rot_uv.x) * sin(rot_uv.y);
}

void fragment() {
    vec2 resolution = 1.0 / TEXTURE_PIXEL_SIZE;
    vec3 color = texture(TEXTURE, UV).rgb;
    float grey = dot(color, luminance) * contrast;
    color = vec3(grey + dot_matrix(UV * resolution) - 1.0);
    COLOR = vec4(color * brightness, 1);
}
