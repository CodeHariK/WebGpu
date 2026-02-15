# world_blender — file overview

This folder contains a small Blender add‑on for placing and managing voxelized objects, computing face hashes, and maintaining a reusable object library. Below is a concise description of what each source file is responsible for.

## Files and purpose 📦

- `__init__.py` — Add‑on entry point: `bl_info`, and `register()`/`unregister()` that register the core modules (`type`, `save`, `library`, `voxel`).

- `type.py` — Core data model and shared helpers:
  - `world_blenderData` WindowManager PropertyGroup (stores voxels, library, hashes, UI state).
  - voxel naming, collection helpers, and registration of PropertyGroup types.

- `voxel.py` — Main voxel UI and interaction:
  - `VOXEL_PT_panel` (3D View sidebar) + UIList for voxels.
  - Hover placement tool (`VOXEL_OT_hover`), deletion, selection toggle and convenience operators.

- `gizmo.py` — 3D gizmos to move/adjust voxel offsets and a toggle operator to enable/disable the gizmo.

- `library.py` — Object library UI and helpers:
  - UI list for library items, operator to add selected objects to the library, and panel controls (hash tools, list view).

- `save.py` — I/O and export operators:
  - Save / Load library JSON, export selected library items to a `.glb` (GLTF) file, and clear library.

- `voxel_hash.py` — Face-hashing implementation and utilities:
  - Multiple hashing strategies (raycast, vertex/plane optimised) for voxel face recognition.
  - `VOXEL_OT_face_hash` operator to compute hashes for library objects.

- `wfc_vertex_cube_spawn.py` — Helper operator to spawn voxelized cube-blocks from an object mask (used for visual tests / debugging).

- `preview.py` — Preview generation & helpers used by the voxel panel (live wireframe previews of voxel grids).

- `voxel_hash.py` — (see above) hashing algorithms and helpers used by the library and export pipeline.

- `example/` — Example assets and saved library (`Voxel.blend`, `library.json`, sample `.glb`) for testing and demos.

- `blender_manifest.toml` — Metadata used for Blender extension packaging (id, version, permissions, supported Blender versions).

- `pyproject.toml` — Python packaging config and dependency list (used for packaging / dev environment).

## Quick usage 👇

1. Install/enable the add‑on in Blender.  
2. Open the 3D View → press `N` to open the Sidebar → select the `world_blender` tab.  
3. Use the **Object Library** panel to add objects to the library, pick a library item, then use the **Voxel** panel to hover/place voxels, toggle gizmos, and export.

## Notes & next steps ✨

- The `type` module stores runtime state on `WindowManager` — register the add‑on before calling functions that depend on that state.  
- If you want, I can add short examples for each operator in this README or generate a developer guide (register flow, unit-test suggestions, or cycle detection for auto-registration).
