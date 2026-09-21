extends Node3D

# FolioUI verification. `make run_folio_ui` (ARGS="--menu --shot=name").
func _ready() -> void:
	Engine.max_fps = 60
	for i in range(10):
		await get_tree().process_frame
	var g := get_node_or_null("Game")
	var shot := ""
	var open_menu := false
	var notify_toast := false
	var play := false
	var modal := false
	for a in OS.get_cmdline_args() + OS.get_cmdline_user_args():
		if a.begins_with("--shot="):
			shot = a.substr(7)
		elif a == "--menu":
			open_menu = true
		elif a == "--notify":
			notify_toast = true
		elif a == "--play":
			play = true
		elif a == "--modal":
			modal = true
	if g and g.has_method("get_ui"):
		var ui = g.get_ui()
		if play and ui.get_title():
			await get_tree().create_timer(0.4).timeout
			ui.get_title().emit_signal("started")
			ui.get_title().close()
			await get_tree().create_timer(0.5).timeout
			print("[ui] started; title state=", ui.get_title().get_state())
		if modal and ui.get_modal():
			ui.get_menu().open()
			ui.get_modal().open_confirm("Quit to title?", "You'll return to the start screen.", "Quit", "Cancel")
			await get_tree().create_timer(0.3).timeout
			print("[ui] modal state=", ui.get_modal().get_state())
		if open_menu and ui.get_menu():
			ui.get_menu().open()
			print("[ui] menu state after open()=", ui.get_menu().get_state())
			# quality toggle demo
			var q = g.get_quality()
			print("[ui] quality before=", q.get_level())
		if notify_toast and ui.get_notifications():
			ui.get_notifications().notify("Welcome to the island")
			await get_tree().create_timer(0.35).timeout
			print("[ui] toast shown")
	if shot != "":
		await get_tree().create_timer(0.5).timeout
		var img := get_viewport().get_texture().get_image()
		img.save_png("res://../docs/png/%s.png" % shot)
		get_tree().quit()
