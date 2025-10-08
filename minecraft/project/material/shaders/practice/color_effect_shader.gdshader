shader_type canvas_item;

#include "library_color.gdshaderinc"

uniform float brightness: hint_range(-1.0, 1.0, 0.01) = 0.0;
uniform float contrast: hint_range(-10.0, 10.0, 0.01) = 1.0;
uniform float saturation: hint_range(-10.0, 10.0, 0.01) = 1.0;

void fragment() {
    vec4 color = texture(TEXTURE, UV);
    mat4 matrix = 
    brightness_matrix(brightness) * 
    contrast_matrix(contrast) * 
    saturation_matrix(saturation);
    COLOR = matrix * color;
}
