shader_type spatial;
render_mode unshaded;

uniform sampler2D SCREEN_TEXTURE: hint_screen_texture, filter_linear_mipmap;
uniform sampler2D DEPTH_TEXTURE: source_color, hint_depth_texture;
uniform float depth_range: hint_range(0.1, 100.0, 0.1) = 10.0;

void vertex() {
    POSITION = vec4(VERTEX, 1.0);
}

void fragment() {
    vec2 screen_uv = SCREEN_UV;
    vec3 color = texture(SCREEN_TEXTURE, screen_uv).rgb;
    float depth = texture(DEPTH_TEXTURE, screen_uv).x;
    vec3 ndc = vec3(screen_uv * 2.0 - 1.0, depth);
    vec4 view = INV_PROJECTION_MATRIX * vec4(ndc , 1.0);
    view.xyz /= view.w;
    float linear_depth = -view.z / depth_range;
    vec3 fog = vec3(linear_depth);
    ALBEDO = color + fog;
}



