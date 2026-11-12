// STEP 1 — the stones' shape: which way each bit faces, as colour.
// stone_mesh.gd makes each stone (Blender's way): an icosphere split into 1280 triangles, pulled
// round into a ball, then every vertex pushed out or in along its normal by a 3D noise (lumps
// about 0.7 of its radius across), the bottom squashed flat. Afterwards the normals are worked out
// again from the new shape, so the light (next step) falls on the lumps. stone_scatter.gd places
// them by a seed, each its own size, stretch and turn. Here: the normal in the world as colour
// (x, y, z as red, green, blue; * 0.5 + 0.5: -1..1 to 0..1), a debug view, unshaded. MODEL_NORMAL_
// MATRIX turns a normal with the stone (a stretched stone's normals need it: the plain model
// matrix would bend them the wrong way).
shader_type spatial;
render_mode unshaded;

varying vec3 world_normal;  // NEW

void vertex() {
	world_normal = normalize(MODEL_NORMAL_MATRIX * NORMAL);  // NEW
}

void fragment() {
	ALBEDO = normalize(world_normal) * 0.5 + 0.5;  // NEW
}
