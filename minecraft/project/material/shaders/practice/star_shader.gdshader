shader_type canvas_item;

uniform vec2 resolution = vec2(600.0, 400.0);
uniform float density : hint_range(1.0, 100.0, 0.1) = 20;
uniform sampler2D noise_texture : filter_nearest, repeat_enable;

void fragment() {
    vec2 uv = UV;
    uv.x *= resolution.x / resolution.y;
    float stars  = step(0.2, pow(texture(noise_texture, uv).r , density));
    COLOR = vec4(vec3(stars), 1.0);
}
