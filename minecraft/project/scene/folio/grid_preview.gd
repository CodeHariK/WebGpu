extends Node3D

# Folio grid-material preview. `make run_folio_grid`. `--shot` saves a screenshot.
func _ready() -> void:
	await get_tree().process_frame
	if has_node("Cam"):
		$Cam.current = true
	var args := OS.get_cmdline_args() + OS.get_cmdline_user_args()
	if args.has("--shot"):
		await get_tree().create_timer(0.6).timeout
		var img := get_viewport().get_texture().get_image()
		img.save_png("res://../docs/png/grid_preview.png")
		get_tree().quit()
