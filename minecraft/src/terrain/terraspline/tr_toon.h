/**
 * @file tr_toon.h
 * @brief Shared toon solid material for TerraSpline vertex-coloured meshes (cliffs, roads, rocks).
 */
#ifndef TR_TOON_H
#define TR_TOON_H

#include <godot_cpp/classes/shader_material.hpp>

namespace godot {

/**
 * @brief A new ShaderMaterial with the built-in cartoon solid shader: albedo from the vertex colour
 * (sRGB), diffuse quantized into `bands` steps whose darkest step is `shadow_level` of the lit colour
 * tinted by `shadow_tint` (never black), shadows quantized the same way, a `rim_strength` rim light,
 * specular off. Uniforms: bands, band_softness, shadow_level, shadow_tint, rim_strength, rim_width,
 * rim_color, saturation. Cheap enough for mobile: one light() with no texture reads.
 */
Ref<ShaderMaterial> make_toon_solid_material();

/**
 * @brief The solid toon material plus lane markings drawn from the road mesh's UV2 (0..1 across the
 * deck, on-deck flag) and UV.y: `lanes` − 1 dividers (dashed or solid), optional solid edge lines.
 * Uniforms on top of the solid ones: deck_width, texture_length, lanes, dashed, edge_lines,
 * line_width, dash_length, edge_inset, line_color. TerrainSplineRoad sets them from its properties.
 */
Ref<ShaderMaterial> make_toon_road_material();

/**
 * @brief Inverted-hull outline pass for a toon material's `next_pass`: front faces culled, the mesh
 * pushed `p_width` metres along its normals, flat `p_color`. Best on smooth-shaded meshes; flat-shaded
 * ones (split normals) show small cracks at hard edges, so the cliff leaves it off by default.
 */
Ref<ShaderMaterial> make_toon_outline_material(
		float p_width,
		const Color &p_color
);

/// Release the cached toon Shaders (solid, road, outline). Call once at extension deinitialization,
/// before Godot's renderer shuts down, or it reports the shared Shaders as leaked at exit.
void clear_toon_material_cache();

} // namespace godot

#endif // TR_TOON_H
