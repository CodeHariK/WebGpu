// STEP 1 — colour by distance: no flow map, no UV, only where the pixel is.
// A whirlpool flows the same way all round its middle, so the shader can work the flow out from
// the position alone: no flow map needed. render_mode world_vertex_coords puts VERTEX in world
// space in vertex(); minus the node's origin (MODEL_MATRIX[3]) it is metres from the middle, so
// moving the sheet moves the whirlpool, and scaling the sheet doesn't stretch the pattern. Here
// only the colour uses it: deep in the middle, shallow out at radius.
shader_type spatial;
render_mode world_vertex_coords;  // NEW: VERTEX (and NORMAL) in world space in vertex()

uniform vec3 deep_color : source_color = vec3(0.03, 0.2, 0.36);  // NEW
uniform vec3 shallow_color : source_color = vec3(0.2, 0.62, 0.7);  // NEW
uniform float radius = 6.0;  // NEW: metres: how far the whirl reaches
uniform float eye = 0.7;  // NEW: metres: the middle (it spins fastest there, from step 4)

varying vec2 offset;  // NEW: metres from the middle (x, z)

void vertex() {
	offset = VERTEX.xz - MODEL_MATRIX[3].xz;  // NEW: the node's origin is the middle
}

void fragment() {
	float r = length(offset);  // NEW: metres from the middle
	ALBEDO = mix(deep_color, shallow_color, smoothstep(eye, radius, r));  // NEW
	ROUGHNESS = 0.25;  // NEW: a little shiny
}
