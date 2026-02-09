shader_type spatial;
render_mode unshaded;

uniform sampler2D SCREEN_TEXTURE: hint_screen_texture, filter_linear_mipmap;
uniform sampler2D NORMAL_TEXTURE: hint_normal_roughness_texture, filter_linear_mipmap;

void vertex() {
    POSITION = vec4(VERTEX, 1.0);
}

void fragment() {
    vec2 screen_uv = SCREEN_UV;
    vec3 color = texture(SCREEN_TEXTURE, screen_uv).rgb;
    vec3 normal = texture(NORMAL_TEXTURE, screen_uv).rgb;    
    ALBEDO = color + normal;
}



