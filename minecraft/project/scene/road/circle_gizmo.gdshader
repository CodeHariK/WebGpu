shader_type spatial;
render_mode unshaded, cull_disabled;

uniform vec4 base_color : source_color;
uniform float ring_inner_radius = 0.48;
uniform float ring_outer_radius = 0.49;
uniform float feather = 0.01;

void fragment() {
    vec2 uv_centered = UV - vec2(0.5);
    float dist_from_center = length(uv_centered);
    
    float ring = smoothstep(ring_inner_radius - feather, ring_inner_radius, dist_from_center) -
                 smoothstep(ring_outer_radius, ring_outer_radius + feather, dist_from_center);

    ALBEDO = base_color.rgb;
    ALPHA = base_color.a * ring;
}
