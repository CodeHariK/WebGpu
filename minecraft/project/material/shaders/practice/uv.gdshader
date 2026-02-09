shader_type spatial;

uniform float uv_x = 1.0;
uniform float uv_y = 1.0;

void vertex() {
	// Called for every vertex the material is visible on.
}

void fragment() {
    ALBEDO = vec3(fract(UV.x * uv_x), fract(UV.y * uv_y), 0);
}

//void light() {
//	// Called for every pixel for every light affecting the material.
//	// Uncomment to replace the default light processing function with this one.
//}
