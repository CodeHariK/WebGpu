// Corner layout of one marching-cubes cell. Godot axes: +Y up, +Z towards the viewer.
// Bit i of a cell's 8-bit hash is corner i (1 = solid). See docs/MarchingCubes.md.
//
//        7 (TLF) ----------- 6 (TRF)
//       / |                 / |
//      4 (TLN) ----------- 5 (TRN)        T/B = top/bottom, L/R = left/right,
//      |   |               |   |          N/F = near (+Z) / far (-Z)
//      |   3 (BLF) --------|-- 2 (BRF)
//      |  /                |  /
//      0 (BLN) ----------- 1 (BRN)
//
// In grid coordinates (cell at x, y, z): c0 = (x, y, z+1), c1 = (x+1, y, z+1), c2 = (x+1, y, z),
// c3 = (x, y, z); c4..c7 are the same four one step up (y+1).
#ifndef MC_CORNERS_H
#define MC_CORNERS_H

#include <cstdint>

namespace godot::mc {

enum Corner : uint8_t { C0 = 0, C1, C2, C3, C4, C5, C6, C7 };

inline constexpr int CORNER_COUNT = 8;
inline constexpr int HASH_COUNT = 256; // every on/off combination of 8 corners

} // namespace godot::mc

#endif // MC_CORNERS_H
