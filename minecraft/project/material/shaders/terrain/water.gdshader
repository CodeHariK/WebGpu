// Water for MCGrid's water layer (src/marching_cubes/grid/mc_grid_water.cpp): pools, rivers and the
// bevelled waterfall strips (mc_grid_water_falls.cpp). For now just one plain blue, lit, opaque:
// any light / dark difference left between pieces comes from the mesh (its normals), not the shader.
// (The full look — depth colour, ripples, foam, streaks — is in git history: commit 8bb3c71.)
shader_type spatial;
render_mode cull_disabled;

uniform vec3 water_color : source_color = vec3(0.15, 0.55, 0.9);

void fragment() {
	ALBEDO = water_color;
	ROUGHNESS = 1.0;
	SPECULAR = 0.0;
}
