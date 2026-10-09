// STEP 2 — how high is this pixel above the water?
// The water stored its depth in the mesh; the ground doesn't need to: every pixel knows its own
// world position (MODEL_MATRIX turns the mesh's local position into world space), and its height
// above the sea is just world y − water_level. Shown as grey: black at the waterline, white 2 m up.
shader_type spatial;

uniform vec3 sand_dry : source_color = vec3(0.98, 0.86, 0.62);
uniform float water_level = 0.0;  // NEW

varying vec3 world_pos;  // NEW

void vertex() {
	world_pos = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;  // NEW
}

void fragment() {
	float h = world_pos.y - water_level;    // NEW: height above the water
	ALBEDO = vec3(clamp(h / 2.0, 0.0, 1.0)); // NEW: debug view
	ROUGHNESS = 1.0;
}
