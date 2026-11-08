// Water, mobile: steps 1 - 5 (liquid_common.gdshaderinc), matte, opaque, no extra step.
// From the minecraft game's MC water: the game uses a copy (minecraft/project/material/shaders/
// terrain/liquid/), MCGrid picks it when the map's liquid is water. It reads the MC water mesh's data
// (COLOR flow, UV, UV2, depth: shader_lib/liquid_common.gdshaderinc); on another mesh it compiles
// but shows the defaults.
shader_type spatial;
render_mode cull_disabled; // falls are seen from both sides

#include "res://shader_lib/liquid_common.gdshaderinc"

// Steps 1 - 5: water's colours and speed.
uniform vec3 deep_color : source_color = vec3(0.07, 0.36, 0.75); // step 1 / 2
uniform vec3 shallow_color : source_color = vec3(0.45, 0.85, 0.95); // step 2: light near the shore
uniform float flow_speed = 0.9; // step 3: metres a second: water runs
uniform vec3 streak_color : source_color = vec3(0.85, 0.96, 1.0); // step 4: white with air
uniform vec3 foam_color : source_color = vec3(0.95, 0.99, 1.0); // step 5

void vertex() {
	depth = CUSTOM0.x; // step 2: this vertex's depth in metres
}

void fragment() {
	LiquidLook look = LiquidLook(deep_color, shallow_color, flow_speed, streak_color, foam_color);
	LiquidBase base = liquid_base(look, UV, UV2, COLOR, depth, TIME);
	ALBEDO = base.colour;
	ROUGHNESS = 1.0; // matte
	SPECULAR = 0.0;
}
