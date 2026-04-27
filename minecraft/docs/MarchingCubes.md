# Marching Cubes & Marching Squares

![Marching Cubes Diagram](./mc.excalidraw.svg)

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

To reduce modeling work, we generate all 256 configurations from a few base meshes using rotational symmetry.

| Operation | Axis | Corner Cyclic Swaps |
| :--- | :--- | :--- |
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

The number of base assets required depends on the level of symmetry in your project.

### Case A: Geometric Symmetry (Standard)
*   **Definition:** The shape is isotropic (e.g., a simple diagonal plane).
*   **Base Assets:** Only **15 unique cases** (Lorensen & Cline) are modeled.
*   **Workload:** Low. Code generates 256 configs via full octahedral rotation (24 orientations).

### Case B: Directional Symmetry (Minecraft-Style)
*   **Definition:** Shapes have orientation (e.g., "Top" grass vs "Bottom" dirt).
*   **Base Assets:** Typically **30+ base meshes**. For example, a 1-Corner mesh at the *Top* of a cube is treated as a different asset than one at the *Bottom* to prevent grass from flipping upside down.
*   **Tradeoff:** By restricting rotations primarily to the **Y-axis**, you preserve "Up/Down" integrity while allowing 90° horizontal turns.
