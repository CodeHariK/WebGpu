// STEP 1 — a flat colour.
// A spatial shader runs fragment() once for every pixel of the mesh on screen.
// Whatever we put in ALBEDO is that pixel's colour. "unshaded" = ignore lights (flat, cartoon).
shader_type spatial;
render_mode unshaded;

void fragment() {
	ALBEDO = vec3(0.12, 0.42, 0.85);  // NEW: every pixel the same blue (r, g, b from 0 to 1)
}
