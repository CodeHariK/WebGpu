shader_type spatial;
render_mode blend_mix, depth_draw_opaque, depth_test_default, cull_back, diffuse_lambert, specular_schlick_ggx;

uniform float WAVE_TIME;
uniform float HEIGHT_SCALE;
uniform float WAVE_SPEED;
uniform float WAVE_SCALE = 0.00390625;
uniform sampler2D HEIGHT_NOISE;
uniform sampler2D DEPTH_TEX : hint_depth_texture;
uniform vec4 WATER_COLOR : source_color = vec4(0.332623, 0.695442, 0.967303, 1.000000);
uniform sampler2D WAVE_NOISE_1;
uniform sampler2D WAVE_NOISE_2;


void vertex() {
    vec3 world_pos = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;

    float height = texture(HEIGHT_NOISE, world_pos.xz * WAVE_SCALE + vec2(WAVE_TIME * WAVE_SPEED)).r;

    VERTEX += NORMAL * height * HEIGHT_SCALE;
}

void fragment() {
	// Depth and world position reconstruction
	float depth_value = texture(DEPTH_TEX, SCREEN_UV).r;
	vec4 world_position = INV_PROJECTION_MATRIX * vec4(SCREEN_UV * 2.0 - 1.0, depth_value, 1.0);
	world_position.xyz /= world_position.w;

	// Fade based on depth proximity to surface
	float fade_distance = 1.0; // can be a uniform for control
	float proximity_fade = clamp(
		1.0 - smoothstep(world_position.z + fade_distance, world_position.z, VERTEX.z),
		0.0, 1.0
	);

	// Time-based wave modulation
	float fade_strength = 1.0 - proximity_fade;
	float time_modulated = WAVE_TIME + fade_strength;
	float wave_pattern = sin(time_modulated * 3.0);

	// Strengthen wave effect near surface
	float fade_pow = pow(fade_strength, 4.0);
	float wave_intensity = wave_pattern * fade_pow;

	// Combine two scrolling noise textures
	vec2 wave_scroll_speed = vec2(0.1 * WAVE_TIME);
	vec4 noise1 = texture(WAVE_NOISE_1, UV + wave_scroll_speed);
	vec4 noise2 = texture(WAVE_NOISE_2, UV - wave_scroll_speed);
	vec4 combined_noise = noise1 * noise2;

	// Water color + noise modulation
	vec4 modulated_color = WATER_COLOR + pow(combined_noise, vec4(1.25));

	// Final color output with wave influence
	vec3 final_color = modulated_color.rgb + vec3(wave_intensity);

	ALBEDO =  modulated_color.rgb;
	// ALPHA = 0.8;
	ROUGHNESS = 0.0;
}
