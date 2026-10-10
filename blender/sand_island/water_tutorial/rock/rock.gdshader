// Rock: RockMat from Rock.blend, rebuilt from one baked texture (rock_rg.png, 1024², data).
// The Blender material is:  colour = mix(#8C8C8C, #FFFFFF, edge) × mix(1, pattern, 0.425)
//   edge     two Ambient Occlusion nodes ("inside") pick out the convex edges → white edges
//   pattern  Voronoi cells × tiny Voronoi dots, + noise, × waves (the speckled stone)
// Those are two different numbers, so the bake keeps them apart:
//   R = edge      (how far from dark to light)
//   G = pattern   (already as the multiplier 1 − 0.425 + 0.425 × pattern)
// and this shader does the same two steps. dark / light can be any two colours; B is still free.
shader_type spatial;

uniform sampler2D rock_rg : hint_default_white, filter_linear_mipmap;  // data, not colour
uniform vec3 dark : source_color = vec3(0.549, 0.549, 0.549);   // #8C8C8C, where R = 0
uniform vec3 light : source_color = vec3(1.0);                  // #FFFFFF, where R = 1 (edges)

void fragment() {
	vec2 rg = texture(rock_rg, UV).rg;
	ALBEDO = mix(dark, light, rg.r) * rg.g;
	ROUGHNESS = 1.0;
	SPECULAR = 0.0;
}
