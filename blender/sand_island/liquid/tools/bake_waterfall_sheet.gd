extends SceneTree
## Bakes the waterfall sheet's still layers (make waterfall-bake): the sheet drawn flat over a
## 1024 x 1024 texture by its UV with waterfall_simple/mobile/sheet_bake.gdshader, twice (one set of
## three layers each), saved as waterfall_simple/mobile/sheet_still_a.png and sheet_still_b.png.
## The viewport writes its colours sRGB-encoded (as anything shown on screen); the mobile shader
## reads them back with source_color, which undoes that.

const DIR := "res://waterfall_simple/"
const SIZE := 1024


func _initialize() -> void:
	var viewport := SubViewport.new()
	viewport.size = Vector2i(SIZE, SIZE)
	viewport.own_world_3d = true
	viewport.transparent_bg = true
	viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	root.add_child(viewport)
	var camera := Camera3D.new() # the bake shader places the triangles itself; a camera must exist
	viewport.add_child(camera)
	camera.position = Vector3(0, 6, 30)
	var scene: Node = load(DIR + "waterfall_sheet.glb").instantiate()
	var sheet := MeshInstance3D.new()
	sheet.mesh = (scene.find_children("*", "MeshInstance3D", true, false)[0] as MeshInstance3D).mesh
	scene.free()
	sheet.extra_cull_margin = 16384.0 # never culled: where it is drawn has nothing to do with where it is
	var material := ShaderMaterial.new()
	material.shader = load(DIR + "mobile/sheet_bake.gdshader")
	sheet.material_override = material
	viewport.add_child(sheet)
	for bake_pass in 2:
		material.set_shader_parameter("bake_pass", bake_pass)
		for i in 8:
			await process_frame
		await RenderingServer.frame_post_draw
		var image := viewport.get_texture().get_image()
		var file := "mobile/sheet_still_%s.png" % ["a", "b"][bake_pass]
		image.save_png(ProjectSettings.globalize_path(DIR + file))
		print("saved ", DIR + file)
	quit()
