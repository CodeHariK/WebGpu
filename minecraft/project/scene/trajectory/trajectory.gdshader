shader_type spatial;
render_mode 
// cull_disabled,
// wireframe,
unshaded;

// Data Textures
uniform sampler2D height_texture : source_color;
uniform sampler2D albedo_texture : source_color, filter_linear_mipmap, repeat_enable;
 
// Mesh Parameters
uniform float amplitude = 2.0;
uniform float perimeter = 1.0;
uniform float texture_scale = 1.0;

void vertex() {
	// The displacement data is stored along the U-axis of the 1D height_texture.
	// Our mesh's UV.y coordinate now maps to the progress along the tube's length.
	vec2 displacement = texture(height_texture, vec2(UV.y, 0.5)).rg;

	 VERTEX.x += (displacement.x - 0.5) * amplitude;
	 VERTEX.y += (displacement.y - 0.5) * amplitude;
}

void fragment() {
	vec2 scaled_uv = vec2(UV.x, UV.y * perimeter * 5.0) * texture_scale;
	vec4 tex_color = texture(albedo_texture, scaled_uv);
	ALBEDO = tex_color.rgb;
}