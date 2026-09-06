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

} // namespace godot

#endif // TR_WATER_H
