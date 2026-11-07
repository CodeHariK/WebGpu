// The column grid's 4 neighbour steps, shared by the water's column steps (1 flood, 2a - 2c) and
// their debug views: +x, -x, +z, -z, in that order, and their names in the trace texts.
#ifndef MC_WATER_STEPS_H
#define MC_WATER_STEPS_H

#include <godot_cpp/variant/vector2i.hpp>

namespace godot {
namespace mc_water {

const Vector2i STEPS[4] = { Vector2i(1, 0), Vector2i(-1, 0), Vector2i(0, 1), Vector2i(0, -1) };
const char *const SIDE_NAMES[4] = { "+x", "-x", "+z", "-z" };

} // namespace mc_water
} // namespace godot

#endif // MC_WATER_STEPS_H
