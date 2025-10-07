shader_type spatial;

uniform sampler2D noise_texture: hint_default_white, filter_nearest, repeat_enable;
uniform sampler2D mask_texture: hint_default_white;
uniform vec4 base_color: source_color = vec4(1.0, 0.75, 0.3, 1.0);
uniform vec4 edge_color: source_color = vec4(1.0, 0.1, 0.0, 1.0);
uniform float fire_alpha : hint_range(0.0, 1.0, 0.1) = 1.0;
uniform float fire_filter : hint_range(0.0, 3.0, 0.1) = 0.2;
uniform float fire_power : hint_range(1.0, 20.0, 0.1) = 10.0;
uniform vec2 fire_speed = vec2(0.0, 1.0);

void vertex() {
	// Called for every vertex the material is visible on.
}

void fragment() {
    vec2 uv = UV + TIME * fire_speed;
    float fire_noise = texture(noise_texture, uv).r;
    float mask = texture(mask_texture, uv).r;
    float noise = (fire_noise * (fire_filter + UV.y) - fire_filter) * UV.y * fire_power * mask;
    vec4 fire_color = mix(edge_color, base_color, noise);
    ALBEDO = fire_color.rgb;
    ALPHA = clamp(noise, 0.0,1.0);
}

//void light() {
//	// Called for every pixel for every light affecting the material.
//	// Uncomment to replace the default light processing function with this one.
//}
