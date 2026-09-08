/**
 * @file prop_material.h
 * @brief Shared semi-realistic stylized material for procedural environment props.
 */
#ifndef PROP_MATERIAL_H
#define PROP_MATERIAL_H

#include <godot_cpp/classes/shader_material.hpp>

namespace godot {

/**
 * @brief A new ShaderMaterial with the built-in prop shader (Odyssey / Link's Awakening look, not cel):
 * albedo from the vertex colour, smooth wrapped diffuse (`wrap` lets light bleed past the terminator like
 * a translucent solid), a tight specular highlight, a fresnel rim tinted towards `rim_color`,
 * back-light transmission (`transmission`, sun seen through the object) and an emission ramp along UV.y
 * (`glow`, 0 = off) so tips can glow. Uniforms: wrap, specular_strength, specular_power, rim_strength,
 * rim_power, rim_color, transmission, transmission_power, glow, glow_color, saturation.
 */
Ref<ShaderMaterial> make_prop_material(bool p_two_sided = false);

/// The prop material tuned for matte stone (low wrap, a whisper of specular, no transmission / glow).
Ref<ShaderMaterial> make_stone_material();

} // namespace godot

#endif // PROP_MATERIAL_H
