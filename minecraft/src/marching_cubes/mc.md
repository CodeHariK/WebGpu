### New Spatial & Bit Mapping

Following **Y-Up, Z-Near (Towards Player)** and your bit-order where **Corner 0 is MSB (`1 << 7`)**:

```text
Corners (Bottom Clockwise, then Top):
      7 (TLF) ----------- 6 (TRF)      Bit:  0 1 2 3 4 5 6 7
     / |                 / |           String: [0][1][2][3][4][5][6][7]
    /  |                /  |           Corner:  0  1  2  3  4  5  6  7
   4 (TLN) ----------- 5 (TRN)
   |   |               |   |           Binary String "10000000" = Corner 0 ONLY
   |   3 (BLF) --------|-- 2 (BRF)
   |  /                |  /
   | /                 | /
   0 (BLN) ----------- 1 (BRN)   
   (-x,-y,+z)

   ^
  Towards 
  Player
```

### Bitwise Mapping Table

| Operation | Corner Movement | Bit Swaps (Index) |
| :--- | :--- | :--- |
| **Ry** (Rotate Y) | `0→1, 1→2, 2→3, 3→0` | `7→6, 6→5, 5→4, 4→7` |
| | `4→5, 5→6, 6→7, 7→4` | `3→2, 2→1, 1→0, 0→3` |
| **Rx** (Rotate X) | `0→3, 3→7, 7→4, 4→0` | `7→4, 4→0, 0→3, 3→7` |
| | `1→2, 2→6, 6→5, 5→1` | `6→5, 5→1, 1→2, 2→6` |
| **Rz** (Rotate Z) | `0→1, 1→5, 5→4, 4→0` | `7→6, 6→2, 2→3, 3→7` |
| | `3→2, 2→6, 6→7, 7→3` | `4→5, 5→1, 1→0, 0→4` |
| **Sx** (Mirror X) | `0↔1, 3↔2, 4↔5, 7↔6` | `7↔6, 4↔5, 3↔2, 0↔1` |
