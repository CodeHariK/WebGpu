// Rotations and the mirror of a cell, applied to its hash AND to the tile's transform together,
// so a rotated tile mesh always matches its rotated corner pattern. Used to build all 256
// library entries from a few base meshes.
#ifndef MC_SYMMETRY_H
#define MC_SYMMETRY_H

#include <cstdint>
#include <godot_cpp/variant/transform3d.hpp>

namespace godot {

// One symmetry operation: 90/180/270 degree turns about X, Y or Z, or a mirror across X.
enum MCTransform : uint8_t { RX90, RX180, RX270, RY90, RY180, RY270, RZ90, RZ180, RZ270, SX };

namespace mc {

// Hash after a +90 degree turn about one axis (right-handed, as Transform3D::rotated).
uint8_t rotate_x(uint8_t p_hash);
uint8_t rotate_y(uint8_t p_hash);
uint8_t rotate_z(uint8_t p_hash);
// Hash after mirroring across the X axis (left <-> right).
uint8_t mirror_x(uint8_t p_hash);

// Apply one operation to a hash and the matching transform.
void apply_transform(
		uint8_t &p_hash,
		Transform3D &p_transform,
		MCTransform p_op
);

} // namespace mc
} // namespace godot

#endif // MC_SYMMETRY_H
