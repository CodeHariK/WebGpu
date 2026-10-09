// STEP 6 — the foam washes in and out (animation).
// TIME is seconds since start. sin(TIME) swings -1..1; "* 0.5 + 0.5" maps it to 0..1.
// The foam edge moves between foam_width and foam_width + wash. Adding p.x to the sine makes the
// wave arrive along the beach instead of everywhere at once. (Sand shader uses the same formula,
// so the wet sand moves with it.)
shader_type spatial;
render_mode unshaded, blend_mix;

uniform vec3 shore_colour : source_color = vec3(0.6, 0.93, 0.9);
uniform vec3 shallow_colour : source_color = vec3(0.25, 0.75, 0.88);
uniform vec3 deep_colour : source_color = vec3(0.12, 0.42, 0.85);
uniform float shallow_end = 0.18;
uniform float deep_start = 0.45;
uniform vec3 foam_colour : source_color = vec3(1.0);
uniform float foam_width = 0.012;
uniform float wash = 0.015;       // NEW: how far the edge moves
uniform float wave_speed = 0.9;   // NEW

varying float depth;
varying vec3 world_pos;  // NEW: where this pixel is in the world

void vertex() {
	depth = COLOR.r;
	world_pos = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;  // NEW: object space → world space
}

void fragment() {
	vec2 p = world_pos.xz;  // NEW: position on the water's surface
	vec3 colour = mix(shore_colour, shallow_colour, smoothstep(0.02, shallow_end, depth));
	colour = mix(colour, deep_colour, smoothstep(deep_start - 0.08, deep_start + 0.08, depth));
	float wave = sin(TIME * wave_speed + p.x * 0.15) * 0.5 + 0.5;          // NEW: 0..1, in time
	float edge = foam_width + wave * wash;                                  // NEW: moving edge
	float foam = 1.0 - smoothstep(edge - 0.004, edge, depth);
	ALBEDO = mix(colour, foam_colour, foam);
	ALPHA = max(mix(0.55, 0.95, smoothstep(0.0, 0.35, depth)), foam);
}
