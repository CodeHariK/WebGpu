shader_type spatial;

// Unshaded emissive glow for the folio car's light bits (energy cells, headlights).
// Pure emission so the scene's bloom lights them up like folio's web build.
render_mode unshaded;

uniform vec3 glow_color : source_color = vec3(1.0);
uniform float glow_strength = 3.0;

void fragment() {
	ALBEDO = glow_color;
	EMISSION = glow_color * glow_strength;
}
