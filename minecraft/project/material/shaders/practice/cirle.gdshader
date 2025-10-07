shader_type canvas_item;

#include "library.gdshaderinc"

void vertex() {
	// Called for every vertex the material is visible on.
}

void fragment() {
	vec2 uv = UV - 0.5;
    vec3 color = vec3(0.0);
    color += draw_circle(uv + 0.3, 0.1, 0.01);
    COLOR = vec4(color, 1.0); 
}

//void light() {
//	// Called for every pixel for every light affecting the CanvasItem.
//	// Uncomment to replace the default light processing function with this one.
//}
