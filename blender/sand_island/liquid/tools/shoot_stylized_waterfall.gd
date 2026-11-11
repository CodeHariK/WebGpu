extends SceneTree
## shots/waterfall_godot.png: stylized_waterfall.tscn at the exported frame (make waterfall-shot),
## to set beside shots/waterfall_blender.png (Blender's own render of that frame, also made by
## make waterfall-shot). `-- --live` lets the shaders run instead of holding the frame;
## `-- --scene=res://... --out=name.png` shoots another scene (make waterfall-simple-shot).

func _initialize() -> void:
	root.size = Vector2i(1280, 720)
	var path := "res://stylized_waterfall/stylized_waterfall.tscn"
	var out := "waterfall_godot.png"
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--scene="):
			path = arg.trim_prefix("--scene=")
		elif arg.begins_with("--out="):
			out = arg.trim_prefix("--out=")
	var scene: Node = load(path).instantiate()
	root.add_child(scene)
	if not "--live" in OS.get_cmdline_user_args():
		for node in scene.find_children("*", "MeshInstance3D", true, false):
			var material := (node as MeshInstance3D).material_override as ShaderMaterial
			if material:
				material.set_shader_parameter("time_scale", 0.0)
	for i in 40:
		await process_frame
	await RenderingServer.frame_post_draw
	DirAccess.make_dir_recursive_absolute(ProjectSettings.globalize_path("res://shots"))
	root.get_texture().get_image().save_png(ProjectSettings.globalize_path("res://shots/" + out))
	print("saved shots/" + out)
	quit()
