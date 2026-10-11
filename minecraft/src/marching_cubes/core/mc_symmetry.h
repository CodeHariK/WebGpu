// Turning a cell about Y, applied to its hash AND to the tile's transform together, so a turned
// tile mesh always matches its turned corner pattern. Used to fill the library from the modelled
// tiles. Tiles are only ever turned about Y (grass stays on top); mirrored or upside-down tiles
// are modelled in Blender.
#ifndef MC_SYMMETRY_H
#define MC_SYMMETRY_H

#include <cstdint>
#include <godot_cpp/variant/transform3d.hpp>

namespace godot::mc {

// Hash after one +90 degree turn about Y (right-handed, as Transform3D::rotated).
uint8_t rotate_y(uint8_t p_hash);

// Turn a hash and its tile transform by p_quarter_turns x 90 degrees about Y (0..3).
void turn_y(
		uint8_t &p_hash,
		Transform3D &p_transform,
		int p_quarter_turns
);

} // namespace godot::mc

#endif // MC_SYMMETRY_H
