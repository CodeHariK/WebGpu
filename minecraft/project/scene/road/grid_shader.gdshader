shader_type spatial;
render_mode unshaded, cull_disabled;

uniform vec4 line_color : source_color = vec4(1.0, 1.0, 1.0, 0.4);
uniform float tiling : hint_range(1.0, 100.0) = 8.0;
uniform float line_thickness : hint_range(0.001, 0.1) = 0.04;

void fragment() {
    // Use UV coordinates multiplied by a tiling factor to create the grid.
    vec2 grid_uv = UV * tiling;

    // Get the fractional part of the coordinates. This will be between 0.0 and 1.0 for each grid cell.
    vec2 fractional_uv = fract(grid_uv);
    
    // 1. Calculate grid lines (branchless)
    // step(edge, x) returns 0 if x < edge, and 1 if x >= edge.
    // This creates lines at the start and end of each grid cell.
    float line_x = 1.0 - step(line_thickness, fractional_uv.x) + step(1.0 - line_thickness, fractional_uv.x);
    float line_y = 1.0 - step(line_thickness, fractional_uv.y) + step(1.0 - line_thickness, fractional_uv.y);
    float grid_alpha = max(line_x, line_y);

    ALBEDO = line_color.rgb;
    ALPHA = line_color.a * grid_alpha;
}
