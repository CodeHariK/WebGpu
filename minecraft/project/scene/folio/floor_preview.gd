extends Node3D

# Folio floor/island preview. `make run_folio_floor`. `--phase=<0..1>` locks a
# day phase; `--shot=<name>` saves a screenshot and quits.
func _ready() -> void:
	await get_tree().process_frame
	if has_node("Cam"):
		$Cam.current = true
		$Cam.look_at(Vector3(0, 0, 0), Vector3(0, 1, 0))
	var phase := -1.0
	var shot := ""
	for a in OS.get_cmdline_args() + OS.get_cmdline_user_args():
		if a.begins_with("--phase="):
			phase = a.substr(8).to_float()
		elif a.begins_with("--shot="):
			shot = a.substr(7)
	if OS.get_cmdline_args().has("--nofog") or OS.get_cmdline_user_args().has("--nofog"):
		var fm := get_node_or_null("Floor/FloorMesh")
		if fm and fm.material_override:
			fm.material_override.set_shader_parameter("has_fog", false)
	var g := get_node_or_null("Game")
	if g and g.has_method("get_day_cycles") and phase >= 0.0:
		g.get_day_cycles().set_progress_override(phase)
	if shot != "":
		await get_tree().create_timer(0.8).timeout
		var img := get_viewport().get_texture().get_image()
		img.save_png("res://../docs/folio_port/" + shot + ".png")
		get_tree().quit()
