/**
 * @file tr_water.h
 * @brief Shared toon water material for TerraSpline water surfaces.
 */
#ifndef TR_WATER_H
#define TR_WATER_H

#include <godot_cpp/classes/shader_material.hpp>

namespace godot {

/// A new ShaderMaterial with the built-in cartoon water shader. Uniforms: speed, alpha, foam_color,
/// stripe_repeat, bank_foam, bob.
Ref<ShaderMaterial> make_toon_water_material();

/// Release the cached water Shader. Call once at extension deinitialization, before Godot's renderer
/// shuts down, or it reports the shared Shader as leaked at exit.
void clear_water_material_cache();

} // namespace godot

#endif // TR_WATER_H
