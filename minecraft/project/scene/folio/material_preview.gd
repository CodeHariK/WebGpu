extends Node3D

# Folio material preview: a couple of casters on a ground plane, all using the
# folio base shader, lit by FolioLighting's sun (with cast shadows). Run it with
# `make run_folio_mat`. Pass `--shot` on the command line to save a screenshot to
# docs/folio_port/material_preview.png and quit (used for verification).
func _ready() -> void:
	await get_tree().process_frame
	if has_node("Cam"):
		$Cam.current = true
	var args := OS.get_cmdline_args() + OS.get_cmdline_user_args()
	if args.has("--shot"):
		await get_tree().create_timer(0.9).timeout
		var img := get_viewport().get_texture().get_image()
		img.save_png("res://../docs/folio_port/material_preview.png")
		get_tree().quit()
