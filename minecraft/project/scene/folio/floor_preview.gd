extends Node3D

# Folio floor/island preview. `make run_folio_floor`. `--phase=<0..1>` locks a
# day phase; `--shot=<name>` saves a screenshot and quits.
func _ready() -> void:
	# Cap the framerate so the preview doesn't run flat-out (default 60; --fps=N).
	var fps := 60
	for a in OS.get_cmdline_args() + OS.get_cmdline_user_args():
		if a.begins_with("--fps="):
			fps = int(a.substr(6))
	Engine.max_fps = fps
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
	# --weather=cold|rain|clear forces Weather so ice / rain splashes are testable.
	var weather := ""
	for a in OS.get_cmdline_args() + OS.get_cmdline_user_args():
		if a.begins_with("--weather="):
			weather = a.substr(10)
	if g and g.has_method("get_weather") and weather != "":
		var w = g.get_weather()
		if weather == "cold":
			w.set_override({"temperature": -6.0, "rain": 0.0}, 1.0)
		elif weather == "rain":
			w.set_override({"temperature": 14.0, "rain": 1.0, "humidity": 1.0, "clouds": 1.0}, 1.0)
		elif weather == "clear":
			w.clear_override()
	if shot != "":
		await get_tree().create_timer(0.8).timeout
		var img := get_viewport().get_texture().get_image()
		img.save_png("res://../docs/folio_port/" + shot + ".png")
		get_tree().quit()
