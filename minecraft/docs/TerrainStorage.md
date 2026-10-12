# Terrain Storage: grid modes, layers, water and save format

Design notes for how MCGrid terrain is stored, saved and extended with more types (materials,
water, paths, objects). Status: **plan** — today only the "corner" mode with one terrain type exists.

---

## 1. What we save today (`.mct`, version 1)

- Header: magic `MCTR`, version, grid size (chunks per axis), chunk size (cells per axis).
- Then every chunk separately. A chunk stores **all its corners as on/off bits** in a 3D block,
  `(size + 1)³` per chunk, ordered y → z → x (layer by layer).
- The bits are packed with a **bit run-length encoding** (`src/utils/encoding/rle.*`):
  a run of equal bits costs 1 byte (up to 63 long) or 2 bytes (up to 16 383).
  ```
  [S:1][T:1][L:6]          short run (T = 0), length L
  [S:1][T:1][LH:6][LL:8]   long run  (T = 1), length (LH << 8) | LL
  ```
- The RLE is good on smooth terrain (layer-by-layer order turns flat ground into long runs) and
  weak on bumpy terrain (every layer changes often).

## 2. Heightmap mode would shrink it 4–6×

In heightmap mode every corner column only needs **one number: its height**.

Measured on generated terrain (same terrain, stored different ways):

| Terrain | Raw bits | **Now** (per-chunk bit RLE) | Heights, 1 byte / column | **Heights + zstd/zlib** |
|---|---|---|---|---|
| Smooth hills 64×16×64 | 8.8 KB | 1.7 KB | 4.1 KB | **0.46 KB** |
| Bumpy 64×16×64 | 8.8 KB | 5.5 KB | 4.1 KB | **1.1 KB** |
| Big island 256×32×256 | 266 KB | 82 KB | 65 KB | **14 KB** |

Only compressing today's 3D bits with zlib/zstd (no format change) gives 0.9 / 2.0 / 27 KB:
2–3× better than now, but heights are still another 2× smaller.

What else heightmap mode changes:

- **Memory:** 1 byte per column instead of 1 bit per corner per layer (32-high grid: 1 byte vs
  33 bits, about 4× less RAM).
- **Editing:** a click raises / lowers a whole column, so an overhang can never be created by
  accident. Every cell uses one of the 22 heightmap tiles (= the add-on's Heightmap (22) set).
- **Faster:** a cell's hash comes straight from 4 column heights.
- **Cost:** no caves, overhangs or arches in that grid. Bridges / arches become placed objects or a
  separate full-3D grid.

## 3. What is zstd

**Zstandard (zstd)** is a general-purpose compression format released by Facebook in 2016: very fast
to pack and unpack, compresses as well as or better than zlib. It is **built into Godot**:
`PackedByteArray.compress(FileAccess.COMPRESSION_ZSTD)` / `decompress(...)`, and compressed file
access. No extra library.

**Use it for both modes:** compress each chunk's layers with zstd (also gives corner mode its free
2–3×).

---

## 4. Grid modes

There are more than three ways to store a grid:

| Mode | Stores | Can do | Used by |
|---|---|---|---|
| **Height** (2.5D) | one height per column | hills, cliffs, terraces; no caves | Animal Crossing-style islands; our `MCHeightmap` |
| **Corner** (3D bits) | solid / empty per corner | anything: caves, overhangs, arches | our MCGrid today |
| **Cell / block** (3D types) | one type id per cell | anything, a type per block | **Minecraft** |
| **Column spans** | per column, a few (start, end) solid spans | caves / overhangs, almost as small as height | some voxel engines |
| **Density** (smooth) | a value per corner, not just on/off | smooth caves and blobs | Astroneer-style smooth terrain |
| **Sparse** | list of (position, type, rotation) | objects: trees, fences, rocks | everyone, for objects |

**Minecraft is not corner mode.** It is cell / block mode: each 1×1×1 cell holds a block type and is
drawn as a cube. Our marching tiles use corners, which is why we get slopes and rounded shapes.
Chunks are 16×16×16 sections, each with a **palette** (the block types used in that section) plus
**bit-packed indices** into it, and each chunk is zlib-compressed.

**For an Animal Crossing-like world, height mode is the better fit**; keep corner mode only where
caves or arches are really needed.

## 5. How Animal Crossing does it (from memory; not looked up)

From what fans worked out by editing save files of *New Horizons* (I couldn't search the web when
writing this, so treat it as approximate):

- The island is a fixed grid of about **112 × 96 tiles** with a few cliff levels (about 3 above the
  beach). Each tile has an **elevation** and a **terrain kind** (land / water / path).
- It is a set of **separate layers over the same squares**: terrain (cliff levels, water / river
  tiles), paths / custom ground designs, two item layers (objects on the ground, things on top), and
  buildings as a list of positions. Each layer is a flat 2D array of small id numbers.
- No overhangs: essentially **height mode with types on top**.

**Water, waterfalls and ponds in Animal Crossing:**

- Water is not a 3D volume: it is a **terrain kind on the tile** (a river / pond tile on some cliff
  level), drawn a bit below that level's ground.
- **Waterfalls are not stored.** They appear automatically wherever a river tile on a higher level
  meets a river tile on a lower level across a cliff edge.
- Rivers connect down level by level to the sea; a **pond** is water tiles that don't connect anywhere.

(Pokopia: I don't know how it stores its world.)

---

## 6. Types: layers that share the chunk grid

Today we only have "terrain". More types become **layers of the same grid**, not separate grids:

| Layer | Stored per | Data | Example |
|---|---|---|---|
| Shape | corner column (height mode) or corner (corner mode) | height, or on/off bits | the terrain we have now |
| Surface material | column (or cell) | small id | grass, sand, snow, stone |
| Ground overlay | column | small id | dirt path, road, flowers |
| Water | column | water surface height (0 = dry) | ponds, rivers |
| Objects | sparse list | type + cell + rotation | trees, fences, rocks (today's placed objects) |

- **Same grid, same chunks:** every layer uses the chunk's coordinates; a chunk holds all its layers;
  an edit to any layer marks the chunk for rebuild.
- **The mode is per grid** (height or corner); layers attach to that grid's columns / cells.
- **Layers overlap, and both render** — it is not "whichever is on top wins": terrain from the height
  layer, water drawn at its level wherever that is above the terrain. Water below the ground simply
  doesn't show (dry).
- **Rendering stays merged:** the chunk builder reads the material layer to pick each cell's slot in
  the texture atlas (material variants), so a chunk is still one mesh. Water (see-through) is a second
  mesh in the same chunk node. Objects go into a MultiMesh per object type.
- **Storing types:** Minecraft's palette trick per chunk — list the ids the chunk actually uses, pack
  indices with only as many bits as needed (1 bit if a chunk is just grass + sand, 4 bits for up to 16
  types).

So "terrain" becomes just the shape layer, and every new kind of thing is one more layer with its own
small id table.

## 7. Water and waterfalls for us (Animal Crossing style)

- **Water layer:** per column, a water surface height (0 = dry).
- **Rendering:** a second, see-through mesh in the same chunk: a flat surface wherever water is above
  the ground.
- **Waterfalls are computed, not stored:** wherever two neighbouring columns both have water and their
  water heights differ, the builder adds a waterfall strip down that edge (plus a splash effect at the
  bottom). Raise a cliff under a river and the waterfall appears by itself.

---

## 8. Save format version 2 (plan)

- Header + **mode byte** (height / corner).
- Per chunk, a **list of layers**: layer id, encoding, byte length, data.
  - Height grids: heights as bytes; corner grids: today's RLE bits.
  - Material / overlay: palette + bit-packed indices.
  - Objects: sparse list.
- **zstd per chunk.** A loader **skips unknown layer ids**, so adding the water layer later doesn't
  break old saves.
- **Version 1 files keep loading.**

## 9. Saving to a server later

Yes — per-chunk compressed blobs are the right shape:

- Store as `(world, chunk_x, chunk_z) → compressed bytes + version`.
- The game uploads **only changed chunks**; the server never needs to unpack them.
- If it ever does (validation), Go has a good zstd library (`github.com/klauspost/compress/zstd`).

## 10. How big a grid fits in 1 MB per user

Rough, extrapolated from the generated terrain above (real islands with lots of detail compress less):

| | Height mode (height + material + water) | Corner mode (32 high, + material) |
|---|---|---|
| Raw, no compression | ~580 × 580 columns | ~480 × 480 |
| With zstd | **~1300–2000 × 1300–2000** | **~900–1500 × 900–1500** |

- An Animal Crossing-sized island (~112 × 96) takes only about **10–40 KB**: 1 MB is plenty for many
  islands per user or one very large world.
- Objects (sparse list) add a few bytes each: 10 000 objects ≈ 40–80 KB.

---

## 11. Recommendation / next steps

1. **Height mode as the main mode** for the open world, with layers: height, surface material,
   water, objects. Corner mode only for special areas (caves, arches).
2. **Save v2:** layer list per chunk, zstd per chunk, v1 still loads.
3. First two layers to build: **height + surface material**; then water + derived waterfalls.
4. Fold `MCHeightmap` (separate class, one node per cell today) into this.
