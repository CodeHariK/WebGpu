// The liquid of MCGrid's water layer (src/marching_cubes/water/): the flat surfaces and the falls,
// one mesh and one material for the whole map, so the map is one draw. One liquid per map:
// MCGrid's `liquid` (Water tab: "Liquid") sets the `liquid` uniform below.
//
// Built one idea per step; each step adds one thing on top of the last:
//   step 1  a flat colour per liquid                                        <- this file so far
//   next    depth, flow, falls, foam, lava crust and glow, poison bubbles
//
// What the mesh gives the shader per vertex (src/marching_cubes/water/build/mc_water_build.cpp):
//   COLOR      the flow (water step 2c): rg the direction (x, z) as 0..1 (0.5 = none), b 1 where
//              it flows, a 1 where a fall lands (foam). The Water tab's "Water view: flow" draws it.
//   UV2        x: 0 on the flat water, 0..1 over a fall's lip, 1 and more down the fall;
//              y: metres above the landing on a fall, -1 on a stream
//   CUSTOM0.x  the depth in metres (surface to ground)
// The old water look (depth colour, ripples, foam, streaks) is in git history: commit 8bb3c71.
shader_type spatial;
render_mode cull_disabled; // falls are seen from both sides

// Which liquid this map has. A uniform is the same for every pixel of the draw, so the `if`s
// below cost nothing: every pixel takes the same branch.
uniform int liquid : hint_enum("Water", "Lava", "Green poison") = 0;

// Step 1: each liquid's colour.
uniform vec3 water_color : source_color = vec3(0.15, 0.55, 0.9);
uniform vec3 lava_color : source_color = vec3(1.0, 0.35, 0.05);
uniform vec3 poison_color : source_color = vec3(0.35, 0.85, 0.15);

void fragment() {
	// step 1: this map's liquid colour
	vec3 colour = water_color;
	if (liquid == 1) {
		colour = lava_color;
	} else if (liquid == 2) {
		colour = poison_color;
	}
	ALBEDO = colour;
	ROUGHNESS = 1.0; // matte: no shiny highlight yet
	SPECULAR = 0.0;
}
