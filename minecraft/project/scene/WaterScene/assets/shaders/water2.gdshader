shader_type spatial;
render_mode depth_draw_always;

uniform sampler2D DEPTH_TEXTURE: hint_depth_texture, filter_linear_mipmap;

uniform float metallic : hint_range(0.0, 1.0) = 0;
uniform float roughness : hint_range(0.0, 1.0) = 0.02;

uniform sampler2D wave;
uniform float wave_time = 0;

uniform vec3 albedo : source_color;
uniform vec3 albedo2 : source_color;
uniform sampler2D texture_normal;
uniform sampler2D texture_normal2;
uniform vec2 wave_direction = vec2(2.0,0.0);
uniform vec2 wave_2_direction = vec2(0.0,1.0);

uniform float time_scale : hint_range(0.0, 0.2, 0.005) = 0.025;
uniform float wave_speed = 2.0;
uniform float noise_scale = 10.0;
uniform float height_scale = 0.15;
uniform float beers_law = 2.0;
uniform float depth_offset = -0.75;

varying float height;
varying vec3 world_pos;

uniform float edge_scale = 0.1;
uniform float near = 0.5;
uniform float far = 100.0;

float fresnel(float amount, vec3 normal, vec3 view)
{
	return pow((1.0 - clamp(dot(normalize(normal), normalize(view)), 0.0, 1.0 )), amount);
}

float edge(float depth) {
	return near * far / (far + depth * (near - far));
}

void vertex() {
	world_pos = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;
	height = texture(wave, world_pos.xz / noise_scale + wave_time * wave_speed).r;
	VERTEX.y += height * height_scale;
}

void fragment() {
	float depth_texture = texture(DEPTH_TEXTURE, SCREEN_UV).r;
	float depth = PROJECTION_MATRIX[3][2] / (depth_texture + PROJECTION_MATRIX[2][2]);
	depth = depth + VERTEX.z;
	float depth_blend = exp((depth + depth_offset) * -beers_law);
	depth_blend = clamp(1.0 - depth_blend, 0.0, 1.0);
	
	float z_depth = edge(texture(DEPTH_TEXTURE, SCREEN_UV).x);
	float z_pos = edge(FRAGCOORD.z);
	float z_dif = z_depth - z_pos;
	
	vec2 time = (TIME * wave_direction) * time_scale;
	vec2 time2 = (TIME * wave_2_direction) * time_scale;

	vec3 normal_blend = mix(texture(texture_normal, world_pos.xz / noise_scale + time).rgb, 
							texture(texture_normal2, world_pos.xz / noise_scale + time2).rgb, 
							0.5);
	
	float fresnel = fresnel(5.0, NORMAL, VIEW);
	vec3 surface_color = mix(albedo, albedo2, fresnel);
	
    ALBEDO = surface_color;
	ALPHA = 1.0;
	METALLIC = metallic;
	ROUGHNESS = roughness;
	NORMAL_MAP = normal_blend;
}