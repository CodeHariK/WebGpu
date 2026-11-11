// STEP 3 — toon light: lit or in shade, nothing in between.
// light() runs for every light on every pixel and adds what that light gives to DIFFUSE_LIGHT
// (Godot multiplies it by ALBEDO). The usual light fades smoothly as the surface turns away from
// the sun (dot(NORMAL, LIGHT): 1 facing it, 0 side on, below 0 facing away). Here smoothstep makes
// that a crisp step: full light on one side, shade (60 %) on the other, never black, so a fall in
// the sun's shadow still reads as bright water. ATTENUATION is 0 inside a shadow: shade there too.
// / PI: Godot's lights are scaled that way (the built-in light does the same).
shader_type spatial;
render_mode cull_disabled, specular_disabled;

uniform vec3 top_color : source_color = vec3(0.13, 0.5, 0.95);  // the flat water
uniform vec3 fall_color : source_color = vec3(0.2, 0.62, 1.0);  // a fall, lighter
uniform float fall_start : hint_range(0.0, 1.0) = 0.6;  // normal.y below this: a fall
uniform float shade : hint_range(0.0, 1.0) = 0.6;  // NEW: light on the side away from the sun

varying vec3 world_normal;

void vertex() {
	world_normal = normalize((MODEL_MATRIX * vec4(NORMAL, 0.0)).xyz);
}

void fragment() {
	vec3 n = normalize(world_normal);
	float fall = 1.0 - smoothstep(fall_start - 0.05, fall_start + 0.05, abs(n.y));
	vec3 colour = mix(top_color, fall_color, fall);
	ALBEDO = colour;
}

void light() {  // NEW
	float lit = smoothstep(-0.05, 0.05, dot(NORMAL, LIGHT)) * ATTENUATION;  // NEW: 0 or 1, crisp
	DIFFUSE_LIGHT += mix(shade, 1.0, lit) * LIGHT_COLOR / PI;  // NEW
}
