extends Node3D

# FolioRainLines verification. `make run_folio_rain`.
# --mode=rain|snow forces the weather so the field shows; --shot=<name> saves a
# burst of frames and quits.
func _ready() -> void:
	Engine.max_fps = 60
	for i in range(10):
		await get_tree().process_frame
	var g := get_node_or_null("Game")
	var mode := "rain"
	var shot := ""
	for a in OS.get_cmdline_args() + OS.get_cmdline_user_args():
		if a.begins_with("--mode="):
			mode = a.substr(7)
		elif a.begins_with("--shot="):
			shot = a.substr(7)
	if g and g.has_method("get_weather"):
		var w = g.get_weather()
		if mode == "snow":
			# Cold + wet -> snow flecks (temperature drives snow via weather).
			w.set_override({"temperature": -6.0, "rain": 0.9, "humidity": 1.0, "clouds": 1.0, "snow": 0.9, "wind": 0.5}, 1.0)
		else:
			w.set_override({"temperature": 12.0, "rain": 1.0, "humidity": 1.0, "clouds": 1.0, "snow": 0.0, "wind": 0.6}, 1.0)
	if shot == "":
		return
	for n in range(4):
		await get_tree().create_timer(0.4).timeout
		var img := get_viewport().get_texture().get_image()
		img.save_png("res://../docs/png/%s_%d.png" % [shot, n])
	get_tree().quit()
