shader_type spatial;

uniform float height = 24.0;
uniform sampler2D color_gradient;

void fragment() {
    // Get the vertex in world space
    vec4 world_vertex = INV_VIEW_MATRIX * vec4(VERTEX, 1.0);
    vec3 model_vertex = (inverse(MODEL_MATRIX) * world_vertex).xyz;

    // Gradient lookup based on height
    float gradient_uv = (model_vertex.y / height) + 0.5;
    ALBEDO = texture(color_gradient, vec2(gradient_uv)).rgb;

    // Use the mesh normal, no override
    NORMAL = normalize(NORMAL);
}