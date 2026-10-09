// STEP 1 — a lit colour.
// The water was "unshaded" (flat). Sand should react to the sun, so we leave lighting on: Godot
// takes ALBEDO and lights it for us — bright towards the sun, dark away from it, cast shadows.
// "source_color" means: the colour you pick in the Inspector is converted from screen colour
// (sRGB) to the linear colour that lighting maths needs. ROUGHNESS 1 = matte, no shiny spot.
shader_type spatial;

uniform vec3 sand_dry : source_color = vec3(0.98, 0.86, 0.62);  // NEW

void fragment() {
	ALBEDO = sand_dry;  // NEW
	ROUGHNESS = 1.0;    // NEW
}
