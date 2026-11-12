// STEP 1 — puffs: round white blobs rising off the froth.
// A GPUParticles3D throws quads up from along the froth tube (the particles' own settings: how
// many, how fast, how long; see the scene's Particles/Puffs). This shader draws each quad:
//   billboard  the vertex shader turns the quad to face the camera, keeping the particle's place
//              and size (Godot's particle billboard: the camera's own axes, the particle's position)
//   round      a circle cut out of the quad: ALPHA 1 inside, 0 outside, and ALPHA_SCISSOR_THRESHOLD
//              makes it a hard cut-out, not a blend: puffs draw like solid shapes (cheap: no
//              sorting, no layers of half-transparent pixels)
// Spray droplets (the small spheres flying off) are their own particles with a plain white
// material: they stay as they are.
shader_type spatial;
render_mode unshaded, cull_disabled;

uniform vec3 puff_color = vec3(2.2, 2.2, 2.3);  // NEW: linear, past white like the froth

void vertex() {
	// NEW: face the camera: the camera's axes, scaled by the particle's size, at the particle
	MODELVIEW_MATRIX = VIEW_MATRIX * mat4(INV_VIEW_MATRIX[0], INV_VIEW_MATRIX[1], INV_VIEW_MATRIX[2], MODEL_MATRIX[3]);  // NEW
	MODELVIEW_MATRIX = MODELVIEW_MATRIX * mat4(vec4(length(MODEL_MATRIX[0].xyz), 0.0, 0.0, 0.0),  // NEW
			vec4(0.0, length(MODEL_MATRIX[1].xyz), 0.0, 0.0), vec4(0.0, 0.0, length(MODEL_MATRIX[2].xyz), 0.0),
			vec4(0.0, 0.0, 0.0, 1.0));
}

void fragment() {
	float from_middle = length(UV - 0.5) * 2.0;  // NEW: 0 in the middle, 1 at the quad's edge
	ALBEDO = puff_color;  // NEW
	ALPHA = step(from_middle, 0.9);  // NEW: inside the circle
	ALPHA_SCISSOR_THRESHOLD = 0.5;  // NEW: a hard cut, no blending
}
