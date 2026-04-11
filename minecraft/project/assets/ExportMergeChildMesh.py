import bpy

def is_binary_name(name):
    # Matches the validation logic in your C++ code
    if len(name) < 8:
        return False
    bin_part = name[:8]
    return all(c in '01' for c in bin_part)

# Ensure we are in Object Mode
if bpy.ops.object.mode_set.poll():
    bpy.ops.object.mode_set(mode='OBJECT')

# Deselect everything first
bpy.ops.object.select_all(action='DESELECT')

# Find all "base" objects (the ones with the binary hash names)
base_objs = [obj for obj in bpy.data.objects if is_binary_name(obj.name)]

merged_count = 0

for base in base_objs:
    # Select the base object and make it active
    base.select_set(True)
    bpy.context.view_layer.objects.active = base
    
    # Select all children that are meshes
    has_children = False
    for child in base.children_recursive: # recursive to catch nested parts
        if child.type == 'MESH':
            child.select_set(True)
            has_children = True
            
    # Join only if children were found
    if has_children:
        bpy.ops.object.join()
        print(f"Merged children into: {base.name}")
        merged_count += 1
    
    # Deselect for the next iteration
    bpy.ops.object.select_all(action='DESELECT')

print(f"Done! Merged children for {merged_count} base meshes.")
