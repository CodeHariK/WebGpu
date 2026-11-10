// STEP 1 — one colour: the poison's own green.
// A spatial shader runs fragment() once for every pixel of the mesh it is on: here the minecraft
// game's MC water mesh (sample/water.res: a pool, the stream into it and a fall). ALBEDO is the
// colour the light falls on; a uniform is a value you set in the Inspector, so the whole look so
// far is one green. ROUGHNESS 1 and SPECULAR 0 make it matte: thick goo, no shiny highlights.
// cull_disabled draws both sides of each triangle (a fall can be seen from behind).
shader_type spatial;
render_mode cull_disabled;

uniform vec3 deep_color : source_color = vec3(0.2, 0.55, 0.08);  // NEW: source_color: a colour picker

void fragment() {
	ALBEDO = deep_color;  // NEW
	ROUGHNESS = 1.0;  // NEW: matte
	SPECULAR = 0.0;  // NEW
}
