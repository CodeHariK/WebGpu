import bpy


class ToggleName(bpy.types.Operator):
    """Toggle Name"""

    bl_idname = "celebi.toggle_name"
    bl_label = "Toggle Name"
    bl_options = {"REGISTER", "UNDO"}

    def execute(self, context):
        for x in bpy.context.selected_objects:
            x.show_name = not x.show_name

        return {"FINISHED"}


class HELP_Panel(bpy.types.Panel):
    """Creates a Panel in the Object properties window"""

    bl_label = "Toggle Name Panel"
    bl_idname = "OBJECT_PT_toggle_name"
    bl_space_type = "VIEW_3D"
    bl_region_type = "UI"
    bl_category = "Celebi"

    def draw(self, context):
        layout = self.layout
        layout.operator("celebi.toggle_name")


def register():
    bpy.utils.register_class(ToggleName)
    bpy.utils.register_class(HELP_Panel)


def unregister():
    bpy.utils.unregister_class(HELP_Panel)
    bpy.utils.unregister_class(ToggleName)
