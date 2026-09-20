extends Node3D

# Day-cycle preview. `make run_folio_cycles` runs the live 4-min loop.
# `--phase=<0..1>` locks a phase; `--shot=<name>` saves a screenshot and quits.
func _ready() -> void:
	await get_tree().process_frame
	if has_node("Cam"):
		$Cam.current = true
	var phase := -1.0
	var shot := ""
	for a in OS.get_cmdline_args() + OS.get_cmdline_user_args():
		if a.begins_with("--phase="):
			phase = a.substr(8).to_float()
		elif a.begins_with("--shot="):
			shot = a.substr(7)
	var g := get_node_or_null("Game")
	if g and g.has_method("get_day_cycles") and phase >= 0.0:
		g.get_day_cycles().set_progress_override(phase)
	if shot != "":
		await get_tree().create_timer(0.6).timeout
		var img := get_viewport().get_texture().get_image()
		img.save_png("res://../docs/png/" + shot + ".png")
		get_tree().quit()
