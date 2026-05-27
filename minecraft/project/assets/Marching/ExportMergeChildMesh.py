import bpy
import os

# --- SETTINGS ---
export_filename = "MarchingCubesMerged.glb"
DEBUG_MODE = True # Keeps the temp collection for inspection
# ----------------

def is_binary_name(name):
    # This matches the C++ logic: first 8 chars must be '0' or '1'
    clean_name = name.split('.')[0]
    if len(clean_name) < 8: return False
    bin_part = clean_name[:8]
    return all(c in '01' for c in bin_part)

def export_merged_visible():
    print("\n--- Starting Merged Export ---")
    
    if not bpy.data.is_saved:
        print("ERROR: Please save your .blend file first so I can find the export path!")
        return

    if bpy.ops.object.mode_set.poll():
        bpy.ops.object.mode_set(mode='OBJECT')

    # 1. Create a temporary collection
    col_name = "TEMP_GLB_EXPORT"
    if col_name in bpy.data.collections:
        # Clean up old debug collection if it exists
        temp_col = bpy.data.collections[col_name]
        for obj in temp_col.objects:
            bpy.data.objects.remove(obj, do_unlink=True)
    else:
        temp_col = bpy.data.collections.new(col_name)
        bpy.context.scene.collection.children.link(temp_col)
    
    # 2. Duplicate visible objects
    duplicates_map = {}
    print(f"Scanning {len(bpy.context.visible_objects)} visible objects...")
    
    for obj in bpy.context.visible_objects:
        dupe = obj.copy()
        if obj.data:
            dupe.data = obj.data.copy()
        temp_col.objects.link(dupe)
        duplicates_map[obj] = dupe

    # Re-establish hierarchy in the duplicates
    for orig, dupe in duplicates_map.items():
        if orig.parent in duplicates_map:
            dupe.parent = duplicates_map[orig.parent]
            dupe.matrix_parent_inverse = orig.matrix_parent_inverse.copy()

    # 3. Apply Transforms and Modifiers on duplicates
    all_dupes = list(duplicates_map.values())
    if all_dupes:
        bpy.ops.object.select_all(action='DESELECT')
        for d in all_dupes:
            d.select_set(True)
        
        mesh_dupes = [d for d in all_dupes if d.type == 'MESH']
        if mesh_dupes:
            bpy.context.view_layer.objects.active = mesh_dupes[0]
        else:
            bpy.context.view_layer.objects.active = all_dupes[0]

        # A. APPLY TRANSFORMS (Rotation and Scale only)
        # We skip 'location=True' to preserve the relative origins/offsets
        print(f"Applying Rotation & Scale to {len(all_dupes)} objects...")
        bpy.ops.object.transform_apply(location=False, rotation=True, scale=True)

        # B. APPLY MODIFIERS
        if mesh_dupes:
            bpy.ops.object.select_all(action='DESELECT')
            for d in mesh_dupes: d.select_set(True)
            bpy.context.view_layer.objects.active = mesh_dupes[0]
            bpy.ops.object.convert(target='MESH')
            print(f"Applied modifiers to {len(mesh_dupes)} meshes.")

    # 4. Merge Children into Binary-Named Parents
    base_dupes = []
    found_count = 0
    for orig, dupe in duplicates_map.items():
        if is_binary_name(orig.name):
            found_count += 1
            bpy.ops.object.select_all(action='DESELECT')
            dupe.select_set(True)
            bpy.context.view_layer.objects.active = dupe
            
            has_children = False
            for child in dupe.children_recursive:
                if child.type == 'MESH':
                    child.select_set(True)
                    has_children = True
            
            if has_children:
                if dupe.type == 'MESH':
                    print(f"Merging children into parent: {orig.name}")
                    bpy.ops.object.join()
                else:
                    print(f"WARNING: Base {orig.name} is not a mesh. Children not merged.")
            
            dupe.name = orig.name.split('.')[0]
            base_dupes.append(dupe)

    print(f"Found {found_count} base config objects.")

    # 5. Export
    bpy.ops.object.select_all(action='DESELECT')
    valid_exports = [d for d in base_dupes if d.name in bpy.context.view_layer.objects]
    for dupe in valid_exports:
        dupe.select_set(True)

    if valid_exports:
        target_path = bpy.path.abspath("//" + export_filename)
        print(f"Exporting to: {target_path}")
        try:
            bpy.ops.export_scene.gltf(
                filepath=target_path,
                use_selection=True,
                export_format='GLB'
            )
            print("EXPORT SUCCESS!")
        except Exception as e:
            print(f"EXPORT FAILED: {str(e)}")
    else:
        print("CANCELLING EXPORT: No valid binary-named parents found.")

    # 6. Cleanup
    if not DEBUG_MODE:
        print("Cleaning up temporary objects...")
        for obj in all_dupes:
            if obj.name in bpy.data.objects:
                bpy.data.objects.remove(obj, do_unlink=True)
    else:
        print("DEBUG_MODE is ON: Collection 'TEMP_GLB_EXPORT' retained.")

export_merged_visible()
