bl_info = {
    "name": "Celebi",
    "blender": (4, 5, 0),
    "category": "Object",
}

import bpy

class ToggleName(bpy.types.Operator):
    """Toggle Name"""
    bl_idname = "celebi.toggle_name"            
    bl_label = "Toggle Name"                       
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        for x in bpy.context.selected_objects:
            x.show_name = not x.show_name

        return {'FINISHED'}

def menu_func(self, context):
    self.layout.operator(ToggleName.bl_idname)

def register():
    bpy.utils.register_class(ToggleName)
    bpy.types.VIEW3D_MT_object.append(menu_func)

def unregister():
    bpy.utils.unregister_class(ToggleName)

if __name__ == "__main__":
    register()
