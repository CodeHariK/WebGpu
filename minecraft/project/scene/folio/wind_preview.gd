extends Node3D

# FolioWindLines focused preview. `make run_folio_wind`.
# Thickens the streaks and captures a burst of frames so the sweeping gust bulge
# is caught against the sky. --shot=<name> saves shots windN.png and quits.
func _ready() -> void:
	Engine.max_fps = 60
	for i in range(8):
		await get_tree().process_frame
	var wl := get_node_or_null("WindLines")
	if wl:
		wl.set_thickness(0.6) # exaggerate so the streak reads in a still
	var shot := ""
	for a in OS.get_cmdline_args() + OS.get_cmdline_user_args():
		if a.begins_with("--shot="):
			shot = a.substr(7)
	if shot == "":
		return
	# Burst: 8 frames over ~4s to catch a gust bulge sweeping.
	for n in range(8):
		await get_tree().create_timer(0.5).timeout
		var img := get_viewport().get_texture().get_image()
		img.save_png("res://../docs/png/%s_%d.png" % [shot, n])
	get_tree().quit()
