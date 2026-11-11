// STEP 4 — triplanar: lay a pattern on the world from three sides, no UV. (C: two views)
// A pattern needs 2D coordinates. Instead of UV, take the pixel's world position and look at it
// from each side: from the top (x, z), from the x side (z, y) and from the z side (x, y). Each view
// gives a noise; blend the three by how much the surface faces that side (the normal's size along
// x, y, z: the weights, adding up to 1). pow() sharpens them, so one side wins quickly and the blend
// zone stays narrow (a soft blend looks smeared). Whatever the mesh, the pattern is the right size
// everywhere and has no seams; falls get the side views, the flat water the top one. Debug, unshaded:
//   weights  x red, y green, z blue: green on the flat, red or blue down a fall
//   noise    a still noise, 2 blobs a metre, laid on from the three sides, as grey
shader_type spatial;
render_mode cull_disabled, unshaded;  // NEW: unshaded for the debug view

#include "res://shader_lib/liquid_noise.gdshaderinc"  // NEW: noise()

uniform float sharpness = 4.0;  // NEW: how quickly the three sides hand over
uniform int show_channel : hint_enum("weights", "noise") = 0;  // NEW: C in the viewer

varying vec3 world_pos;  // NEW: metres, in the world
varying vec3 world_normal;

void vertex() {
	world_pos = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;  // NEW: 1.0: a point, moved as well as turned
	world_normal = normalize((MODEL_MATRIX * vec4(NORMAL, 0.0)).xyz);
}

void fragment() {
	vec3 n = normalize(world_normal);
	vec3 w = pow(abs(n), vec3(sharpness));  // NEW: how much each side faces, sharpened
	w /= w.x + w.y + w.z;  // NEW: adding up to 1
	float side_x = noise(world_pos.zy * 2.0);  // NEW: seen from the x side
	float top = noise(world_pos.xz * 2.0);  // NEW: seen from above
	float side_z = noise(world_pos.xy * 2.0);  // NEW: seen from the z side
	float pattern = side_x * w.x + top * w.y + side_z * w.z;  // NEW
	ALBEDO = show_channel == 1 ? vec3(pattern) : w;  // NEW
}
