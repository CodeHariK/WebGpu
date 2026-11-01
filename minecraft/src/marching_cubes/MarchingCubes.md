# Marching Cubes – code map

Short map of `src/marching_cubes`. The full design notes are in [docs/MarchingCubes.md](../../docs/MarchingCubes.md);
save formats are in [docs/TerrainStorage.md](../../docs/TerrainStorage.md).

## Nodes in `cubes.tscn`

| Node | Class | Job |
|---|---|---|
| `MCTileLibrary` | MCTileLibrary | Loads the tile-set `.glb` (BrickBlock91: L / M / H) and fills all 256 hashes per level |
| `MCGrid` | MCGrid | Corner bits in chunks, one merged mesh + collider per chunk; each cell drawn from its level (L / M / H); Randomize (`solid_floor` off) |
| `MCManager` | MCManager | Wires library → grid, UI, mouse editing, save / load |

## Call order

```
MCTileLibrary::_ready
└─ initialize_library                    (only the first call works)
   ├─ load_tiles                         .glb → tiles[hash] + variant meshes
   ├─ fill_by_y_turns                    empty hashes ← modelled tile turned 90/180/270
   └─ report_missing
└─ build_showcase                        rows of every tile (debug view)

MCGrid::_ready                           nothing; waits for MCManager

MCManager::_ready
└─ call_deferred(initialize_all)
   ├─ tile_library->initialize_library   (already done → returns)
   ├─ grid->set_tile_library             drops the surface cache
   ├─ grid->initialize_grid              allocate chunks
   │  ├─ _initialize_boundaries          floor solid, sides/top empty
   │  └─ refresh_grid
   │     ├─ _free_spawned_nodes
   │     └─ per chunk:
   │        ├─ _build_chunk_mesh         MCChunkBuilder add… → commit + trimesh collider
   │        └─ _spawn_debug_corners
   ├─ setup_ui, update_ui
   ├─ _initialize_previews
   └─ GameManager::register_mc_manager

MCManager::_process (every frame)
├─ update_ui                             every 0.1 s
└─ _update_hover_raycast                 → _update_hover_box / grid->update_hover_preview

MCManager::_unhandled_input (click)
└─ _raycast_click
   ├─ terrain mode: _terrain_click
   │  └─ grid->set_corner
   │     ├─ _mark_cell_dirty ×8          the 8 cells sharing the corner
   │     ├─ _queue_rebuild               call_deferred, once per frame
   │     │  └─ flush_dirty_chunks        end of frame → _build_chunk_mesh per dirty chunk
   │     └─ _update_debug_corner
   ├─ place mode:  _place_click → grid->add_placed_object
   └─ drag mode:   _select_or_start_drag → _start_drag … _finish_drag / _delete_object
```

## Folders

| Folder | What lives there |
|---|---|
| `core/` | Plain data + math, no nodes: corners, hashes, turns, chunk bits, variants |
| `library/` | `MCTileLibrary`: tiles from Blender |
| `grid/` | `MCGrid`: the editable world, chunk meshes, placed objects, save / load |
| `editor/` | `MCManager`: setup, UI, input, hover, object mode |
| `util/` | Collider helpers |

## core/

| File | Functions |
|---|---|
| `mc_corners.h` | `Corner` C0..C7, `CORNER_COUNT`, `HASH_COUNT` + corner diagram |
| `mc_hash.h/.cpp` | `binary_to_hash` "00001111" → hash · `hash_to_binary` · `corner_count` |
| `mc_symmetry.h/.cpp` | `rotate_y` hash after +90° · `turn_y` turn hash + transform N quarter turns |
| `mc_variant.h` | `cell_seed` stable per-cell hash · `variant_index` pick variant · `TileTag` / `tile_tag` parse `_ma2` (height, group, number; old `_vN`) · `variant_number` |
| `mc_level_plan.h/.cpp` | `CellBlock` · `assign_levels` L / M / H of existing corners (bottom layer L, H on roof regions only beside H) · `LevelPin` (one fixed cell) · `choose_roof_m` some roof regions M · `plan_levels` random corners that follow the rules (L bottom solved, roofs spread, nothing above an H, some roofs M), best of N layouts |
| `mc_grid_dims.h` | `GridDims`: `max_corner` · `chunk_index` · `contains_corner` · `corner_to_local` · `cell_to_local` · `owns_corner` |
| `mc_chunk.h/.cpp` | `Chunk`: `get/set_corner_bit` · `get/set_corner` · `get_cell_hash` · `has_active/inactive_neighbor` · `serialize_rle` · `deserialize_rle` |

## library/

| File | Functions |
|---|---|
| `mc_library.h/.cpp` | groups: `MCTileGroup` (L / M / H per letter b..z) · `get_group_tile` (group's level tile → group's M → group a) · `get_group_letters` / `get_groups` · `_level_table` · `MCTile` (mesh, variant_meshes, variant_sockets, turn, source_hash; `variant_for_cell` · `sockets_for_cell`) · `MCSocket` (`socket_<type>_p<percent>` child nodes) · decor pieces (`decor_<type>_<n>`: `get_decor` · `decor_for_socket`, rolled per cell + the grid's `decor_seed`, which Randomize sets) · `MCTileLevel` · `mesh_for_cell` · `_ready` · `initialize_library` · `load_tiles` (M a → tiles, L a → ground, H a → roof) · `get_tile` · `get_level_tile` · `get_tile_for_level` (M when the level has none) · `has_level` · `get_hashes_per_tile` · `set/get_tile_set_path` |
| `mc_library_fill.cpp` | `fill_by_y_turns` (every level) · `report_missing` |
| `mc_library_showcase.cpp` | `build_showcase` (every mesh at its Blender position, labelled, click box on LAYER_TILE_LIBRARY) · `get_showcase_bounds` |

## grid/

| File | Functions |
|---|---|
| `mc_grid.h/.cpp` | `_ready` · `set_grid_dims` · `set_chunk_dims` · `set/get_tile_library` · `initialize_grid` · `refresh_grid` · `_initialize_boundaries` · `_is_boundary_corner` · `set_corner` · `get_cell_hash` · `is_corner_active` |
| `mc_grid_cells.cpp` | `_build_chunk_mesh` (tile of the cell's level, M if that level has none; its sockets filled with decor, not collided) · `_free_chunk_mesh` · `_mark_cell_dirty` · `_queue_rebuild` · `flush_dirty_chunks` · `_free_spawned_nodes` |
| `mc_grid_inspect.cpp` | inspect mode: `set_inspect_mode` · `inspect_cell` (orange wire cube on it) · `clear_inspect` · `make_cell_preview` (tile + wireframe, cube, 8 numbered corner dots, at the origin) · `get_cell_info` (cell, hash, modelled tile + turn, group, level, variant) |
| `mc_grid_debug_view.cpp` | debug views: `_style_debug_corner` (corner nodes draw nothing: picking only) · `set_debug_edge_corners` / `_rebuild_edge_corners` (inactive corners with an edge to an active one: MultiMesh dots + edge lines; rebuilt in flush_dirty_chunks / refresh_grid) · `set_debug_wireframe` / `_apply_wireframe` (wireframe overlay on chunk meshes) · `set_debug_water` / `_rebuild_water_debug` (held / stream / sea squares, flow arrows, framed / open fall edges, last pour dots) · `replay_water_debug` (tweens the pour order in) · `set_pour_step` / `get_pour_step_text` / `_add_pour_step_debug` (the last pour step by step; keys . , / in `mc_manager_tools.cpp` `_pour_step_shortcut`) |
| `mc_grid_water_mesh.cpp` | the water mesh: `_emit_water_cells` (per dual cell, per water level: surface outside the bank layer and inside the bed layer, clipped on the tiles' sub-grid; fall along the bed layer's contour) · `set_water_blocky_groups` · shape port of cliff_tiles.py (`blend`, `gradient`, `clip`) |
| `mc_grid_water.cpp` | water layer: `get/set_water_level` · `paint_water` (waterscaping: dig one tier, water at the old height) · `fill_water` (pour: flood onto ground not higher, one layer deep) · `settle_water` (re-pour all water; load + terrain clicks) · `_flow_region` / `_sea_region` / `_set_water` / `_flood_sea` · `_water_surfaces` (held one layer deep vs thin stream) · `_fall_framed` (fall cut into the land?) · `_water_flow` (flow field toward the falls, landing foam) · `clear_water` · `set/get_sea_level` · `water_material` / `waterfall_material` · `_is_wet` · `_queue_water` · `_rebuild_water` (quarters per wet column tucked under banks so edges follow the cliffs, flow in vertex COLOR, fall sheets run into banks at their ends + waterfall sheet per level step and per spill onto lower dry ground, one mesh, rebuilt in flush_dirty_chunks / refresh_grid) |
| `mc_grid_groups.cpp` | `_update_groups` (regions of solid corners by flood fill, one group rolled per region from its first corner + decor_seed; called from `_update_levels`) · `_group_at` · `get_cell_group` |
| `mc_grid_heights.cpp` | heightmap mode: `set/get_heightmap_mode` · `get/set_column_height` · `randomize_heights` (island from FastNoiseLite: tiers to the middle, rim at the floor, M roofs rolled) · `set/get_lock_rim` · `load_heights` (MCHeightmap's data, its M-roof seed) · `_roll_roof_m` · `_heights_from_corners` (flattens overhangs) · `_write_column` · `_set_corner_as_height` (set_corner in the mode: fill → column up to just above, empty → down to just below) · `_column_counts` · `_column_index` · `_is_edge_column` |
| `mc_grid_levels.cpp` | `_cell_block` · `_level_at` · `get_cell_level` · `_update_levels` (after every edit / refresh) · `set_solid_floor` · `set_pin` / `clear_pin` / `get_pin_cell` · `_live_pin` · `randomize_levels` · `get_last_plan` |
| `mc_chunk_builder.h/.cpp` | `MCSurfaceCache::get / clear` · `MCChunkBuilder::add` (p_collide false: drawn only, socket decor) · `_surface_for` · `_append` · `is_empty` · `commit` · `get_faces` |
| `mc_grid_debug_corners.cpp` | (corner nodes = picking colliders, no look) `set/get_show_debug_corners` · `set/is_debug_corners_visible` · `set_corner_collision_enabled` · `_spawn_debug_corners` · `_update_debug_corner` · `_has_neighbor_with_state` |
| `mc_grid_hover.cpp` | `_initialize_hover_previews` · `update_hover_preview` · `hide_hover_preview` |
| `mc_grid_objects.cpp` | `is_area_blocked_by_grid` · `is_area_blocked_by_objects` · `add_placed_object` · `remove_placed_object` · `detach_placed_object` · `get_placed_object` · `_clear_placed_objects` |
| `mc_placed_objects.h/.cpp` | `MCPlacedObjects`: `add_object` · `remove_object_at` · `remove_object_by_node` · `is_area_blocked` · `get_object_by_node` · `clear` |
| `mc_grid_undo.cpp` | undo / redo: `begin_edit` · `end_edit` · `undo` · `redo` · `can_undo` / `can_redo` · `clear_history` · `_record_height` / `_record_water` / `_record_corner` (old value once per edit) · `_apply_edit` (writes them back, records the inverse, refresh_grid) |
| `mc_grid_save_v2.cpp` | save format version 2: `_save_v2` (header with mode, lock_rim, decor_seed, sizes; block-size table; blocks of layers, zstd when smaller) · `_load_v2` (validate into temporaries, skip unknown layers, apply) · `_block_columns` (corner columns a chunk column owns) |
| `mc_grid_io.cpp` | `save_grid` · `load_grid` (validates first) · `cells_to_json` (Copy for Blender) |

## editor/ (all `MCManager`)

| File | Functions |
|---|---|
| `mc_manager.h/.cpp` | `_ready` · `_process` · `initialize_all` · `set/get_tile_library_path` · `set/get_grid_path` · `save_terrain` · `load_terrain` · `_on_load_terrain` · `_on_save_terrain` · `_on_copy_for_blender` |
| `mc_manager_input.cpp` | `_unhandled_input` · `_raycast_click` · `_terrain_click` · `_drag_group_exclusions` |
| `mc_object_mode.cpp` | `_place_click` · `_select_or_start_drag` · `_start_drag` · `_finish_drag` · `cancel_drag` · `_delete_object` · `_clear_selection` |
| `mc_manager_hover.cpp` | `_initialize_previews` · `_update_hover_raycast` · `_update_hover_box` · `_get_cell_hash` · `_on_toggle_visual_corners` |
| `mc_ui.cpp` | `setup_ui` · `update_ui` · `_on_toggle_placement_mode` · `_on_toggle_ui` · `_on_gui_input` · `_on_show_help` · `_save_screenshot_and_quit` |
| `mc_manager_inspect.cpp` | inspect panel: `_setup_inspect_panel` (bottom-right SubViewport, own world, light, orbit camera) · `_show_inspect_cell` · `_on_inspect_view_input` (drag orbit, scroll zoom) · `_update_inspect_view` (slow spin) · `_inspect_input` (left inspect, right clear) · `inspect_at_cell` (from code) |
| `mc_manager_tools.cpp` | tool bar: `_setup_tool_bar` (Terrain / Water / Place / Move + Undo / Redo) · `_on_select_tool` · `_mark_active_tool` · `_on_undo` / `_on_redo` · `_update_history_buttons` · `_history_shortcut` (Cmd/Ctrl+Z …) · `_water_click` |
| `mc_manager_randomize.cpp` | `_setup_randomize_ui` · `_on_randomize_grid` (Randomize Grid button, next seed) · `_on_unpin` · `_try_pin_click` (click a library tile: pin it, move the grid behind the library, randomize) |

## util/

| File | Functions |
|---|---|
| `mc_physics.h/.cpp` | `MCPhysics::create_static_box_collider` · `create_static_cylinder_collider` · `create_static_sphere_collider` |
