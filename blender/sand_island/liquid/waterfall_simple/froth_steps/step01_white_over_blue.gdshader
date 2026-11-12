// STEP 1 — froth: a tube lying where the fall lands, white over blue.
// Blender's bottom foam is a tube along the foot of the falls (froth_tube.gd builds one: 10 m long,
// 1.6 m thick, half under the pool). Its material doesn't use the light at all (its Shader to
// RGB is never bright enough to count): the colour is only a line around the tube, by height on
// the tube's own box (b, like Blender's Generated: b.x along, b.y depth, b.z height, 0..1),
// turned a little (95 degrees about y: mostly height, a touch along it):
//   above    past-white foam (2.74: the tonemap makes it a bright white)
//   below    a clear light blue: the foam's shadowed underside, the water seen through it
// For now the line is straight and the tube smooth: the next steps bend the line and boil the
// tube.
shader_type spatial;
render_mode unshaded, cull_disabled;

uniform vec3 foam_color = vec3(2.74, 2.6, 2.75);  // NEW: linear, past white
uniform vec3 under_color = vec3(0.038, 0.471, 0.66);  // NEW
uniform float tube_length = 10.0;  // NEW: froth_tube.gd's length
uniform float tube_radius = 1.6;  // NEW: and radius

varying vec3 b;  // NEW: the tube's box, 0..1, Blender's order: along, depth, height

void vertex() {
	b = vec3(VERTEX.x / tube_length + 0.5, 0.5 - VERTEX.z / (2.0 * tube_radius), 0.5 + VERTEX.y / (2.0 * tube_radius));  // NEW
}

void fragment() {
	float line = -0.087 * b.x + 0.996 * b.z - 0.47;  // NEW: Blender: the box turned 95 deg, moved 0.47
	ALBEDO = mix(under_color, foam_color, step(0.241, line));  // NEW: white above the line
}
