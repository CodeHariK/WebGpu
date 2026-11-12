// STEP 2 — two tones: a lit side and a shaded side, nothing in between.
// Blender's "rocks" material lights a grey stone (Diffuse BSDF), turns the light into a number
// (Shader to RGB) and cuts it once: over 0.468 the lit tone, under it the shaded one. For its sun
// (strength 3.17) that cut comes where the stone faces it at about 58 degrees or less (cos 0.53).
// Both tones are very dark teal-greys (the tonemap lifts them), brightened 1.3 x at the end. The
// sun's direction is the Blender file's.
shader_type spatial;
render_mode unshaded;

uniform vec3 lit_color = vec3(0.016, 0.037, 0.041);  // NEW: linear, Blender's
uniform vec3 shade_color = vec3(0.0086, 0.0206, 0.0238);  // NEW
uniform float value = 1.3;  // NEW: the last brightening (Blender's Hue / Saturation / Value)
uniform vec3 sun_direction = vec3(-0.777, 0.602, 0.370);  // NEW: toward the sun, Godot's axes
uniform float lit_cut = 0.53;  // NEW: facing the sun more than this (cos): lit

varying vec3 world_normal;

void vertex() {
	world_normal = normalize(MODEL_NORMAL_MATRIX * NORMAL);
}

void fragment() {
	float facing = dot(normalize(world_normal), normalize(sun_direction));  // NEW: 1 facing the sun
	vec3 colour = facing >= lit_cut ? lit_color : shade_color;  // NEW
	ALBEDO = colour * value;  // NEW
}
