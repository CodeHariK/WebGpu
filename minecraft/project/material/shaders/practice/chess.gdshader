shader_type canvas_item;

#include "library.gdshaderinc"

void vertex() {
	// Called for every vertex the material is visible on.
}

void fragment() {
    float zoom = 2.0;
    float square = 8.0;
    vec2 uv = to_polar(UV);
    uv.y /= TAU;
    uv.y = uv.y + TIME * 0.05;
    float color = fract((floor(uv.x * square * zoom) + floor(uv.y * square * zoom)) * 0.5) * 2.0;
    COLOR = vec4(color,color,color, 1.0);
}

//void light() {
//	// Called for every pixel for every light affecting the CanvasItem.
//	// Uncomment to replace the default light processing function with this one.
//}
