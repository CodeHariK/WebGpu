// STEP 2 — look at the data: how deep is the poison here?
// The game's water mesh carries extra numbers per vertex. CUSTOM0.x is the depth in metres (from
// the surface down to the ground), worked out by the game when it builds the mesh. vertex() runs
// once per vertex; a "varying" carries a value from vertex() to fragment(), blended smoothly
// across each triangle. Here the depth is only SHOWN, as grey: black at the shore, white at
// depth_range metres and deeper (a debug view, a shader's "print"; unshaded: no light).
shader_type spatial;
render_mode cull_disabled, unshaded;  // NEW: unshaded for the debug view

uniform float depth_range : hint_range(0.1, 4.0) = 1.2;  // NEW: metres from shore to full colour

varying float depth;  // NEW: passes a value from vertex() to fragment()

void vertex() {
	depth = CUSTOM0.x;  // NEW: this vertex's depth in metres
}

void fragment() {
	ALBEDO = vec3(smoothstep(0.0, depth_range, depth));  // NEW: 0 at the shore .. 1 deep, as grey
}
