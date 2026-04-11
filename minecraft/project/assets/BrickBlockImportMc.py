import bpy

unique_names = ['11110110', '11111010', '11111110', '11010111', '11011010',
                '11101010', '11110010', '11100001', '10100101', '10101010',
                '10100010', '10100001', '10100000', '11100000', '10000010',
                '10000000', '11101000', '11100100', '11111111', '11110000',
                '10010000']

# ✅ Use directly
unique_binary = set(unique_names)

def to_8bit_binary(n):
    return format(n, '08b')

def permute_bits(x):
    mapping = [4, 7, 6, 5, 0, 3, 2, 1]
    result = 0
    for out_pos, in_pos in enumerate(mapping):
        bit = (x >> in_pos) & 1
        result |= bit << out_pos
    return result

def get_number(name):
    try:
        return int(name.split('.')[0])
    except:
        return 0

def goop():
    collection_name = "Filtered_Duplicates"

    if collection_name in bpy.data.collections:
        new_col = bpy.data.collections[collection_name]
    else:
        new_col = bpy.data.collections.new(collection_name)
        bpy.context.scene.collection.children.link(new_col)

    selected_objs = bpy.context.selected_objects
    selected_objs.sort(key=lambda obj: obj.name)

    for index, obj in enumerate(selected_objs):
        num = get_number(obj.name)

        binary_str = to_8bit_binary(num)
        permuted_num = permute_bits(num)
        permuted_str = to_8bit_binary(permuted_num)

        obj.name = f"{permuted_str}_{binary_str}"

        obj.show_name = True

        # ✅ FILTER
        first_part = obj.name.split('_')[0]

        if first_part in unique_binary:
            dup = obj.copy()
            dup.data = obj.data.copy()
            dup.name = first_part
            new_col.objects.link(dup)
            
    # --- Arrange duplicates in X axis ---
    gap = 2.0

    objs = list(new_col.objects)
    objs.sort(key=lambda o: o.name)  # optional: keep order clean

    for i, obj in enumerate(objs):
        obj.location.x = i * gap
        obj.location.y = 0
        obj.location.z = 0

def rev_name(suffix):
    for obj in bpy.context.selected_objects:
        original_name = obj.name
        
        # Take first 8 characters
        substr = original_name[:8]
        
        # Reverse the substring
        reversed_substr = substr[::-1]
        
        # Assign new name
        obj.name = reversed_substr + suffix
    
rev_name('_circle')
