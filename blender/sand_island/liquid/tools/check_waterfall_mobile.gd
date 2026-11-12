extends SceneTree
## Checks the mobile sheet against the full one (make waterfall-bake runs it after baking): the
## sheet with waterfall_simple.gdshader beside it with mobile/waterfall_sheet_mobile.gdshader, the
## same moment, saved as shots/waterfall_mobile_check.png, and how many pixels differ.

const DIR := "res://waterfall_simple/"
const SIZE := Vector2i(1280, 720)


func _initialize() -> void:
	var scene: Node = load(DIR + "waterfall_sheet.glb").instantiate()
	var sheet: Mesh = (scene.find_children("*", "MeshInstance3D", true, false)[0] as MeshInstance3D).mesh
	scene.free()
	var full := _view(sheet, "waterfall_simple.gdshader")
	var mobile := _view(sheet, "mobile/waterfall_sheet_mobile.gdshader")
	for i in 10:
		await process_frame
	await RenderingServer.frame_post_draw
	var a := full.get_texture().get_image()
	var b := mobile.get_texture().get_image()
	var differ := 0
	var counted := 0
	for y in range(0, SIZE.y, 2):
		for x in range(0, SIZE.x, 2):
			var ca := a.get_pixel(x, y)
			var cb := b.get_pixel(x, y)
			counted += 1
			if absf(ca.r - cb.r) + absf(ca.g - cb.g) + absf(ca.b - cb.b) > 0.1:
				differ += 1
	var both := Image.create(SIZE.x * 2, SIZE.y, false, a.get_format())
	both.blit_rect(a, Rect2i(Vector2i.ZERO, SIZE), Vector2i.ZERO)
	both.blit_rect(b, Rect2i(Vector2i.ZERO, SIZE), Vector2i(SIZE.x, 0))
	DirAccess.make_dir_recursive_absolute(ProjectSettings.globalize_path("res://shots"))
	both.save_png(ProjectSettings.globalize_path("res://shots/waterfall_mobile_check.png"))
	print("full vs mobile sheet: %.1f %% of pixels differ (shots/waterfall_mobile_check.png)" % (100.0 * differ / counted))
	quit()


func _view(p_mesh: Mesh, p_shader: String) -> SubViewport:
	var viewport := SubViewport.new()
	viewport.size = SIZE
	viewport.own_world_3d = true
	viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	root.add_child(viewport)
	var camera := Camera3D.new()
	viewport.add_child(camera)
	camera.position = Vector3(0, 6.7, 15)
	var node := MeshInstance3D.new()
	node.mesh = p_mesh
	var material := ShaderMaterial.new()
	material.shader = load(DIR + p_shader)
	material.set_shader_parameter("still_a", load(DIR + "mobile/sheet_still_a.png"))
	material.set_shader_parameter("still_b", load(DIR + "mobile/sheet_still_b.png"))
	node.material_override = material
	viewport.add_child(node)
	return viewport
