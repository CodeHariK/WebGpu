# Marching Cubes & Marching Squares

![Marching Cubes Diagram](./mc.excalidraw.svg)

Code map (every file, function and the call order from `_ready`): [src/marching_cubes/MarchingCubes.md](../src/marching_cubes/MarchingCubes.md)

## Overview

| Feature | Description |
| :--- | :--- |
| **Classification** | Isosurface Extraction / Surface Reconstruction |
| **Input** | Discrete scalar field (Voxels / Grid values) |
| **Output** | Polygonal Mesh (Triangles/Quads) |
| **Mechanism** | Lookup-table based tiling. It "marches" through cells, classifies vertices, and places pre-defined geometry. |

---

## 1. Spatial & Bit Mapping

The implementation uses a strict mapping between corner indices and bit positions. This creates a deterministic 8-bit hash (0-255) for every cube configuration.

### 3D Cube Layout (Y-Up, Z-Near)
Following Godot space conventions where **+Y is Up** and **+Z is Towards Player**:

```text
       7 (TLF) ----------- 6 (TRF)      Bit Index:    7 6 5 4 3 2 1 0 (Lsb)
      / |                 / |           Corner Index: 7 6 5 4 3 2 1 0
     /  |                /  |
    4 (TLN) ----------- 5 (TRN)         T = Top,  B = Bottom
    |   |               |   |           L = Left, R = Right
    |   3 (BLF) --------|-- 2 (BRF)     N = Near, F = Far
    |  /                |  /
    | /                 | /             Example: "00000001" (Hash 1)
    0 (BLN) ----------- 1 (BRN)                  = Only Corner 0 is Active
```

### Corner Definitions
| Index | Pos (X, Y, Z) | Nickname | Bit Weight |
| :--- | :--- | :--- | :--- |
| **0** | (-1, -1,  1) | BLN (Bottom Left Near) | `1 << 0` |
| **1** | ( 1, -1,  1) | BRN (Bottom Right Near) | `1 << 1` |
| **2** | ( 1, -1, -1) | BRF (Bottom Right Far) | `1 << 2` |
| **3** | (-1, -1, -1) | BLF (Bottom Left Far) | `1 << 3` |
| **4** | (-1,  1,  1) | TLN (Top Left Near) | `1 << 4` |
| **5** | ( 1,  1,  1) | TRN (Top Right Near) | `1 << 5` |
| **6** | ( 1,  1, -1) | TRF (Top Right Far) | `1 << 6` |
| **7** | (-1,  1, -1) | TLF (Top Left Far) | `1 << 7` |

---

## 2. Transformation Matrices

To reduce modeling work, the game fills the 256 configurations from the modelled tiles by turning them about Y only (`mc::turn_y` in `core/mc_symmetry.cpp`).

| Operation | Axis | Corner Cyclic Swaps |
| :--- | :--- | :--- |
| **Ry** | Y-Axis | `0→1, 1→2, 2→3, 3→0` and `4→5, 5→6, 6→7, 7→4` |

Turns about X or Z and mirroring are **not** used: they would put grass on walls and ceilings, and mirroring a mesh at runtime breaks its normals and textures. Mirrored or upside-down tiles are modelled in Blender instead.

> [!TIP]
> **Horizontal Symmetry (Y-Rotations)** preserves the "Up" vector, preventing grass from appearing on ceilings or floors.

--- | :--- | :--- |
| **Ry** | Y-Axis | `0→1, 1→2, 2→3, 3→0` and `4→5, 5→6, 6→7, 7→4` |
| **Rx** | X-Axis | `0→4, 4→7, 7→3, 3→0` and `1→5, 5→6, 6→2, 2→1` |
| **Rz** | Z-Axis | `0→4, 4→5, 5→1, 1→0` and `3→7, 7→6, 6→2, 2→3` |
| **Sx** | Mirror X | `0↔1, 3↔2, 4↔5, 7↔6` |

> [!TIP]
> **Horizontal Symmetry (Y-Rotations)** is the most common for architecture/buildings as it preserves the "Up" vector, preventing grass from appearing on ceilings or floors.

---

## 3. Topological Connectivity

The algorithm must maintain consistency around a shared vertex to ensure a **watertight** (manifold) mesh.

### 2D Connectivity (Squares)
A vertex is shared by **4 squares**. To form a continuous loop:
*   **Shared Boundaries:** 4 edges radiating from the vertex.
*   **Consistency:** The squares must "agree" on the interpolation point along those 2 vertical and 2 horizontal edges.

### 3D Connectivity (Cubes)
A vertex is shared by **8 cubes**. To form a continuous surface:
*   **Shared Boundaries:** 12 faces meeting at the vertex.
*   **Axis Distribution:**
    *   **4 faces** parallel to XY plane (separating Z).
    *   **4 faces** parallel to XZ plane (separating Y).
    *   **4 faces** parallel to YZ plane (separating X).

> [!IMPORTANT]
> This "Corner Case" logic is why rotational variants must be bit-perfect. If a rotated "1-Corner" mesh doesn't align its face-edges correctly, you will see **cracks or gaps** in the terrain.

---

## 4. Symmetry & Mesh Strategy

`MCTileLibrary` loads whatever tiles the `.glb` contains and fills every empty hash by turning a loaded tile about Y. There is no mode setting: the tile set decides.

| Tile set | Tiles to model | Result in Godot |
| :--- | :--- | :--- |
| **XZ Symmetric (68)** | one per Y-turn group | all 254 drawable hashes |
| **Heightmap (22)** | the 68 set without overhangs (a top corner always has a solid corner below it) | the 79 hashes a heightmap can produce |
| **Complete (256)** | every hash (254; empty is never drawn, full is optional) | nothing to fill |

Godot prints which hashes are still missing after loading. The Blender add-on in `blender/mc_tiles/mc_tile_tools` shows the same check while modelling (missing tiles, rotated copies, overhangs in a heightmap set).

A set using all 24 cube rotations (21 tiles) was dropped: it flips tiles on their side and upside down, which does not suit directional (grass-on-top) tiles.

### Mesh variants

Several meshes can be modelled for one hash: `00001111_circle`, `00001111_circle_v2`, `00001111_circle_v3`
(no ending = variant 1; Blender's `.001` and Godot's `_001` after it are ignored). `MCTileLibrary` keeps every
variant of a hash and turns them all; each cell picks one with `mc::variant_index(cell, count)`
(`core/mc_variant.h`): a hash of the cell's grid position, so the choice looks random but never changes
between rebuilds or loads. The Blender add-on uses the same formula, so its playground matches the game.

### Chunk meshes

`MCGrid` draws each chunk as one `MeshInstance3D`: every tile of the chunk is placed and appended into
one `ArrayMesh` (one surface per material) by `MCChunkBuilder` (`grid/mc_chunk_builder.*`), with one
`ConcavePolygonShape3D` collider on `LAYER_TERRAIN`. Tile mesh arrays are read once and cached
(`MCSurfaceCache`). A corner edit marks the chunks of the 8 cells around it dirty; they are rebuilt
once at the end of the frame (`flush_dirty_chunks()`), so many edits in one frame cost one rebuild
per chunk. Chunk size trades culling and rebuild cost against the number of objects.

See also `docs/TerrainStorage.md`: grid modes (height / corner / sparse), layers (material, water,
objects), waterfalls, save format v2 with zstd, size per user.

### Names used in the code

| Name | Meaning |
| :--- | :--- |
| **corner** | a grid point, solid or empty (`MCGrid::set_corner`, `is_corner_active`) |
| **cell** | the cube between 8 corners; its **hash** is the 8 corner bits (`get_cell_hash`) |
| **chunk** | a block of cells stored and drawn together (`Chunk`, one merged mesh + collider) |
| **tile** | the mesh drawn for one hash (`MCTile`: variant meshes + Y `turn` + `source_hash`) |
| **tile set** | the `.glb` of modelled tiles, exported by the Blender add-on (`tile_set_path`) |
| **tile library** | `MCTileLibrary`: `tiles[256]` filled from the tile set by Y turns |
| **variant** | another mesh for the same hash (`_v2`), picked per cell (`variant_meshes`) |
| **placed object** | a box placed in object mode (`MCPlacedObjects`) |

