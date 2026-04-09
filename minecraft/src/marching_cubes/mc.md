### New Spatial & Bit Mapping (Original Layout)

Following **Y-Up, Z-Near (Towards Player)** and your bit-order where **Corner 7 is MSB (`1 << 7`)**:

```text
Corners (Bottom Clockwise, then Top):
      7 (TLF) ----------- 6 (TRF)      Bit Index:    7 6 5 4 3 2 1 0 (Lsb)
     / |                 / |           String Index: [0][1][2][3][4][5][6][7]
    /  |                /  |           Corner Index: 7 6 5 4 3 2 1 0
   4 (TLN) ----------- 5 (TRN)
   |   |               |   |           Binary String "00000001" = Corner 0 ONLY
   |   3 (BLF) --------|-- 2 (BRF)
   |  /                |  /
   | /                 | /
   0 (BLN) ----------- 1 (BRN)   
   (-x,-y,+z)

   ^
  Towards 
  Player
```

### Bitwise Mapping Table (Corner Index == Bit Index)

| Operation | Corner Movement | Bit Swaps (Index) |
| :--- | :--- | :--- |
| **Ry** (Rotate Y) | `0→1, 1→2, 2→3, 3→0` | `0→1, 1→2, 2→3, 3→0` |
| | `4→5, 5→6, 6→7, 7→4` | `4→5, 5→6, 6→7, 7→4` |
| **Rx** (Rotate X) | `0→4, 4→7, 7→3, 3→0` | `0→4, 4→7, 7→3, 3→0` |
| | `1→5, 5→6, 6→2, 2→1` | `1→5, 5→6, 6→2, 2→1` |
| **Rz** (Rotate Z) | `0→4, 4→5, 5→1, 1→0` | `0→4, 4→5, 5→1, 1→0` |
| | `3→7, 7→6, 6→2, 2→3` | `3→7, 7→6, 6→2, 2→3` |
| **Sx** (Mirror X) | `0↔1, 3↔2, 4↔5, 7↔6` | `0↔1, 3↔2, 4↔5, 7↔6` |
