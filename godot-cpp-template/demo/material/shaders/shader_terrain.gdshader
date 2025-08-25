shader_type spatial;
render_mode cull_back, depth_draw_opaque, diffuse_burley, specular_schlick_ggx;

uniform vec3 water_color : source_color = vec3(0.0, 0.3, 0.8);
uniform vec3 grass_color : source_color = vec3(0.3, 0.8, 0.1);
uniform vec3 rock_color  : source_color = vec3(0.5, 0.5, 0.5);
uniform vec3 snow_color  : source_color = vec3(1.0, 1.0, 1.0);

void fragment() {
    float h = VERTEX.y;

    if (h < 0.0) {
        ALBEDO = water_color;   // Below sea level
    } else if (h < 2.0) {
        ALBEDO = grass_color;   // Lowlands
    } else if (h < 5.0) {
        ALBEDO = rock_color;    // Mountains
    } else {
        ALBEDO = snow_color;    // High peaks
    }
}
