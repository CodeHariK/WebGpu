shader_type spatial;
render_mode unshaded, cull_disabled;

uniform vec4 rotation_color : source_color;
uniform vec4 planar_move_color : source_color = vec4(0.5, 0.8, 1.0, 0.2);

// Define the bounds for the two-part gizmo
uniform float rotation_ring_inner = 0.42;
uniform float rotation_ring_outer = 0.45; // This is also the start of the move area
uniform float move_area_outer = 0.5;      // Outer edge of the whole gizmo

uniform float feather = 0.01;

void fragment() {
    vec2 uv_centered = UV - vec2(0.5);
    float dist_from_center = length(uv_centered);
    
    // 1. Define the rotation ring
    float rotation_alpha = smoothstep(rotation_ring_inner - feather, rotation_ring_inner, dist_from_center) -
                           smoothstep(rotation_ring_outer, rotation_ring_outer + feather, dist_from_center);
    vec4 rotation_final_color = vec4(rotation_color.rgb, rotation_color.a * rotation_alpha);
    
    // 2. Define the planar move area (between the rotation ring and the outer edge)
    float planar_alpha = smoothstep(rotation_ring_outer - feather, rotation_ring_outer, dist_from_center) -
                         smoothstep(move_area_outer, move_area_outer + feather, dist_from_center);
    vec4 planar_final_color = vec4(planar_move_color.rgb, planar_move_color.a * planar_alpha);

    // 3. Combine them. Since they are mutually exclusive, we can just add them.
    vec4 final_color = rotation_final_color + planar_final_color;

    ALBEDO = final_color.rgb;
    ALPHA = final_color.a;
}
