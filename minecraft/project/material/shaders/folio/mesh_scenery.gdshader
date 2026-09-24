shader_type spatial;

// =============================================================================
// Folio scenery material — MeshDefaultMaterial (folio Materials/) for objects
// that colour themselves from a shared PALETTE texture (UV -> swatch) instead of
// a single base_color. Runs the palette colour through the exact same folio
// pipeline (light bounce, core + drop shadows, fog) so props match the world.
// =============================================================================

render_mode cull_back, ambient_light_disabled;

#include "res://material/shaders/folio/folio_material.gdshaderinc"

uniform sampler2D palette : source_color, filter_linear;

// Base colour sampled in fragment(), carried to light() for the shadow tint.
varying vec3 v_base;

void vertex() {
	v_world_pos = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;
	v_world_normal = normalize(mat3(MODEL_MATRIX) * NORMAL);
}

void fragment() {
	v_base = texture(palette, UV).rgb;
	ALBEDO = folio_shade(v_base, SCREEN_UV, -VERTEX.z);
}

void light() {
	DIFFUSE_LIGHT = folio_light_shadow(ALBEDO, v_base, ATTENUATION);
}
