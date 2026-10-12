# Marching cubes TODO

Code: `src/marching_cubes/` + Blender add-on `../blender/mc_tiles/mc_tile_tools`.

Tile sets are modelled in Blender (`project/assets/Marching/Marching.blend`), one collection per set,
tiles named by their 8 corner bits. Godot only turns tiles about Y; mirrors / upside-down tiles are
modelled in Blender. Docs: `docs/MarchingCubes.md`.

## Done

- [x] Bug fixes, file split, click handling split (129fb80, 19a525e, f8a6160)
- [x] Dropped the 21-tile (all rotations) mode and `import_mode`: MCTileLibrary loads any set and fills gaps
      by Y turns, logs missing hashes (Cube.glb output identical to before) — uncommitted
- [x] Blender add-on: tile sets on collections (kind / mode 68 · 256 · heightmap 22 / export path),
      searchable tile list, Check (missing, rotated copies, overhangs) — uncommitted

## Next (in order)

- [ ] **Model `11110000`** in House / HouseMini (now empty in `cubes.tscn`; 21 mode used to flip `00001111`)
- [x] **Chunk merging**: each MCGrid chunk is one MeshInstance3D (one surface per material, built by
      `grid/mc_chunk_builder.*` from cached tile arrays) + one trimesh collider on LAYER_TERRAIN; edits
      mark chunks dirty, rebuilt once per frame. Same triangles as before (30 749 / 30 749). Bench
      (32x8x32 cells, Forward+): render objects 1091 → 12, draw calls 24 → 12 (Godot was already
      instancing identical tiles), primitives +19% (culling per chunk, not per cell), build 24 → 32 ms,
      one edit 0.03 → 0.55 ms — uncommitted
- [ ] MCHeightmap still spawns one node per cell: move it onto the same chunk builder
- [ ] **Layered height grid + water/waterfalls + save v2 (zstd per chunk)** — plan in `docs/TerrainStorage.md`
- [ ] **Separate collision meshes** (modelled in Blender):
      - Blender: child object `<tile>_col` (e.g. `00010001_col`, `00010001_v2_col`), moves/turns with the tile;
        shown as wireframe, never rendered; "Show Collision" toggle. No `_col` = collide with the visible
        mesh (today); empty `_col` = no collision (flowers, grass tufts)
      - Export Set: `_col` not joined into the visible mesh, exported as a child node
        (`00010001_circus` → `00010001_circus_col`), rot/scale/modifiers applied, flipped by mirror on
        export; a variant without its own `_col` uses the base tile's
      - Godot: MCTileLibrary reads the `_col` child's triangles per variant; MCChunkBuilder fills the chunk
        collider from them (visible mesh as fallback); still one collider per chunk
      - Check: tiles without a collision mesh (info), collision geometry outside the 1×1×1 cell
      - Verify first: does Godot's importer treat `_col` like its `-col` / `-colonly` hints? If so use a
        neutral ending (`_collision`) in the .glb
- [x] Blender: helper debug material (plain light blue on object-level slots; tiles keep real materials)
- [x] Blender: **Playground** v1 — selected tile in the middle of 3×3×3, neighbours from random corners
      (or no-overhang heights), tiles turned about Z like Godot, copies share mesh data (live edits),
      red box for missing hashes, Build / Shuffle / Clear — uncommitted
- [x] Godot → Blender: Stats panel "Copy for Blender" (`MCGrid::cells_to_json`, cells + hashes on the
      clipboard) → add-on "Paste from Godot" rebuilds the grid in the Playground, every tile selectable,
      real materials, shared mesh data (Edit Mode edits the real tile) — uncommitted
- [ ] Playground next: cycle neighbours per side, presets (ground, cliff edge, inner corner), seam check
- [x] **Mesh variants**: `_v2`, `_v3` suffix; add-on lists / checks / exports them (Check flags the same
      hash + variant twice), playground and Paste show the same variant per cell as Godot; MCTileLibrary keeps
      every variant per hash, turns them, each cell picks one from its grid position
      (`core/mc_variant.h`, Python copy verified identical) — uncommitted
- [ ] Variant weights (e.g. flowers 20%) from a custom property / glTF extras
- [x] Blender: tile names are just `hash` / `hash_vN` (Circus renamed in Marching.blend + mc_tiles.blend);
      export adds the set name (`00010001_circus_v2`); Check lists untidy names + "Rename to Hash Names";
      tile list has a variant column (1/2) — uncommitted
- [x] Blender: **mirror on export** — Export Set writes a left/right flipped copy of any tile whose
      mirror shape isn't modelled (normals fixed, variants too); Check shows "N made by mirror on export"
      and flags hand-modelled mirror partners; playground / paste show them as negative-X-scale copies.
      Circus now 53 modelled (15 partners deleted in Marching.blend and mc_tiles.blend); exported
      library = Cube.glb on 242/254 hashes, the 12 others are 3 pairs that weren't exact mirrors — uncommitted
- [ ] **Material variants baked to texture**: bake each variant (grass / ice / sand…) with the same UV
      layout into one atlas; chunk merge offsets the cell's UVs to its slot → still 1 material,
      1 draw call per chunk even with mixed variants. Needs UVs inside 0–1 and padding / no mipmaps.
      Texture2DArray + small shader only if many variants or blended borders are wanted
- [x] Blender: per-set **export** to its own .glb (Export path + button): only the set's tiles, copies
      with own mesh data, rot/scale + modifiers applied, children joined; Circus export identical to
      Cube.glb (68/68) from the committed blend — uncommitted
- [x] "Move helper objects into Helpers" button (set's non-tile objects → `Helpers` child collection);
      run it on Circus in Marching.blend (48 helpers) — uncommitted

## Later

- [ ] Prism kind in the add-on (read `src/terrain/marching_prism` for corner order + 120° turns)
- [ ] Prism review fixes: `modify_corner` negative index, UI hash z double offset (`mp_manager.cpp`),
      duplicate border debug spheres, `_input` → `_unhandled_input`, `load_grid` validation, only 4 of 6
      lattice neighbours checked, wireframe ids miss `loc_y`, per-cell nodes + BoxShape3D, cell-corner
      code duplicated 4×
- [ ] Skip the runtime library showcase (`display_library`) outside debug
- [ ] MCHeightmap: don't let its noise overwrite heights loaded from a save
- [ ] Heightmap set: copy the 22 no-overhang tiles out of Circus into their own set

## Streaming (chunk border corners)

Each chunk stores (size + 1)^3 corners, so a corner on a shared face / edge / vertex is copied in 2 / 4 / 8
chunks; `set_corner` writes every loaded copy and `_build_chunk_mesh` reads only its own chunk, so a chunk
builds and loads on its own. Keep the duplicates (+20% bits at 16^3, mostly gone after RLE / zstd).
- [ ] Edits next to an unloaded chunk: `set_corner` only updates chunks in memory, so the unloaded
      neighbour's saved border goes stale. Write the change into its saved block, or keep a pending
      border-edit list applied when that chunk loads
- [ ] Per-chunk save blocks: `save_grid` writes one `.mct` for the whole grid; streaming needs seekable
      blocks (offset table in the header, or one file per region) — fits save v2 in `docs/TerrainStorage.md`
- [ ] Debug-build border check after load: compare each chunk's shared faces with its neighbour's and
      report mismatches (catches stale copies from the first item)

Docs: `docs/MarchingCubes.md` (design), `MarchingCubes.md` in this folder (code map + call order),
`docs/TerrainStorage.md` (save formats).
