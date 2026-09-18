extends SceneTree
var scene_root
var frames := 0
func _initialize():
	scene_root = load("res://scene/folio/game.tscn").instantiate()
	get_root().add_child(scene_root)
func _process(_delta):
	frames += 1
	if frames >= 6:
		var lg = scene_root.get_node("Lighting")
		print("light_dir=", lg.get_direction())
		print("light_node=", lg.get_child_count(), " children (expect DirectionalLight3D)")
		return true
	return false
