// STEP 10 — the surface moves (vertex animation). This is the finished water.gdshader.
// vertex() can move the vertices themselves: VERTEX.y += ... lifts and drops the mesh in a slow
// swell made of a sine across X and a cosine across Z. smoothstep on depth keeps it calm right at
// the shore so the water doesn't lift off the sand. (Needs enough vertices: the mesh is a grid.)
// depth_draw_opaque + cull_back: normal settings for a transparent surface seen from above.
shader_type spatial;
render_mode unshaded, blend_mix, cull_back, depth_draw_opaque;  // NEW: cull_back, depth_draw_opaque

uniform vec3 shore_colour : source_color = vec3(0.6, 0.93, 0.9);
uniform vec3 shallow_colour : source_color = vec3(0.25, 0.75, 0.88);
uniform vec3 deep_colour : source_color = vec3(0.12, 0.42, 0.85);
uniform float shallow_end = 0.18;
uniform float deep_start = 0.45;
uniform vec3 foam_colour : source_color = vec3(1.0);
uniform float foam_width = 0.012;
uniform float wash = 0.015;
uniform float wave_speed = 0.9;
uniform float line_speed = 0.12;
uniform float line_count = 3.0;
uniform float sparkle_density = 0.004;
uniform float ripple_height = 0.03;  // NEW: swell height in metres

varying float depth;
varying vec3 world_pos;

float hash(vec3 p) {
	p = fract(p * 0.3183099 + 0.1);
	p *= 17.0;
	return fract(p.x * p.y * p.z * (p.x + p.y + p.z));
}

float noise(vec3 x) {
	vec3 i = floor(x);
	vec3 f = fract(x);
	f = f * f * (3.0 - 2.0 * f);
	return mix(mix(mix(hash(i), hash(i + vec3(1, 0, 0)), f.x), mix(hash(i + vec3(0, 1, 0)), hash(i + vec3(1, 1, 0)), f.x), f.y),
			   mix(mix(hash(i + vec3(0, 0, 1)), hash(i + vec3(1, 0, 1)), f.x), mix(hash(i + vec3(0, 1, 1)), hash(i + vec3(1, 1, 1)), f.x), f.y), f.z);
}

void vertex() {
	depth = COLOR.r;
	world_pos = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;
	float swell = sin(TIME * 1.3 + world_pos.x * 0.6) * cos(TIME * 1.1 + world_pos.z * 0.5);  // NEW: -1..1
	VERTEX.y += swell * ripple_height * smoothstep(0.05, 0.3, depth);                          // NEW
}

void fragment() {
	vec2 p = world_pos.xz;
	float wobble = (noise(vec3(p * 0.6, TIME * 0.2)) - 0.5) * 0.08;
	float d = depth + wobble;
	vec3 colour = mix(shore_colour, shallow_colour, smoothstep(0.02, shallow_end, d));
	colour = mix(colour, deep_colour, smoothstep(deep_start - 0.08, deep_start + 0.08, d));
	float wave = sin(TIME * wave_speed + p.x * 0.15) * 0.5 + 0.5;
	float edge = foam_width + wave * wash;
	float foam = 1.0 - smoothstep(edge - 0.004, edge, depth + wobble * 0.15);
	float lines = fract(d * line_count / deep_start - TIME * line_speed);
	float line = smoothstep(0.0, 0.03, lines) * (1.0 - smoothstep(0.05, 0.09, lines));
	line *= step(0.45, noise(vec3(p * 0.9, TIME * 0.3))) * (1.0 - smoothstep(0.1, deep_start, d));
	foam = max(foam, line * 0.8);

	vec3 cell = floor(vec3((p + TIME * vec2(0.3, 0.2)) * 9.0, floor(TIME * 2.0)));
	float sparkle = step(1.0 - sparkle_density, hash(cell)) * smoothstep(0.15, 0.4, d);

	ALBEDO = mix(colour, foam_colour, max(foam, sparkle));
	ALPHA = max(mix(0.55, 0.95, smoothstep(0.0, 0.35, depth)), foam);
}
