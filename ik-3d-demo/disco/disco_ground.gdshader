// The void around the hall: glossy black with a faint neon grid that fades out with distance
// (synthwave floor). Lines every `cell` metres, aligned with the dance-floor tiles.
shader_type spatial;
render_mode specular_schlick_ggx;

uniform vec3 base : source_color = vec3(0.025, 0.015, 0.045);
uniform vec3 line_color : source_color = vec3(0.55, 0.2, 1.0);
uniform float cell = 2.2;
uniform float line_width = 0.04; // metres
uniform float energy = 0.9;
uniform float fade_start = 14.0; // metres from the centre where the grid starts fading
uniform float fade_end = 60.0;

varying vec3 world;

void vertex() {
	world = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;
}

void fragment() {
	vec2 p = world.xz / cell + 0.5; // +0.5: lines fall between tiles, like the floor's gaps
	vec2 d = abs(fract(p) - 0.5) * cell; // metres from the nearest line
	float line = 1.0 - smoothstep(line_width * 0.5, line_width, min(0.5 * cell - d.x, 0.5 * cell - d.y));
	float fade = 1.0 - smoothstep(fade_start, fade_end, length(world.xz));
	ALBEDO = base;
	ROUGHNESS = 0.15;
	EMISSION = line_color * line * fade * energy;
}
