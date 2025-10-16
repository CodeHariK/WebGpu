shader_type spatial;
// render_mode cull_disabled;

// Data Textures
uniform sampler2D height_texture : source_color;
uniform sampler2D albedo_texture : source_color, filter_linear_mipmap, repeat_enable;
 
// Mesh Parameters
uniform float amplitude = 2.0;
uniform float perimeter = 1.0;
uniform float texture_scale = 1.0;

void vertex() {
	vec2 displacement = texture(height_texture, UV).rg;

	// VERTEX.x += displacement.x * amplitude;
	// VERTEX.y += displacement.y * amplitude;
}

void fragment() {
	// Scale UVs based on the mesh's actual world dimensions to prevent stretching.
	vec2 scaled_uv = vec2(UV.x * perimeter, UV.y) * texture_scale;
	vec4 tex_color = texture(albedo_texture, scaled_uv);
	ALBEDO = tex_color.rgb;
}