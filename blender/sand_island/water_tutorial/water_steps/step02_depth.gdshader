// STEP 2 — look at the data: how deep is the water here?
// The mesh carries extra data per vertex: the water depth is stored in the vertex colour
// (red channel: 0 at the shore, 1 at 2 m deep) — baked by Blender for the glb island, computed at
// runtime by IslandMeshBuilder for the procedural one. vertex() runs once per vertex; a "varying"
// carries a value from vertex() to fragment(), smoothly blended across each triangle.
// Here we just SHOW the depth as grey: black at the shore, white in deep water.
shader_type spatial;
render_mode unshaded;

varying float depth;  // NEW: passes a value from vertex() to fragment()

void vertex() {
	depth = COLOR.r;  // NEW: the vertex colour's red channel = depth
}

void fragment() {
	ALBEDO = vec3(depth);  // NEW: grey = depth (debug view — a shader's "print")
}
