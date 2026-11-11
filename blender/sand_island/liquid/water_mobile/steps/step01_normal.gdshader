// STEP 1 — look at the data: which way does the water face? (C in the viewer: two views)
// The mesh has a NORMAL per vertex: an arrow 1 long pointing out of the surface. vertex() turns it
// from the mesh's own space into the world's with MODEL_MATRIX (the 0.0 at the end: a direction,
// only turned, never moved) and hands it to fragment() in a varying, blended across each triangle.
// Blending shortens it a little, so fragment() makes it 1 long again (normalize). No UV anywhere:
// the whole look will come from which way the water faces and where it is. Shown as colour (a
// debug view, unshaded: no light):
//   normal   x, y, z as red, green, blue (* 0.5 + 0.5: -1..1 to 0..1): the flat water faces up
//            (green), each side of a fall a colour of its own
//   up only  normal.y as grey: white on the flat, black down a fall, grey where it bends over
shader_type spatial;
render_mode cull_disabled, unshaded;  // NEW: cull_disabled: a fall can be seen from behind

uniform int show_channel : hint_enum("normal", "up only") = 0;  // NEW: C in the viewer

varying vec3 world_normal;  // NEW: which way the surface faces, in the world

void vertex() {
	world_normal = normalize((MODEL_MATRIX * vec4(NORMAL, 0.0)).xyz);  // NEW
}

void fragment() {
	vec3 n = normalize(world_normal);  // NEW: 1 long again after blending
	ALBEDO = show_channel == 1 ? vec3(clamp(n.y, 0.0, 1.0)) : n * 0.5 + 0.5;  // NEW
}
