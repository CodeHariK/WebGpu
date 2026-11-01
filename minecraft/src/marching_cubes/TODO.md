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
- [ ] ~~MCHeightmap onto the chunk builder~~ → replaced by **heightmap mode in MCGrid** (see the
      Animal Crossing / Link's Awakening section)
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
- [x] Blender: **Filter** (Tiles panel) — search, shapes (no overhang / overhang), corners, variants,
      problems, family of selected; drives the tile list and (Filter Viewport) hides the rest in the
      3D view, never unhides what you hid yourself 
- [x] Blender: **Arrange** — layouts by corner count / hash / family / heightmap, variants behind
      their base, flipped copies in mirror-on-export slots, red boxes for missing, Restore puts the
      hand layout back (obj["mc_home"]), "Only Filtered" + Hash Labels overlay — uncommitted
- [x] Blender: playground neighbours selectable / editable (shared mesh), Build on a copy re-centres
      on its tile
- [x] Blender: filter "Used by Mode" (default; Heightmap hides overhangs), Arrange "Filtered First"
- [x] Blender: Corner Boxes overlay (cell box, solid corners orange / empty light blue); modes renamed
      "X=Z, Y Asymmetric (68)" / "X=Z, Y Asymmetric Heightmap (22)"
- [x] Blender: **Seam check** (Check panel) — each tile caches 6 wall bitmaps (64x64 samples of where
      its flat wall faces = its solid cross-section; side walls seen from outside so Y turns keep them,
      u flipped for mirrors); A right == flipped B left, A top == B bottom for every pair that can touch
      (same 4 shared corners, turned + mirrored library, variants); differences under 0.4% ignored.
      Click a pair = the two tiles side by side in the playground, red = solid only on A, blue = only
      on B; real A / B tiles moved next to the pair while shown. Tile list: conflict count (pins the
      seam list to that tile) + playground button; playground spawns next to its tile on grid lines;
      Face Hashes overlay; searching a turned / mirrored hash finds the tile that draws it. Point lists were tried first:
      hundreds of false clashes from hidden internal faces, split wall faces and rounding
- [ ] Seam check next: normals (shading seams), UVs / materials
- [x] Blender: **Build from pieces** (set setting "Pieces") — a shape with no tile and no mirror is made
      of smaller tiles (turned / mirrored) overlapping; every solid edge (and, Edges + Faces, every full
      face) must lie inside one piece. Minimum sets: 68 -> 8 (corner, edge, slab, top corner, top edge,
      top slab, column, wall), heightmap -> 5; Edges Only: 5 / 4. Export joins the pieces into one tile
      (Godot unchanged); playground, Paste from Godot, seam check and search use the same pieces; Check
      lists "Built from pieces"; a modelled tile always wins — uncommitted. Test: the 8 pieces from
      Cube -> 68/68, export 68 tiles covering all 254 hashes
- [x] Blender: pieces from any collection (Find Pieces, dropdown per piece, linked-mesh copy in the set),
      one rule (edges + faces), placement on cell centres (x.5, y.5, z.5)
- [x] Blender: customising generated tiles — status icon in the tile list (linked / edited / detached),
      Select Pieces, Detach (join, or Detach + Clean Union: exact Boolean), Regenerate Linked (in place,
      edited / detached kept); generated tiles are never used as pieces — uncommitted

## Levels, BrickBlock set, sockets (Oct 2026)

- [x] **L / M / H levels**: tags `hash_<height><group><number>` (`00001101_ma1`); L only in the bottom
      layer, H only beside H, L -> M -> H up a column, M or H can top a column (`roof_m`); same rules in
      the Blender playground, MCGrid Randomize (`core/mc_level_plan`) and MCHeightmap (now MCGrid's
      heightmap mode)
- [x] BrickBlock set (`BrickBlock88.blend` -> `BrickBlock91.glb`, 75 modelled + 16 mirrors) loaded by `cubes.tscn`
- [x] **Build from Pieces** set toggle (on for Circus, off for BrickBlock); generated copies exported
      2 m above their source
- [x] **Pin a tile**: click a showcase tile (laid out at its Blender position) -> grid moves next to it,
      randomizes around the pinned cell
- [x] **Sockets**: empties on a tile (`socket_<type>_p<pct>`, +Z out, mirrored with mirror copies),
      decor pieces `decor_<type>_<n>` in a `Decor` collection; add-on preview (pick / shuffle);
      Godot rolls each socket per cell (chance, then piece) from cell + `MCGrid.decor_seed` (set by
      Randomize); decor drawn, not collided. Checked: 4 sockets x 400 seeds match 50% / even piece split

## Next (paused — agreed, not started)

- [ ] **Blender playground socket rolls**: each copied tile rolls its sockets (chance, then piece) like
      Godot, new roll on rebuild; today every copy shows the source tile's preview
- [ ] **Export & Reload**: add-on button exports the glb then runs the headless reimport (Godot + project
      path in add-on prefs); debug builds: MCTileLibrary watches the glb mtime and reloads it at runtime
      (`GLTFDocument`) + rebuilds the grid
- [x] **Groups a/b/c/d**: every group loads (MCTileGroup per letter, filled by Y turns); each
      connected solid region (corners joined across cube edges) rolls one group from its first
      corner + `decor_seed` (`grid/mc_grid_groups.cpp`); a group without a shape falls back to
      group a (`MCTileLibrary::get_group_tile`); `get_cell_group` / `get_groups` for scripts.
      Checked: Cliffs.glb (rounded a + blocky b), 4 islands in one grid -> groups a b a a — uncommitted
- [x] **Undo / redo** (Cmd/Ctrl+Z, Cmd/Ctrl+Shift+Z or Cmd/Ctrl+Y, and Undo / Redo buttons):
      `grid/mc_grid_undo.cpp` — between begin_edit / end_edit every changed value records its old
      value once (heights + water in heightmap mode, corners in corner mode, plus decor seed + M roofs);
      undo writes them back while recording the redo; 100 steps; cleared on initialize / load / mode
      switch. Each terrain / water click and Randomize is one step. Placed objects and pins aren't in it.
      **Tool bar** (`editor/mc_manager_tools.cpp`): Terrain / Water / Place / Move in the top bar (one
      active, "● Name"), replacing the "Mode" cycle button and the "Water brush" toggle. Checked:
      undo / redo of a column edit, a water fill, Randomize, corner edits — uncommitted
- [ ] Multi-pin (Shift-click showcase tiles) — optional
- [ ] **Mobile perf**: decor through MultiMesh with visibility range, chunk LOD, one merged material per chunk
- [ ] Socket fit check: skip a socket in a cell when the corners in front of it (its +Z, piece depth) are
      solid — big pieces (awnings, balconies) poking into neighbours
- [ ] Same decor style per building (roll per connected region, not per cell)
- [ ] Decor collision per type (e.g. crates collide, vines don't)
- [ ] Add-on Check: decor leaving the cell, sockets with no decor of their type, piece facing not +Z
- [ ] Level-aware Arrange (L / M / H blocks)
- [ ] Simple AI floor graph (only when AI needs it): `00001111` cells = flat floor at their height,
      same-height neighbours linked, no automatic links between heights (no ladders / stairs)

## Animal Crossing / Link's Awakening terrain (design agreed, not started)

Rule: if it changes how you walk between heights and comes from the corners, it's terrain (tiles);
everything built on top is an object. Terrain stays pure corners.

- [ ] **Cliff tile set** in the AC / LA look (heightmap mode, the 22 no-overhang tiles): grass lip on
      top, rock face; rounded (AC) and / or blocky (LA) as tile groups.
      **v1 generated (uncommitted):** `blender/mc_tiles/scripts/cliff_tiles.py` computes the 22
      tiles from their corner bits (bilinear corner blend per layer: straight sides, rounded outer /
      inner corners, always seamless), grass top + grass lip + rock face, writes `Cliff.blend` +
      `Cliff.glb`; `scene/marching/cliffs.tscn` (`make run_cliffs`) = MCGrid heightmap mode on it.
      **v2:** rock strata (STRATA bands per half cell, grooves pushed in by a height-only function
      that is 0 at z -0.5 / 0 / 0.5, darker in the grooves via vertex colour); wall normals and the
      groove push kept in the cell side's plane where a wall meets it (seamless shading); grass
      material plain colour (Godot only enabled vertex colour as albedo on one material).
      **Blocky style** (`--style blocky` -> `Cliff_blocky.blend/.glb`, `scene/marching/cliffs_blocky.tscn`,
      `make run_cliffs_blocky`): each solid corner owns its quarter of the cell, edges along the
      cells' middle lines, a corner rounded only where it sticks out, inner corners sharp.
      **Both styles in one set** (`--style both` -> `Cliffs.blend/.glb`: rounded = group a, blocky =
      group b "_mb1"); `cliffs.tscn` now loads Cliffs.glb, each island rolls its style.
      Next: UVs + textures, bumps along the face (would need the same 0-on-sides rule)
- [ ] **Slopes = tiles, not a system**: the same corner pattern that draws a cliff step ("high on one
      side, low on the other") modelled as a tilted surface in another group (`a` cliffs, `b` slopes),
      rolled per region or chosen; collider follows the mesh, so walkable for free.
      Catch: one level over one cell is 45°: try half-height levels (0.5 high, ~27°) first; two-cell
      slopes (neighbour-aware tiles) only if that's not enough. (LA's smooth height changes look like
      slope tiles — guessed from how it looks, not checked)
- [ ] **Stairs / ramps = objects** (any width / height / depth), standing on the lower level in front
      of the cliff, back against the face, top landing on the upper edge — not cut into the cliff, so
      wall → stair → wall needs no special tiles (the cliff runs on behind it). Built from pieces: left
      end, middle repeated across the width, right end; height from the levels it spans, depth =
      height × run per level; own collider (steps or ramp)
      - Placement check: snaps to a cliff edge; footprint on the lower level flat and free
        (`MCPlacedObjects`); top meets ground exactly its height above
      - Protection: corners under the footprint and the cliff-top corners it lands on are locked while
        it stands (block the click, or remove the stair and give it back) — no floating / buried stairs
- [ ] **Fences / walls = objects on cell edges** (no thickness), height read from the terrain under
      them, auto-connect to neighbours (straight / corner / T / end pieces)
- [ ] **Water layer + waterfalls** per `docs/TerrainStorage.md` §7: water height per column, see-through
      surface mesh per chunk, waterfalls derived where neighbouring water heights differ (+ splash).
      **v1 done (uncommitted):** `grid/mc_grid_water.cpp` — a water level per corner column (wet when
      above its ground; surface 0.15 below the next tier), drawn as a square per wet column (same dual
      grid as the cliffs, so a level step sits exactly on the cliff) + a waterfall sheet per step between
      wet neighbours (never stored); `fill_water` (bucket fill below the level, stops at banks),
      `clear_water`, `set_water_level`; `sea_level` (cliffs.tscn: 1) fills the sea on randomize_heights;
      editor "Water brush" (left: fill the clicked hole one tier deep, right: drain); shaders
      `material/shaders/terrain/water_surface.gdshader` / `waterfall.gdshader` (stepwise comments).
      **v2 done (uncommitted):** looks like water and flows. `fill_water` (Water tool left click) pours:
      a flood fill onto 4-neighbours whose ground is not higher (same tier spreads, lower falls and
      carries on), one layer deep everywhere (level = ground + 1); higher ground is a bank.
      `settle_water` re-pours all standing water (after load_grid and every terrain click, same undo
      step), so old saves and freshly dug channels flow. `clear_water` dries connected water at any
      level but not the sea. `paint_water` (dig one tier + water) kept in the API. Spill falls onto
      lower dry ground. Flow field (`_water_flow`) toward the falls in vertex COLOR, foam at fall feet.
      Shader: depth colour, shore foam, flow-map ripples, glints, glow. Checked: Hari's
      user://terrain.mct channel now runs down and out.
      **Held vs stream** (`_water_surfaces`, `_fall_framed`): water stands one layer deep only where
      banks hold it (every side a bank, held water, or a fall cut into the land onto lower water; the
      sea); elsewhere it's a stream 0.1 m over its ground that keeps running and pours over edges
      (no more slabs floating over open tier edges). Derived at mesh build, not stored. Shader:
      UV2.x = stream (no shore foam on streams).
      **Look pass:** the water surface is built in quarters that run on under a bank beside them (the
      bank's terrain hides the overlap), so the water's edge follows the rounded cliffs, not the
      column squares; fall sheets run into banks at their ends; waterfall shader rewritten (opaque
      threads scrolling down, white lip, landing foam).
      **Water mesh per cell** (`grid/mc_grid_water_mesh.cpp`): surfaces and falls built on the dual grid
      with the cliff tiles' shape function (rounded / blocky per group, `water_blocky_groups`), so
      water edges and falls follow the cliffs exactly — no grass wedges at fall feet, no straight
      falls on curved cliffs; edges run 5 cm into the rock; a fully covered cell is one quad.
      **Square tiles** (`cliff_tiles.py --style square` → `Cliff_square.glb`, `cliffs_square.tscn`,
      `make run_cliffs_square`): blocky with no curves — sharp corners, flat rock faces; the grid's
      `water_square` gives the water the same sharp shape; its falls sit 1 cm off the rock.
      **Sampled water mesh** (default, `water_sampled`, `grid/mc_grid_water_sampled.cpp`): a fine sample
      grid (`water_sample_rate` per cell, 8) asks each cell's bodies for the height; marching squares
      per height with bisected, cached edge crossings (neighbours share points exactly); falls built
      from the same contour points where lower water / lower dry ground is just outside; whole cells
      are one quad. Shape code shared in `grid/mc_water_shape.h`. Debug: wet samples as dots.
      Still: 45° chamfers of up to one sample at sharp corners (square tiles).
      Tried and dropped: skirts (curved / vertical lips / overflow sheets at open water edges).
      Open: a dry corner of the bed tier reached only diagonally (terrain.mct (11, 22) by (12, 21))
      is a real gap in the terrain — the water either leaks out there or ends at it (now: ends).
      **Debug tools** (Stats panel → Diagnostics; `grid/mc_grid_debug_view.cpp`): Inspect (click a cell: an orange wire cube marks it and a side panel — a
      SubViewport with its own world — shows its tile + wireframe, cube and 8 corner dots numbered by
      hash bit, orange active / black inactive, slowly turning, drag to orbit, scroll to zoom; text:
      cell, hash, modelled tile + turn, group, level, variant; the terrain never changes;
      `grid/mc_grid_inspect.cpp`, `editor/mc_manager_inspect.cpp`), Edge corners (every inactive corner with an edge to an
      active corner: a light blue dot + a thin line along each such edge; follows edits), Wireframe (overlay on chunk meshes), Water debug (held /
      stream / sea squares, flow arrows, framed vs open falls, last pour order), Replay pour (the
      flood fill's order animated). Step through the last pour with . / , (Shift: 10
      steps), / for the whole pour: done columns, the queue, the column taken, each neighbour's
      result (queued / already queued / higher ground) and a label explaining the step
      (`MCPourStep` trace from `_flow_region`). Hover: wire cube + outlined face (`util/mc_wire.*`); the
      placement box is a wire box too.
      Still: one water mesh per grid (per chunk for big worlds), no splash particles, water in corner
      mode, a drag-to-paint stroke (each click is one column)
- [ ] **Ground material per cell** (grass / sand / path) baked into the atlas (see Material variants)
- [ ] **Heightmap mode in MCGrid** (decided: fold MCHeightmap into MCGrid, not grow it into a second
      terrain). **Step 1 done (uncommitted):** `heightmap_mode` property + "Heightmap mode" button;
      a height per corner column kept in step with the corner bits (`grid/mc_grid_heights.cpp`);
      set_corner / clicks move whole columns; switching on / loading flattens overhangs; Randomize
      in the mode = `randomize_heights` (noise island). Checked: 0 corner/height mismatches, edits
      clamp, rim locked. **Port verified:** `load_heights` (MCHeightmap's data + its M-roof roll) with
      `lock_rim` off rebuilds cubes.tscn's MCHeightmap cell for cell — 686 cells, 189 tiles: 0 hash,
      0 drawn, 0 mesh-variant / turn / level differences (MCGrid adds socket decor, MCHeightmap
      doesn't). MCHeightmap removed (heightmap/ folder, the node in cubes.tscn).
      Still to do: save v2 height layer (today saves the corner bits), a cliff tile set (BrickBlock reads as buildings). One terrain system: chunk meshes + colliders, editing, hover, save, placed objects,
      stairs / fences, water and streaming written once.
      - Storage: height byte per column (1 B / column vs ~4 B of corner bits at 32 high);
        `is_corner_active` = `y < height[x][z]`; corner mode stays for special areas (caves, arches)
      - Editing rule: a click raises / lowers a whole column (no overhangs possible)
      - Save v2 height layer (`docs/TerrainStorage.md`) — **done (uncommitted):** `grid/mc_grid_save_v2.cpp`
      - Island generator: `randomize_heights` (noise island) is its start; MCHeightmap's optional
        Noise resource (Inspector) isn't ported — add a `noise_source` property if wanted
- [ ] **Island generator**: coastline + beach, 2–3 cliff tiers, a river down to the sea, a pond; still
      editable one corner per click

## Later

- [ ] Prism kind in the add-on (read `src/terrain/marching_prism` for corner order + 120° turns)
- [ ] Prism review fixes: `modify_corner` negative index, UI hash z double offset (`mp_manager.cpp`),
      duplicate border debug spheres, `_input` → `_unhandled_input`, `load_grid` validation, only 4 of 6
      lattice neighbours checked, wireframe ids miss `loc_y`, per-cell nodes + BoxShape3D, cell-corner
      code duplicated 4×
- [ ] Skip the runtime library showcase (`display_library`) outside debug
- [ ] Island generator: don't overwrite heights loaded from a save
- [ ] Heightmap set: copy the 22 no-overhang tiles out of Circus into their own set

## Streaming (chunk border corners)

Each chunk stores (size + 1)^3 corners, so a corner on a shared face / edge / vertex is copied in 2 / 4 / 8
chunks; `set_corner` writes every loaded copy and `_build_chunk_mesh` reads only its own chunk, so a chunk
builds and loads on its own. Keep the duplicates (+20% bits at 16^3, mostly gone after RLE / zstd).
- [ ] Edits next to an unloaded chunk: `set_corner` only updates chunks in memory, so the unloaded
      neighbour's saved border goes stale. Write the change into its saved block, or keep a pending
      border-edit list applied when that chunk loads
- [x] **Per-chunk save blocks**: save v2 (`grid/mc_grid_save_v2.cpp`) writes a block per chunk (corner
      mode) or chunk column (heightmap mode) with a size table up front, so a reader can seek to one
      block; layers (corners / heights / water) zstd'd when smaller; unknown layer ids skipped; v1
      still loads. Round trips checked (heightmap: heights, water, corners, groups; corner mode;
      an old v1 user://terrain.mct). Size: 33x5x33 island 1416 B (16 blocks of 81 columns: per-block
      headers + tiny blocks compress poorly; corner bits of so short a grid are only 5 bits a column) —
      the win is on tall grids (32 high: 2 bytes vs 33 bits a column) — uncommitted
- [ ] Debug-build border check after load: compare each chunk's shared faces with its neighbour's and
      report mismatches (catches stale copies from the first item)

Docs: `docs/MarchingCubes.md` (design), `MarchingCubes.md` in this folder (code map + call order),
`docs/TerrainStorage.md` (save formats).
