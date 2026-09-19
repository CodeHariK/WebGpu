extends Node3D

# FolioAudio verification. `make run_folio_audio`.
# Registers two positional beeps (one at the camera, one far away) and prints the
# distance-faded volumes, then checks the mute toggle. Quits automatically.
func _ready() -> void:
	Engine.max_fps = 60
	# Let FolioGame boot and FolioView solve the camera.
	for i in range(10):
		await get_tree().process_frame
	var g := get_node_or_null("Game")
	if g == null or not g.has_method("get_audio"):
		push_error("No FolioGame/Audio"); get_tree().quit(); return
	var audio = g.get_audio()
	var cam := get_viewport().get_camera_3d()
	var cam_pos: Vector3 = cam.global_position if cam else Vector3.ZERO

	var beep := "res://audio/folio/test_beep.wav"
	var near_id: int = audio.register_sound({
		"path": beep, "positions": cam_pos, "distance_fade": 20.0,
		"volume": 0.5, "loop": true,
	})
	var far_id: int = audio.register_sound({
		"path": beep, "positions": cam_pos + Vector3(1000, 0, 0), "distance_fade": 20.0,
		"volume": 0.5, "loop": true,
	})
	audio.play(near_id)
	audio.play(far_id)
	# A couple ticks so update() applies the fade.
	for i in range(5):
		await get_tree().process_frame

	print("[audio] near_vol=", audio.get_item_volume(near_id), " (expect ~0.5)")
	print("[audio] far_vol=", audio.get_item_volume(far_id), " (expect ~0.0)")

	audio.set_mute(true)
	print("[audio] master muted after set_mute(true)=", AudioServer.is_bus_mute(0))
	audio.set_mute(false)
	print("[audio] master muted after set_mute(false)=", AudioServer.is_bus_mute(0))

	if not ("--keep" in OS.get_cmdline_user_args()):
		get_tree().quit()
