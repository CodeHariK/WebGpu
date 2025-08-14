import bpy


class CustomMenu(bpy.types.Menu):
    """Demo custom menu"""

    # example menu from Templates->Python->ui_menu.py
    bl_label = "Custom Menu"
    bl_idname = "OBJECT_MT_custom_menu"

    def draw(self, context):
        layout = self.layout

        layout.operator("wm.open_mainfile")
        layout.operator("wm.save_as_mainfile").copy = True

        layout.operator("object.shade_smooth")

        layout.label(text="Hello world!", icon="WORLD_DATA")

        # use an operator enum property to populate a sub-menu
        layout.operator_menu_enum(
            "object.select_by_type",
            property="type",
            text="Select All by Type...",
        )

        # call another menu
        layout.operator("wm.call_menu", text="Unwrap").name = "VIEW3D_MT_uv_map"


class LayoutPanel(bpy.types.Panel):
    bl_label = "Demo layout"
    bl_space_type = "VIEW_3D"
    bl_region_type = "TOOLS"
    bl_region_type = "UI"
    bl_category = "Celebi"
    bl_context = "objectmode"

    def draw(self, context):
        layout = self.layout
        self.layout.operator(
            "object.mode_set", text="Edit", icon="EDITMODE_HLT"
        ).mode = "EDIT"

        layout.label(text="Some Operators")
        row = layout.row(align=True)
        row.alignment = "EXPAND"
        row.operator(
            "object.mode_set", text="Button1", icon="OBJECT_DATAMODE"
        ).mode = "OBJECT"
        row.operator(
            "object.mode_set", text="Button2", icon="SCULPTMODE_HLT"
        ).mode = "SCULPT"
        row.operator(
            "object.mode_set", text="Button3", icon="TPAINT_HLT"
        ).mode = "TEXTURE_PAINT"

        row = layout.row()
        box = row.box()

        row = box.row()
        row.label(text="Some menus", icon="LINENUMBERS_ON")

        row = box.row()
        # add the custom menu defined above
        box.menu(CustomMenu.bl_idname, text="My custom menu", icon="CANCEL")

        row = box.row()
        # add a standard blender menu - the add menu
        box.menu("RENDER_MT_framerate_presets", text="Add", icon="EDITMODE_HLT")

        row = box.row()
        # add an enum property menu
        # this allows only certain values to be set for a property
        box.prop_menu_enum(context.scene, "test_enum", text="enum property", icon="NLA")


enum_menu_items = [
    ("OPT1", "Option 1", "", 1),
    ("OPT2", "Option 2", "", 2),
    ("OPT3", "Option 3", "", 3),
    ("OPT4", "Option 4", "", 4),
]


def register():
    bpy.types.Scene.test_enum = bpy.props.EnumProperty(items=enum_menu_items)
    bpy.utils.register_class(CustomMenu)
    bpy.utils.register_class(LayoutPanel)


def unregister():
    bpy.utils.unregister_class(LayoutPanel)
    bpy.utils.unregister_class(CustomMenu)
    del bpy.types.Scene.test_enum
