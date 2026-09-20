extends Node3D

# FolioUI verification. `make run_folio_ui` (ARGS="--menu --shot=name").
func _ready() -> void:
	Engine.max_fps = 60
	for i in range(10):
		await get_tree().process_frame
	var g := get_node_or_null("Game")
	var shot := ""
	var open_menu := false
	for a in OS.get_cmdline_args() + OS.get_cmdline_user_args():
		if a.begins_with("--shot="):
			shot = a.substr(7)
		elif a == "--menu":
			open_menu = true
	if g and g.has_method("get_ui"):
		var ui = g.get_ui()
		if open_menu and ui.get_menu():
			ui.get_menu().open()
			print("[ui] menu state after open()=", ui.get_menu().get_state())
			# quality toggle demo
			var q = g.get_quality()
			print("[ui] quality before=", q.get_level())
	if shot != "":
		await get_tree().create_timer(0.5).timeout
		var img := get_viewport().get_texture().get_image()
		img.save_png("res://../docs/png/%s.png" % shot)
		get_tree().quit()
