shader_type canvas_item;

uniform sampler2D noise_texture : repeat_enable;
uniform sampler2D second_texture : hint_default_black;
uniform float threshold : hint_range(0, 1.0, 0.01) = 0;

void fragment() {
    vec3 color = texture(second_texture, UV).rgb;
    float noise = texture(noise_texture, UV).r;
    if (noise > threshold) {
        color = texture(TEXTURE, UV).rgb;
    }
    COLOR = vec4(color, 1.0);
}
