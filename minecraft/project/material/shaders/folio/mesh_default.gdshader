shader_type spatial;

// =============================================================================
// Folio "MeshDefaultMaterial" (folio-2025 Game/Materials/MeshDefaultMaterial.js).
// The whole stylized pipeline lives in the shared include; this material just
// feeds its base colour through it. Base colour = base_color uniform (folio
// _colorNode), white by default.
// =============================================================================

// Shaded (not unshaded) so the sun's light() pass can read real cast-shadow
// ATTENUATION; ambient disabled so DIFFUSE_LIGHT alone is the final colour.
render_mode cull_back, ambient_light_disabled;

#include "res://material/shaders/folio/folio_material.gdshaderinc"

void vertex() {
	v_world_pos = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;
	v_world_normal = normalize(mat3(MODEL_MATRIX) * NORMAL);
}

void fragment() {
	ALBEDO = folio_shade(base_color.rgb, SCREEN_UV, -VERTEX.z);
}

void light() {
	DIFFUSE_LIGHT = folio_light_shadow(ALBEDO, folio_base_color(), ATTENUATION);
}
