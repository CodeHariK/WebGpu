extends Node3D

# Folio post-processing preview: a receding grid ground (detail at the top of the
# frame) under FolioGame's Rendering (glow + cheap-DOF). `make run_folio_post`.
# `--shot` saves docs/folio_port/post_preview.png. `--nodof` disables cheap-DOF
# (set quality to low) so you can A/B the tilt-shift.
func _ready() -> void:
	await get_tree().process_frame
	if has_node("Cam"):
		$Cam.current = true
	var args := OS.get_cmdline_args() + OS.get_cmdline_user_args()
	if args.has("--nodof"):
		var g := get_node_or_null("Game")
		if g and g.has_method("get_quality"):
			g.get_quality().change_level(1)
	if args.has("--shot"):
		await get_tree().create_timer(0.7).timeout
		var img := get_viewport().get_texture().get_image()
		img.save_png("res://../docs/folio_port/post_preview.png")
		get_tree().quit()
