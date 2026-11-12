extends SceneTree
## A shot of a waterfall scene from its own camera, saved in shots/ (make waterfall-simple-shot,
## make waterfall-full-build). `-- --scene=res://... --out=name.png` picks the scene and the file;
## without them it shoots waterfall_full.tscn to shots/waterfall_full.png.

func _initialize() -> void:
	root.size = Vector2i(1280, 720)
	var path := "res://waterfall_simple/waterfall_full.tscn"
	var out := "waterfall_full.png"
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--scene="):
			path = arg.trim_prefix("--scene=")
		elif arg.begins_with("--out="):
			out = arg.trim_prefix("--out=")
	root.add_child(load(path).instantiate())
	for i in 40: # let the shaders compile and the particles start
		await process_frame
	await RenderingServer.frame_post_draw
	DirAccess.make_dir_recursive_absolute(ProjectSettings.globalize_path("res://shots"))
	root.get_texture().get_image().save_png(ProjectSettings.globalize_path("res://shots/" + out))
	print("saved shots/" + out)
	quit()
