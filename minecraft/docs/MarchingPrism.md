## Overview

| Feature | Description |
| --- | --- |
| **Classification** | Isosurface Extraction / Surface Reconstruction |
| **Input** | Discrete scalar field (Triangular Prismatic Grid) |
| **Output** | Polygonal Mesh (Triangles) |
| **Mechanism** | Lookup-table based tiling. Marches through 6-vertex prisms, classifies vertices, and generates geometry from 64 possible configurations. |

---

## 1. Spatial & Bit Mapping

Since a triangular prism has 6 vertices, there are $2^6 = 64$ possible configurations. The implementation maps the corner indices directly to a 6-bit hash (0-63).

### 3D Triangular Prism Layout (Y-Up, Z-Near)

Following Godot space conventions (+Y is Up, +Z is Towards Player).
*Note: In a standard 3D triangular grid, prisms alternate pointing "Far" (-Z) and "Near" (+Z). This layout assumes a "Far-pointing" prism as the base.*

```text
          5 (Top Apex)                  Bit Index:    5 4 3 2 1 0 (Lsb)
         / \                            Corner Index: 5 4 3 2 1 0
        /   \
      3 ----- 4 (TR)                    T = Top,  B = Bottom
      | (TL)  |                         L = Left, R = Right
      |   2   | (Bottom Apex)           A = Apex (Far)
      |  / \  |
      | /   \ |                         Example: "000001" (Hash 1)
      0 ----- 1 (BR)                             = Only Corner 0 is Active
     (BL)

```

### Corner Definitions

| Index | Pos (X, Y, Z) | Nickname | Bit Weight |
| --- | --- | --- | --- |
| **0** | (-1, -1,  1) | BL (Bottom Left) | `1 << 0` |
| **1** | ( 1, -1,  1) | BR (Bottom Right) | `1 << 1` |
| **2** | ( 0, -1, -1) | BA (Bottom Apex) | `1 << 2` |
| **3** | (-1,  1,  1) | TL (Top Left) | `1 << 3` |
| **4** | ( 1,  1,  1) | TR (Top Right) | `1 << 4` |
| **5** | ( 0,  1, -1) | TA (Top Apex) | `1 << 5` |

---

## 2. Transformation Matrices & Symmetries

With only 64 configurations, manual LUT generation is feasible, but relying on symmetries reduces the workload.

| Operation | Axis | Corner Cyclic Swaps |
| --- | --- | --- |
| **Ry** | Y-Axis (120°) | `0→1, 1→2, 2→0` and `3→4, 4→5, 5→3` |
