#ifndef PUZZLE_LIQUID_H
#define PUZZLE_LIQUID_H

#include <godot_cpp/classes/shader_material.hpp>

namespace godot {

/**
 * PuzzleLiquid — cartoon water and lava surfaces for puzzle islands (cheap, unlit-ish
 * toon shaders; pattern from world position + TIME, so tiles tile seamlessly).
 * Each call makes a new material (the grid owns it; no static resources alive at exit).
 */
namespace PuzzleLiquid {

Ref<ShaderMaterial> water_material();
Ref<ShaderMaterial> lava_material();

} // namespace PuzzleLiquid

} // namespace godot

#endif // PUZZLE_LIQUID_H
