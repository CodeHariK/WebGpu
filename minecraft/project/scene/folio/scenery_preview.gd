extends Node3D

func _ready() -> void:
	var scn: PackedScene = load("res://assets/folio/scenery/scenery.glb")
	var root := scn.instantiate()
	add_child(root)
	# Report the top-level object list + overall bounds.
	var names: Array = []
	var lo := Vector3(1e9, 1e9, 1e9)
	var hi := Vector3(-1e9, -1e9, -1e9)
	for c in root.get_children():
		names.append(c.name)
		var p: Vector3 = (c as Node3D).global_position if c is Node3D else Vector3.ZERO
		lo = Vector3(min(lo.x,p.x), min(lo.y,p.y), min(lo.z,p.z))
		hi = Vector3(max(hi.x,p.x), max(hi.y,p.y), max(hi.z,p.z))
	print("[scenery] %d top-level objects" % root.get_child_count())
	print("[scenery] bounds min=%s max=%s" % [lo, hi])
	print("[scenery] names: ", names.slice(0, 40))
	for a in OS.get_cmdline_args() + OS.get_cmdline_user_args():
		if a.begins_with("--shot="):
			await get_tree().create_timer(0.6).timeout
			var img := get_viewport().get_texture().get_image()
			img.save_png("res://../docs/png/" + a.substr(7) + ".png")
			get_tree().quit()
