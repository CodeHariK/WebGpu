shader_type spatial;

// Easily tweak your colors in the Godot Inspector
uniform vec3 snow_color : source_color = vec3(0.3, 0.5, 1.0);
uniform vec3 grass_color : source_color = vec3(0.15, 0.5, 0.2);
uniform vec3 rock_color : source_color = vec3(0.4, 0.4, 0.4);

// Where the snow starts (Y height)
uniform float snow_height = 12.0; 

varying vec3 world_pos;
varying vec3 world_normal;

void vertex() {
    // Grab the true world position and normal of the geometry
    world_pos = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;
    world_normal = normalize((MODEL_MATRIX * vec4(NORMAL, 0.0)).xyz);
}

void fragment() {
    // 1. Calculate Slope (0.0 is a sheer cliff, 1.0 is perfectly flat)
    float slope = clamp(world_normal.y, 0.0, 1.0);
    
    // 2. Calculate the Grass Blend
    // smoothstep(min, max, value) returns 0.0 below min, 1.0 above max, and smoothly blends in between.
    // This replaces "if (slope > 0.5)"
    float grass_weight = smoothstep(0.4, 0.6, slope);
    
    // Mix the rock and grass colors based on the weight
    vec3 base_color = mix(rock_color, grass_color, grass_weight);
    
    // 3. Calculate the Snow Blend
    // Blends smoothly over a 5-unit distance (e.g., from Y=45 to Y=50)
    float snow_weight = smoothstep(snow_height, snow_height + 5.0, world_pos.y);
    
    // Mix the base_color and snow based on height
    ALBEDO = mix(base_color, snow_color, snow_weight);
}
